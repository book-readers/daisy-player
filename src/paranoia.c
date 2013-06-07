/* paranoia.c
 *
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
extern char sound_dev[], tmp_wav[];
int sector_ptr;
int pipefd[2];
cdrom_paranoia_t *par;
cdrom_drive_t *drv;
CdIo_t *p_cdio;
lsn_t lsn_cursor;

void view_screen ();
void playfile (char *, char *, char *, char *, char *);

char *get_mcn (char *cd_dev)
{
   CdIo_t *p_cdio;

   if ((p_cdio = cdio_open (cd_dev, DRIVER_UNKNOWN)) == NULL)
      return "_";
   return cdio_get_mcn (p_cdio);
} // get_mcn                     

void init_paranoia (char *cd_dev)
{
   p_cdio = cdio_open (cd_dev, DRIVER_UNKNOWN);
   if (! (drv = cdio_cddap_identify_cdio (p_cdio, 0, NULL)))
   {
      endwin ();
      beep ();
      printf ("Unable to identify audio CD disc.\n");
      kill (getppid (), SIGKILL);
   } // if
   if (cdda_open (drv) != 0)
   {
      endwin ();
      beep ();
      printf ("Unable to open disc.\n");
      kill (getppid (), SIGKILL);
   } // if
   if (pipe (pipefd) == -1)
   {
      int e;

      e = errno;
      endwin ();
      beep ();
      printf ("pipe: %s\n", strerror (e));
      kill (getppid (), SIGKILL);
   } // if
   par = paranoia_init (drv);
   paranoia_modeset (par, PARANOIA_MODE_FULL ^ PARANOIA_MODE_NEVERSKIP);
} // init_paranoia

pid_t play_track (char *cd_dev, char *out_file, char *type,
                  lsn_t from, float speed)
{
   int ret;

   cdio_paranoia_free (par);
   init_paranoia (cd_dev);
   switch (player_pid = fork ())
   {
   case 0: /* Child reads from pipe */
   {
      char path[MAX_STR + 1], str[MAX_STR + 1];

      close (pipefd[1]);     /* don't need this in child */
#ifdef F_SETPIPE_SZ
      fcntl (pipefd[0], F_SETPIPE_SZ, 1024000);
#endif
      snprintf (path, MAX_STR, "/dev/fd/%d", pipefd[0]);
      snprintf (str, MAX_STR, "%f", speed);
      playfile (path, "cdda", out_file, type, str);
      close (pipefd[0]);
      _exit (0);
   }
   default:
      paranoia_seek (par, from, SEEK_SET);
      lsn_cursor = from;
      return player_pid;
   } // switch
   ret = ret;
} // play_track
