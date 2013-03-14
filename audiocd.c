/* audiocd.c - handle Audio-CD's

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

#include "daisy.h"
#include <dirent.h>

char tag[MAX_TAG], label[max_phrase_len];
extern char cd_dev[];
extern char daisy_title[], bookmark_title[], sound_dev[];
int current, displaying, total_items, max_y, total_time, cddb_flag;
extern daisy_t daisy[];
pid_t player_pid, daisy_player_pid;
float speed;
sox_format_t *sox_in, *sox_out;

int get_tag_or_label (xmlTextReaderPtr);

float calc_track_time (char *file)
{
   struct stat buf;

   stat (file, &buf);
   return buf.st_size / 44100 / 2 / 2;
} // calc_track_time

void get_cddb_info ()
{
   FILE *r;
   size_t len = MAX_STR;
   char *str = NULL, cd[MAX_STR + 1];
   int i;

   if (access ("/usr/bin/cddbget", F_OK) == -1)
   {
      endwin ();
      beep ();
      printf (gettext ("\nDaisy-player needs the \"cddbget\" programme.\n"));
      printf (gettext ("Please install it and try again.\n"));
      fflush (stdout);
      _exit (1);
   } // if
   snprintf (cd, MAX_STR, "cddbget -c %s -I -d", cd_dev);
   r = popen (cd, "r");
   str = malloc (len + 1);
   i = 0;
   while (1)
   {
      if (getline (&str, &len, r) == -1)
         break;
      if (strcasestr (str, "DTITLE"))
      {
         strncpy (bookmark_title, strchr (str, '=') + 1, MAX_STR - 1);
         if (strchr (bookmark_title, '\n'))
            *strchr (bookmark_title, '\n') = 0;
         if (strchr (bookmark_title, '\r'))
            *strchr (bookmark_title, '\r') = 0;
         if (strchr (bookmark_title, '/'))
            *strchr (bookmark_title, '/') = '-';
         strncpy (daisy_title, strchr (str, '=') + 1, MAX_STR - 1);
         if (strchr (daisy_title, '\n'))
            *strchr (daisy_title, '\n') = 0;
         if (strchr (daisy_title, '\r'))
            *strchr (daisy_title, '\r') = 0;
      } // if
      if (strcasestr (str, "TTITLE"))
      {
         strncpy (daisy[i].label, strchr (str, '=') + 1, max_phrase_len - 1);
         if (strchr (daisy[i].label, '\n'))
            *strchr (daisy[i].label, '\n') = 0;
         if (strchr (daisy[i].label, '\r'))
            *strchr (daisy[i].label, '\r') = 0;
         i++;
      } // if
   } // while
   fclose (r);
} // get_cddb_info

void get_toc_audiocd (char *line)
{
   char *dir;
   struct dirent **namelist;

   dir = strrchr (line, (int) ' ') + 1;
   *strchr (dir, (int) '\n') = 0;
   switch (chdir (dir))
   {
   int e;

   case -1:
      e = errno;
      endwin ();
      beep ();
      printf ("%s: %s\n", dir, strerror (e));
      fflush (stdout);
      _exit (1);
   } // switch
   current = displaying = 0;
   total_items = scandir (".", &namelist, NULL, alphasort) - 2;
   total_time = 0;
   for (current = 0; current < total_items; current++)
   {
      snprintf (daisy[current].label, 15, "Track %2d", current + 1);
      snprintf (daisy[current].filename, MAX_STR - 1,
                "%s/Track %d.wav", dir, current + 1);
      daisy[current].duration = calc_track_time (daisy[current].filename);
      total_time += daisy[current].duration;
      if (displaying == max_y)
         displaying = 1;
      daisy[current].x = 1;
      daisy[current].screen = current / max_y;
      daisy[current].y = current - daisy[current].screen * max_y;
      displaying++;
   } // for
   if (cddb_flag != 'n')
      get_cddb_info ();
} // get_toc_audiocd

void playcd (int current)
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
      return;
   } // switch
   setsid ();

   char str[MAX_STR + 1];

   snprintf (str, MAX_STR, "%f", speed);
   playfile (daisy[current].filename, str);
   _exit (0);

/***** not implemented yet

   char str[MAX_STR + 1];

   sox_globals.verbosity = 0;
   sox_format_init ();
   if (! (sox_in = sox_open_read (daisy[current].filename, NULL, NULL, NULL)))
      _exit (-1);
   while (! (sox_out = sox_open_write (sound_dev, &sox_in->signal,
          NULL, "alsa", NULL, NULL)))
   {
      strncpy (sound_dev, "hw:0", MAX_STR - 1);
      save_rc ();
      if (sox_out)
         sox_close (sox_out);
   } // while
***************************************************************************/
} // playcd
