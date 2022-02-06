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

#include "src/daisy.h"

char tag[MAX_TAG], label[max_phrase_len];
extern char cd_dev[];       
extern char daisy_title[], bookmark_title[], sound_dev[];
int current, displaying, total_items, max_y, cddb_flag;
extern daisy_t daisy[];                   
pid_t player_pid, daisy_player_pid;
float speed, total_time;
extern sox_effects_chain_t *effects_chain;
CdIo_t *p_cdio;
                
int get_tag_or_label (xmlTextReaderPtr);

void get_cddb_info ()
{
   FILE *r;
   size_t len = MAX_STR;
   char *str = NULL, cd[MAX_STR + 1];
   int i;

   snprintf (cd, MAX_STR, "cddbget -c %s -I -d 2> /dev/null", cd_dev);
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
         char l[MAX_STR + 1];

         snprintf (l, MAX_STR, "TTITLE%d=", i);
// some titles are spanned over more lines
         if (strstr (str, l) == NULL)
            continue;
         strncpy (daisy[i].label, strchr (str, '=') + 1, max_phrase_len - 1);
         if (strchr (daisy[i].label, '\n'))
            *strchr (daisy[i].label, '\n') = 0;
         if (strchr (daisy[i].label, '\r'))
            *strchr (daisy[i].label, '\r') = 0;
         daisy[i].label[64 - daisy[i].x] = 0;
         i++;
      } // if
   } // while
   fclose (r);
} // get_cddb_info

void get_toc_audiocd (char *dev)
{
   char *dir;
   CdIo_t *cd;
   track_t first_track;

   current = displaying = 0;
   dir = "";
   if ((cd = cdio_open (dev, DRIVER_UNKNOWN)) == NULL)
   {     
      endwin ();
      beep ();
      _exit (0);
   } // if
   total_items = cdio_get_num_tracks (cd);
   first_track = cdio_get_first_track_num (cd);
   for (current = 0; current < total_items; current++)
   {
      snprintf (daisy[current].label, 15, "Track %2d", current + 1);
      snprintf (daisy[current].filename, MAX_STR - 1,
                "%s/Track %d.wav", dir, current + 1);
      daisy[current].first_lsn = cdio_get_track_lsn (cd,
                                                     first_track + current);
      if (displaying == max_y)
         displaying = 1;
      daisy[current].x = 1;
      daisy[current].screen = current / max_y;
      daisy[current].y = current - daisy[current].screen * max_y;
      displaying++;
   } // for
   for (current = 0; current < total_items; current++)
   {
      daisy[current].last_lsn = daisy[current + 1].first_lsn;
      daisy[current].duration =
               (daisy[current].last_lsn - daisy[current].first_lsn) / 75;
   } // for
   daisy[--current].last_lsn = cdio_get_track_lsn (cd, CDIO_CDROM_LEADOUT_TRACK);
   daisy[current].duration =
               (daisy[current].last_lsn - daisy[current].first_lsn) / 75;
   total_time = (daisy[current].last_lsn - daisy[0].first_lsn) / 75;
   cdio_destroy (cd);
   if (cddb_flag != 'n')
      get_cddb_info ();
} // get_toc_audiocd

void set_drive_speed (int drive_speed)
{
   cdio_set_speed (p_cdio, drive_speed);
} // set_drive_speed                  
