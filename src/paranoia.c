/* paranoia.c
 *
 *  Copyright (C)2017 J. Lemmens
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

#include "daisy.h"

char *get_mcn (misc_t *misc)
{
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
   {
      snprintf (misc->mcn, MAX_STR, ".MCN_%s", cdio_get_mcn (misc->p_cdio));
      return misc->mcn;
   } // if
   return "";
} // get_mcn

void init_paranoia (misc_t *misc)
{
   misc->p_cdio = cdio_open (misc->cd_dev, DRIVER_UNKNOWN);
   if (! (misc->drv = cdio_cddap_identify_cdio (misc->p_cdio, 0, NULL)))
   {
      endwin ();
      beep ();
      printf ("Unable to identify audio CD disc.\n");
      kill (getppid (), SIGKILL);
   } // if
   if (cdda_open (misc->drv) != 0)
   {
      endwin ();
      beep ();
      printf ("Unable to open disc.\n");
      kill (getppid (), SIGKILL);
   } // if
   if (pipe (misc->pipefd) == -1)
   {                     
      int e;

      e = errno;
      endwin ();
      beep ();
      printf ("pipe: %s\n", strerror (e));
      kill (getppid (), SIGKILL);
   } // if
   misc->par = paranoia_init (misc->drv);
   paranoia_modeset (misc->par, PARANOIA_MODE_FULL ^ PARANOIA_MODE_NEVERSKIP);
} // init_paranoia

pid_t play_track (misc_t *misc, char *out_file, char *type,
                  lsn_t from)
{      
   init_paranoia (misc);
   switch (misc->cdda_pid = fork ())
   {
   case 0: /* Child reads from pipe */
   {
      char path[MAX_STR + 1];

      close (misc->pipefd[1]);     /* don't need this in child */
#ifdef F_SETPIPE_SZ
      fcntl (misc->pipefd[0], F_SETPIPE_SZ, 1024000);
#endif
      snprintf (path, MAX_STR, "/dev/fd/%d", misc->pipefd[0]);
      snprintf (misc->str, MAX_STR, "%f", misc->speed);
      playfile (path, "cdda", out_file, type, misc->str);
      close (misc->pipefd[0]);
      _exit (0);
   }
   default:
      paranoia_seek (misc->par, from, SEEK_SET);
      misc->lsn_cursor = from;
      return misc->cdda_pid;
   } // switch
} // play_track
