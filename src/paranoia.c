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
sox_format_t *sox_in, *sox_out;
extern char sound_dev[];

void view_screen ();

void playcdda (char *in_file, char *out_file, char *type, char *tempo)
{
   sox_effects_chain_t *effects_chain;
   sox_effect_t *e;
   char *args[MAX_STR], str[MAX_STR];

   sox_globals.verbosity = 0;
   sox_globals.stdout_in_use_by = NULL;
   sox_init ();
   sox_in = sox_open_read (in_file, NULL, NULL, "cdda");
   if (sox_in == NULL)
   {
      endwin ();
      printf ("sox_open_read\n");
      beep ();
      kill (getppid (), SIGKILL);
   } // if
   while ((sox_out = sox_open_write (out_file, &sox_in->signal,
                         NULL, type, NULL, NULL)) == NULL)
   {
      strncpy (sound_dev, "hw:0", MAX_STR - 1);
      save_rc ();
      if (sox_out)
         sox_close (sox_out);
   } // while
   sox_in->encoding.encoding = SOX_ENCODING_SIGN2;
   sox_in->encoding.bits_per_sample = 16;
   sox_in->encoding.reverse_bytes = sox_option_no;
   effects_chain = sox_create_effects_chain
                 (&sox_in->encoding, &sox_out->encoding);

   e = sox_create_effect (sox_find_effect ("input"));
   args[0] = (char *) sox_in, sox_effect_options (e, 1, args);
   sox_add_effect (effects_chain, e, &sox_in->signal, &sox_in->signal);

   e = sox_create_effect (sox_find_effect ("tempo"));
   args[0] = tempo, sox_effect_options (e, 1, args);
   sox_add_effect (effects_chain, e, &sox_in->signal, &sox_in->signal);

   snprintf (str, MAX_STR - 1, "%lf", sox_out->signal.rate);
   e = sox_create_effect (sox_find_effect ("rate"));
   args[0] = str, sox_effect_options (e, 1, args);
   sox_add_effect (effects_chain, e, &sox_in->signal, &sox_in->signal);

   snprintf (str, MAX_STR - 1, "%i", sox_out->signal.channels);
   e = sox_create_effect (sox_find_effect ("channels"));
   args[0] = str, sox_effect_options (e, 1, args);
   sox_add_effect (effects_chain, e, &sox_in->signal, &sox_in->signal);

   e = sox_create_effect (sox_find_effect ("output"));
   args[0] = (char *) sox_out, sox_effect_options (e, 1, args);
   sox_add_effect (effects_chain, e, &sox_in->signal, &sox_out->signal);

   sox_flow_effects (effects_chain, NULL, NULL);
   sox_delete_effects_chain (effects_chain);
   sox_close (sox_out);
   sox_close (sox_in);
   sox_quit ();
} // playcdda

char *get_mcn (char *cd_dev)
{
   CdIo_t *p_cdio;

   p_cdio = cdio_open (cd_dev, DRIVER_UNKNOWN);
   return cdio_get_mcn (p_cdio);
} // get_mcn

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
      kill (getppid (), SIGKILL);
   } // if
   if (cdda_open (drv) != 0)
   {
      endwin ();
      beep ();
      printf ("Unable to open disc.\n");
      kill (getppid (), SIGKILL);
   } // if

   cdrom_paranoia_t *par;
   lsn_t cursor;;

   par = paranoia_init (drv);
   paranoia_modeset (par, PARANOIA_MODE_FULL^PARANOIA_MODE_NEVERSKIP);
   paranoia_seek (par, first_lsn, SEEK_SET);
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

pid_t play_track (char *cd_dev, char *out_file, char *type,
                  lsn_t from, lsn_t to, float speed)
{
   switch (player_pid = fork ())
   {
   case -1:
      endwin ();
      beep ();
      puts ("fork()");
      fflush (stdout);
      kill (getppid (), SIGKILL);
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
      playcdda (path, out_file, type, str);
      close (pipefd[0]);
      _exit (0);
   }
   } // switch
   ret = ret;
} // play_track
