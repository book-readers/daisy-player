/* audiocd.c - handle Audio-CD's
 *
 *  Copyright (C)2015 J. Lemmens
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

void get_cddb_info (misc_t *misc, daisy_t *daisy)
{
   FILE *r;
   size_t len = MAX_STR;
   char *str = NULL, cd[MAX_STR + 1];
   int i;

   snprintf (cd, MAX_STR, "cddbget -c %s -I -d 2> /dev/null", misc->cd_dev);
   r = popen (cd, "r");
   str = malloc (len + 1);
   i = 0;
   while (1)
   {
      if (getline (&str, &len, r) == -1)
         break;
      if (strcasestr (str, "DTITLE"))
      {
         strncpy (misc->bookmark_title, strchr (str, '=') + 1, MAX_STR - 1);
         if (strchr (misc->bookmark_title, '\n'))
            *strchr (misc->bookmark_title, '\n') = 0;
         if (strchr (misc->bookmark_title, '\r'))
            *strchr (misc->bookmark_title, '\r') = 0;
         if (strchr (misc->bookmark_title, '/'))
            *strchr (misc->bookmark_title, '/') = '-';
         strncpy (misc->daisy_title, strchr (str, '=') + 1, MAX_STR - 1);
         if (strchr (misc->daisy_title, '\n'))
            *strchr (misc->daisy_title, '\n') = 0;
         if (strchr (misc->daisy_title, '\r'))
            *strchr (misc->daisy_title, '\r') = 0;
      } // if
      if (strcasestr (str, "TTITLE"))
      {
         char l[MAX_STR + 1];

         snprintf (l, MAX_STR, "TTITLE%d=", i);
// some titles are spanned over more lines
         if (strstr (str, l) == NULL)
            continue;
         strncpy (daisy[i].label, strchr (str, '=') + 1, 80);
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

daisy_t *get_number_of_tracks (misc_t *misc)
{
   CdIo_t *cd;

   misc->current = misc->displaying = 0;
   if ((cd = cdio_open (misc->cd_dev, DRIVER_UNKNOWN)) == NULL)
   {
      endwin ();
      beep ();
      _exit (0);
   } // if
   misc->total_items = cdio_get_num_tracks (cd);
   return  (daisy_t *) malloc (misc->total_items * sizeof (daisy_t));
} // get_number_of_tracks

void get_toc_audiocd (misc_t *misc, daisy_t *daisy)
{
   char *dir;
   CdIo_t *cd;
   track_t first_track;

   misc->current = misc->displaying = 0;
   dir = "";
   if ((cd = cdio_open (misc->cd_dev, DRIVER_UNKNOWN)) == NULL)
   {
      endwin ();
      beep ();
      _exit (0);
   } // if
   misc->total_items = cdio_get_num_tracks (cd);
   first_track = cdio_get_first_track_num (cd);
   for (misc->current = 0; misc->current < misc->total_items; misc->current++)
   {
      snprintf (daisy[misc->current].label, 15, "Track %2d", misc->current + 1);
      snprintf (daisy[misc->current].filename, MAX_STR - 1,
                "%s/Track %d.wav", dir, misc->current + 1);
      daisy[misc->current].first_lsn = cdio_get_track_lsn (cd,
                                                     first_track + misc->current);
      if (misc->displaying == misc->max_y)
         misc->displaying = 1;
      daisy[misc->current].x = 1;
      daisy[misc->current].screen = misc->current / misc->max_y;
      daisy[misc->current].y =
                      misc->current - daisy[misc->current].screen * misc->max_y;
      misc->displaying++;
   } // for
   for (misc->current = 0; misc->current < misc->total_items; misc->current++)
   {
      daisy[misc->current].last_lsn = daisy[misc->current + 1].first_lsn;
      daisy[misc->current].duration =
               (daisy[misc->current].last_lsn - daisy[misc->current].first_lsn) / 75;
   } // for
   daisy[--misc->current].last_lsn = cdio_get_track_lsn (cd, CDIO_CDROM_LEADOUT_TRACK);
   daisy[misc->current].duration =
               (daisy[misc->current].last_lsn - daisy[misc->current].first_lsn) / 75;
   misc->total_time = (daisy[misc->current].last_lsn - daisy[0].first_lsn) / 75;
   cdio_destroy (cd);
   if (misc->cddb_flag != 'n')
      get_cddb_info (misc, daisy);
} // get_toc_audiocd
