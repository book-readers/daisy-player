/*
 *  Copyright (C) 2013 J. Lemmens
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "src/daisy.h"

pid_t player_pid;
extern daisy_t daisy[];

static void put_num (long int num, int fd, int bytes)
{
  unsigned int i;
  unsigned char c;
  int ret;

  for (i=0; bytes--; i++)
  {
    c = (num >> (i<<3)) & 0xff;
    ret = write (fd, &c, 1);
  } // if
  ret = ret;
} // put_num

#define writestr(fd, s) \
  write(fd, s, sizeof(s) - 1)  /* Subtract 1 for trailing '\0'. */

void write_WAV_header (int fd, int32_t i_bytecount)
{
   int ret;

  /* quick and dirty */
  ret = writestr(fd, "RIFF");     /*  0-3 */
  put_num(i_bytecount+44-8, fd, 4);     /*  4-7 */
  ret = writestr(fd, "WAVEfmt "); /*  8-15 */
  put_num(16, fd, 4);                   /* 16-19 */
  put_num(1, fd, 2);                    /* 20-21 */
  put_num(2, fd, 2);                    /* 22-23 */
  put_num(44100, fd, 4);                /* 24-27 */
  put_num(44100 * 2 * 2, fd, 4);            /* 28-31 */
  put_num(4, fd, 2);                    /* 32-33 */
  put_num(16, fd, 2);                   /* 34-35 */
  ret = writestr(fd, "data");     /* 36-39 */
  put_num(i_bytecount, fd, 4);          /* 40-43 */
  ret = ret;
} // write_WAV_header

void paranoia (char *cd_dev, lsn_t first_lsn, lsn_t last_lsn, int fd)
{
   cdrom_drive_t *drv = NULL;
   CdIo_t *p_cdio;
   int ret;

   p_cdio = cdio_open (cd_dev, DRIVER_UNKNOWN);
   drv = cdio_cddap_identify_cdio (p_cdio, 0, NULL);
   if (! drv)
   {
      endwin ();
      beep ();
      printf ("Unable to identify audio CD disc.\n");
      _exit (0);
   } // if
   if (cdda_open (drv) != 0)
   {
      endwin ();
      beep ();
      printf ("Unable to open disc.\n");
      _exit (0);
   } // if

   cdrom_paranoia_t *par;
   lsn_t cursor;;

   par = paranoia_init (drv);
   paranoia_modeset (par, PARANOIA_MODE_FULL^PARANOIA_MODE_NEVERSKIP);
   paranoia_seek (par, first_lsn, SEEK_SET);
   write_WAV_header (fd, (last_lsn - first_lsn + 1) * CDIO_CD_FRAMESIZE_RAW);
   for (cursor = first_lsn; cursor <= last_lsn; cursor++)
   {
      int16_t *p_readbuf;

      p_readbuf = paranoia_read (par, NULL);
      if (! p_readbuf)
         break;
      ret = write (fd, p_readbuf, CDIO_CD_FRAMESIZE_RAW);
   } // for
   paranoia_free (par);
   cdda_close (drv);
   ret = ret;
} // paranoia

pid_t play_track (char *cd_dev, lsn_t from, lsn_t to, float speed)
{
   switch (player_pid = fork ())
   {
   case -1:
      endwin ();
      beep ();
      puts ("fork()");
      fflush (stdout);
      _exit (1);
   case 0: // child
      break;
   default: // parent
      return player_pid;
   } // switch

   int pipefd[2], ret;

   ret = pipe (pipefd);
   switch (fork ())
   {
   case 0: /* Child reads from pipe */
      close(pipefd[0]);          // Close unused read end
      paranoia (cd_dev, from, to, pipefd[1]);
      close(pipefd[1]);          // Reader will see EOF
      _exit (0);
   default:
   {
      char path[MAX_STR + 1], str[MAX_STR + 1];

      close (pipefd[1]);     /* don't need this in child */
      snprintf (path, MAX_STR, "/dev/fd/%d", pipefd[0]);
      snprintf (str, MAX_STR, "%f", speed);
      fcntl (pipefd[0], F_SETPIPE_SZ, 1024000);
      playfile (path, str);
      close (pipefd[0]);
      _exit (0);
   }
   } // switch
   ret = ret;
} // play_track
