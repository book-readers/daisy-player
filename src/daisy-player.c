/* daisy-player
 *
 * Copyright (C)2003-2021 J. Lemmens
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

void put_bookmark (misc_t *misc)
{
   xmlTextWriterPtr writer;
   struct passwd *pw;
   char name[MAX_CMD];

   pw = getpwuid (geteuid ());
   snprintf (misc->str, MAX_STR - 1, "%s/.daisy-player", pw->pw_dir);
   mkdir (misc->str, 0755);
   snprintf (name, MAX_CMD - 1, "%s/.daisy-player/%s%s",
             pw->pw_dir, misc->bookmark_title, get_mcn (misc));
   if (! (writer = xmlNewTextWriterFilename (name, 0)))
      return;
   xmlTextWriterStartDocument (writer, NULL, NULL, NULL);
   xmlTextWriterStartElement (writer, BAD_CAST "bookmark");
   if (misc->playing >= 0)
      xmlTextWriterWriteFormatAttribute
         (writer, BAD_CAST "item", "%d", misc->playing);
   else
      xmlTextWriterWriteFormatAttribute
         (writer, BAD_CAST "item", "%d", misc->current);
   if (misc->playing == -1 && misc->player_pid == -2)
      xmlTextWriterWriteFormatAttribute (writer, BAD_CAST
          "elapsed_seconds", "0");
   else
      xmlTextWriterWriteFormatAttribute (writer, BAD_CAST
        "elapsed_seconds", "%ld", time (NULL) - misc->elapsed_seconds);
   xmlTextWriterWriteFormatAttribute (writer, BAD_CAST
                   "level", "%d", misc->level);
   xmlTextWriterEndElement (writer);
   xmlTextWriterEndDocument (writer);
   xmlFreeTextWriter (writer);
} // put_bookmark

void get_clips (misc_t *misc, my_attribute_t *my_attribute)
{
   char begin_str[MAX_STR], *begin, *orig_begin, *end;

   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      return;
   orig_begin = my_attribute->clip_begin;
   end = my_attribute->clip_end;
   if (! *orig_begin)
      return;
   strcpy (begin_str, orig_begin);
   begin = begin_str;
   while (! isdigit (*begin))
      begin++;
   if (strchr (begin, 's'))
      *strchr (begin, 's') = 0;
   if (! strchr (begin, ':'))
   {
      misc->clip_begin = atof (begin);
   }
   else
      misc->clip_begin = read_time (begin);

// fill end
   while (! isdigit (*end))
      end++;
   if (strchr (end, 's'))
      *strchr (end, 's') = 0;
   if (! strchr (end, ':'))
   {
      misc->clip_end = atof (end);
   }
   else
      misc->clip_end = read_time (end);
} // get_clips

float read_time (char *p)
{
   char *h, *m, *s;

   s = strrchr (p, ':') + 1;
   if (s > p)
      *(s - 1) = 0;
   if (strchr (p, ':'))
   {
      m = strrchr (p, ':') + 1;
      *(m - 1) = 0;
      h = p;
   }
   else
   {
      h = "0";
      m = p;
   } // if
   return atoi (h) * 3600 + atoi (m) * 60 + (float) atof (s);
} // read_time

void get_bookmark (misc_t *misc, my_attribute_t *my_attribute)
{
   xmlTextReaderPtr local_reader;
   htmlDocPtr local_doc;
   struct passwd *pw;
   char *name;
   int len;

   if (misc->ignore_bookmark == 1)
      return;
   pw = getpwuid (geteuid ());
   if (! *misc->bookmark_title)
      return;
   len = strlen (pw->pw_dir) + strlen (misc->bookmark_title) +
         strlen (get_mcn (misc)) + 100;
   name = malloc (len);
   snprintf (name, len, "%s/.daisy-player/%s%s",
             pw->pw_dir, misc->bookmark_title, get_mcn (misc));
   local_doc = htmlParseFile (name, "UTF-8");
   free (name);
   if (! (local_reader = xmlReaderWalker (local_doc)))
   {
      xmlFreeDoc (local_doc);
      return;
   } // if
   do
   {
      if (! get_tag_or_label (misc, my_attribute, local_reader))
         break;
   } while (strcasecmp (misc->tag, "bookmark") != 0);
   xmlTextReaderClose (local_reader);
   xmlFreeDoc (local_doc);
   if (misc->current < 0)
   {
      misc->current = 0;
      return;
   } // if

   if (misc->current >= misc->total_items)
   {
      misc->current = 0;
      return;
   } // if
   misc->displaying = misc->playing = misc->current;
} // get_bookmark

void get_next_audio_tag (misc_t *misc, my_attribute_t *my_attribute,
                    daisy_t *daisy)
{
   int eof;

   while (1)
   {
      eof = 1 - get_tag_or_label (misc, my_attribute, misc->reader);
      if (strcasecmp (misc->tag, "audio") == 0)
      {
         misc->current_audio_file = realloc
                  (misc->current_audio_file,
                   strlen (misc->daisy_mp) + strlen (my_attribute->src) + 5);
         get_realpath_name (misc->daisy_mp, convert_URL_name (misc,
                   my_attribute->src), misc->current_audio_file);
         get_clips (misc, my_attribute);
         return;
      } // if
      if (strcasestr (misc->daisy_version, "2.02") &&
          strcasecmp (misc->tag, "text") == 0)
      {
         if (get_page_number_2 (misc, my_attribute, daisy, my_attribute->src))
            return;
      } // if
      if (strcasestr (misc->daisy_version, "3") &&
          (strcasecmp (misc->tag, "pagenum") == 0 ||
           strcasecmp (my_attribute->class, "pagenum") == 0))
      {
         if (get_page_number_3 (misc, my_attribute))
            return;
      } // if

      if ((misc->playing + 1 < misc->total_items &&
           strcasecmp (my_attribute->id,
                       daisy[misc->playing + 1].smil_anchor) == 0) ||
          eof)
      {
         if (*daisy[misc->playing + 1].smil_anchor || eof)
         {
// go to next item
            view_screen (misc, daisy);
            strncpy (my_attribute->clip_begin, "0", 2);
            misc->clip_begin = 0;
            if (++misc->playing >= misc->total_items)
            {
               struct passwd *pw;
               int len;
               char *name;

               pw = getpwuid (geteuid ());
               quit_daisy_player (misc, my_attribute, daisy);
               len = strlen (pw->pw_dir) + strlen (misc->bookmark_title) +
                     strlen (get_mcn (misc)) + 100;
               name = malloc (len);
               snprintf (name, len, "%s/.daisy-player/%s%s",
                         pw->pw_dir, misc->bookmark_title, get_mcn (misc));
               unlink (name);
               free (name);
               _exit (EXIT_SUCCESS);
            } // if
            if (daisy[misc->playing].level <= misc->level)
               misc->displaying = misc->current = misc->playing;
            if (misc->just_this_item != -1)
            {
               if (daisy[misc->playing].level <= misc->level)
               {
                  misc->playing = misc->just_this_item = -1;
                  break;
               } // if
            } // if
            if (misc->playing >= 0)
            {
               open_smil_file (misc, my_attribute,
                                daisy[misc->playing].smil_file,
                                daisy[misc->playing].smil_anchor);
               misc->elapsed_seconds = time (NULL);
               break;
            } // if
         } // if
      } // if
   } // while
   view_screen (misc, daisy);
} // get_next_audio_tag

void view_page (misc_t *misc, daisy_t *daisy)
{
   if (misc->playing == -1)
      return;
   if (daisy[misc->playing].screen != daisy[misc->current].screen)
      return;
   if (misc->total_pages == 0)
      return;
   if (! misc->update_time)
      return;
   if (misc->current_page_number)
   {
      wattron (misc->screenwin, A_BOLD);
      mvwprintw (misc->screenwin, daisy[misc->playing].y, 62, "(%3d)",
                 misc->current_page_number);
      wattroff (misc->screenwin, A_BOLD);
      wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
      wrefresh (misc->screenwin);
   } // if
} // view_page

void view_time (misc_t *misc, daisy_t *daisy)
{
   time_t elapsed_seconds, remain_seconds;

   elapsed_seconds = remain_seconds = 0;
   if (misc->playing == -1 ||
       daisy[misc->current].screen != daisy[misc->playing].screen)
      return;
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
   {
      elapsed_seconds =
       (misc->lsn_cursor - daisy[misc->playing].first_lsn) / 75;
      remain_seconds =
        (daisy[misc->playing].last_lsn - misc->lsn_cursor) / 75;
   }
   else
   {
      elapsed_seconds = (float) (time (NULL) - misc->elapsed_seconds);
      remain_seconds = daisy[misc->playing].duration - elapsed_seconds;
   } // if
   elapsed_seconds /= misc->speed;
   wattron (misc->screenwin, A_BOLD);
   mvwprintw (misc->screenwin, daisy[misc->playing].y, 68, "%02d:%02d",
              (int) ((elapsed_seconds / 60) + 0.5),
              (int) (((int) elapsed_seconds % 60) + 0.5));
   remain_seconds /= misc->speed;
   mvwprintw (misc->screenwin, daisy[misc->playing].y, 74, "%02d:%02d",
              (int) ((remain_seconds / 60) + 0.5),
              (int) (((int) remain_seconds % 60) + 0.5));
   wattroff (misc->screenwin, A_BOLD);
   wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
   wrefresh (misc->screenwin);
} // view_time

void view_duration (misc_t *misc, daisy_t *daisy, int nth)
{
// added by Colomban Wendling <cwendling@hypra.fr>
   if (daisy[nth].screen == daisy[misc->current].screen &&
       (daisy[nth].level <= misc->level || misc->playing == nth))
   {
      int x = nth, dur = 0;

      do
      {
         dur += daisy[x].duration;
         if (++x >= misc->total_items)
            break;
      } while (misc->playing != nth && daisy[x].level > misc->level);
      dur /= misc->speed;
      if (misc->playing == nth)
      {
         mvwprintw (misc->screenwin, daisy[misc->playing].y, 68, "     ");
         wattron (misc->screenwin, A_BOLD);
      } // if
      mvwprintw (misc->screenwin, daisy[nth].y, 74, "%02d:%02d",
                 (int) (dur + .5) / 60, (int) (dur + .5) % 60);
      if (misc->playing == nth)
      {
         wattroff (misc->screenwin, A_BOLD);
         wmove (misc->screenwin, daisy[misc->current].y,
                daisy[misc->current].x);
      } // if
   } // if
} // view_duration

void view_screen (misc_t *misc, daisy_t *daisy)
{
   int i, x, x2,  hours, minutes, seconds;
   float time;

   mvwprintw (misc->titlewin, 1, 0,
              "----------------------------------------");
   wprintw (misc->titlewin, "----------------------------------------");
   mvwprintw (misc->titlewin, 1, 0, gettext ("'h' for help -"));
   if (misc->total_pages)
   {
      wprintw (misc->titlewin, " ");
      wprintw (misc->titlewin, gettext ("%d pages"),
               misc->total_pages);
      wprintw (misc->titlewin, " ");
   } // if
   mvwprintw (misc->titlewin, 1, 29, " ");
   wprintw (misc->titlewin, gettext ("level: %d of %d"),
            misc->level, misc->depth);
   wprintw (misc->titlewin, " ");
   time = misc->total_time / misc->speed;
   hours   = time / 3600;
   minutes = (time - hours * 3600) / 60;
   seconds = time - (hours * 3600 + minutes * 60);
   mvwprintw (misc->titlewin, 1, 47, " ");
   wprintw (misc->titlewin, gettext ("total length: %02d:%02d:%02d"),
              hours, minutes,seconds);
   wprintw (misc->titlewin, " ");
   mvwprintw (misc->titlewin, 1, 74, " %d/%d ",
              daisy[misc->current].screen + 1,
              daisy[misc->total_items - 1].screen + 1);
   wrefresh (misc->titlewin);
   werase (misc->screenwin);
   for (i = 0; daisy[i].screen != daisy[misc->current].screen; i++);
   do
   {
      mvwprintw (misc->screenwin, daisy[i].y, daisy[i].x + 1,
                 "%s", daisy[i].label);
      x = strlen (daisy[i].label) + daisy[i].x;
      if (x / 2 * 2 != x)
         wprintw (misc->screenwin, " ");
      for (x2 = x; x2 < 59; x2 = x2 + 2)
         wprintw (misc->screenwin, " .");
      if (daisy[i].page_number)
          mvwprintw (misc->screenwin, daisy[i].y, 61, " (%3d)",
                    daisy[i].page_number);
      view_duration(misc, daisy, i);
      if (i >= misc->total_items - 1)
         break;
      i++;
   } while (daisy[i].screen == daisy[misc->current].screen);
   if (misc->just_this_item != -1 &&
       daisy[misc->current].screen == daisy[misc->playing].screen)
      mvwprintw (misc->screenwin, daisy[misc->displaying].y, 0, "J");
   wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
   wrefresh (misc->screenwin);
   if (misc->update_time)
   {
      view_page (misc, daisy);
      view_time (misc, daisy);
   } // if
} // view_screen

void start_playing (misc_t *misc, daisy_t *daisy, audio_info_t *sound_devices)
{
   char tempo_str[15], begin[20], duration[20];

   if (strcasecmp (misc->tag, "audio") != 0)
      return;
   if (misc->playing == -1)
      return;
   if (misc->clip_end - misc->clip_begin <= 0)
      failure (misc, "end < begin", errno);
   view_page (misc, daisy);
   switch ((misc->player_pid = fork ()))
   {
   case -1:
      failure (misc, "fork ()", errno);
      break;
   case 0: // child
   {
      reset_term_signal_handlers_after_fork ();
      lseek (misc->tmp_wav_fd, SEEK_SET, 0);
      snprintf (begin, 20, "%f", misc->clip_begin);
      snprintf (duration, 20, "%f", misc->clip_end - misc->clip_begin);
      (void) madplay (misc->current_audio_file, begin, duration, misc->tmp_wav);
      snprintf (tempo_str, 10, "%lf", misc->speed);
      playfile (misc, sound_devices, misc->tmp_wav, "wav", tempo_str);
      _exit (EXIT_SUCCESS);
   }
   default: // parent
      break;
   } // switch
} // start_playing

void open_smil_file (misc_t *misc, my_attribute_t *my_attribute,
                     char *smil_file, char *anchor)
{
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      return;
   if (! (misc->doc = htmlParseFile (smil_file, "UTF-8")))
   {
      int e;
      char str[MAX_STR];

      e = errno;
      snprintf (str, MAX_STR, "htmlParseFile (%s)", smil_file);
      failure (misc,  str, e);
   } // if
   if (! (misc->reader = xmlReaderWalker (misc->doc)))
   {
      int e;
      char str[MAX_STR];

      e = errno;
      snprintf (str, MAX_STR, "open_smil_file(): %s", smil_file);
      failure (misc, str, e);
   } // if

   if (*anchor == 0)                                    
      return;

// skip to anchor
   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, misc->reader))
         break;;
      if (strcasecmp (my_attribute->id, anchor) == 0)
         break;
   } // while
} // open_smil_file

void write_wav (misc_t *misc, my_attribute_t *my_attribute,
                daisy_t *daisy, audio_info_t *sound_devices, char *label)
{
   char *out_file, *out_cdr, *complete_cdr;
   struct passwd *pw;
   int old_playing, old_displaying, old_just_this_item;
   int old_current, org_current;
   char begin[20], duration[20];
   int w;

   pw = getpwuid (geteuid ());
   out_file = malloc (strlen (pw->pw_dir) + strlen (label) + 10);
   sprintf (out_file, "%s/%s.wav", pw->pw_dir, label);
   while (access (out_file, R_OK) == 0)
   {
      out_file = realloc (out_file, strlen (out_file) + 5);
      strcat (out_file, ".wav");
   } // while

   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
   {
      pid_t pid;
      int sp;
      int16_t *p_readbuf;

      sp = misc->speed;
      misc->speed = 1;
      pid = play_track (misc, sound_devices, daisy[misc->current].first_lsn);
      misc->speed = sp;
      do
      {
         if (! (p_readbuf = paranoia_read (misc->par, NULL)))
            break;
         write (misc->pipefd[1], p_readbuf, CDIO_CD_FRAMESIZE_RAW);
      } while (++misc->lsn_cursor <= daisy[misc->current].last_lsn);
      if (misc->par)
         paranoia_free (misc->par);
      free (out_file);
      kill (pid, SIGQUIT);
      return;
   } // if

   old_playing = misc->playing;
   old_displaying = misc->displaying;
   old_current = org_current = misc->current;
   old_just_this_item = misc->just_this_item;
   misc->just_this_item = misc->playing = misc->current;

   out_cdr = malloc (strlen (misc->tmp_dir) + 10);
   sprintf (out_cdr, "%s/out.cdr", misc->tmp_dir);
   complete_cdr = malloc (strlen (misc->tmp_dir) + 20);
   sprintf (complete_cdr, "%s/complete.cdr", misc->tmp_dir);
   w = open (complete_cdr, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
   open_smil_file (misc, my_attribute, daisy[misc->current].smil_file,
                                       daisy[misc->current].smil_anchor);
   while (1)
   {
#define BUF_SIZE 8192
      int r;
      char buffer[BUF_SIZE];
      size_t in, out;

      misc->clip_begin = misc->clip_end = -1;
      get_next_audio_tag (misc, my_attribute, daisy);
      if (misc->clip_begin == -1 || misc->clip_end == -1)
         continue;
      if (misc->current != old_current)
      {
         old_current = misc->current;
         if (daisy[misc->current].level <= misc->level)
            break;
      } // if
      snprintf (begin, 20, "%f", misc->clip_begin);
      snprintf (duration, 20, "%f", misc->clip_end - misc->clip_begin);
      if (access (misc->current_audio_file, R_OK) == -1)
      {
         int e;

         e= errno;
         endwin ();
         beep ();
         printf ("%s: %s\n", misc->current_audio_file, strerror (e));
         remove_tmp_dir (misc);
         _exit (EXIT_FAILURE);
      } // if
      (void) madplay (misc->current_audio_file, begin, duration, out_cdr);
      r = open (out_cdr, O_RDONLY);
      while ((in = read (r, &buffer, BUF_SIZE)) > 0)
      {
         out = write (w, &buffer, in);
         if (out != in)
            failure (misc, "read/write", errno);
      } // while
      close (r);
   } // while
   close (w);
   playfile (misc, sound_devices, complete_cdr, "cdr", "1");
   free (out_file);
   free (out_cdr);
   free (complete_cdr);
   misc->playing = old_playing;
   misc->displaying = old_displaying;
   misc->current = org_current;
   misc->just_this_item = old_just_this_item;
   wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
} // write_wav

void pause_resume (misc_t *misc, my_attribute_t *my_attribute,
                          daisy_t *daisy, audio_info_t *sound_devices)
{
   if (misc->playing < 0 && misc->pause_resume_playing < 0)
   {
      beep ();
      return;
   } // if
   if (misc->current_sink < 0)
      select_next_output_device (misc, daisy, sound_devices);
   if (misc->playing > -1)
   {
      misc->pause_seconds = time (NULL) - misc->elapsed_seconds;
      misc->pause_resume_playing = misc->playing;
      misc->pause_resume_id = strdup (misc->current_id);
      misc->playing = -1;
      misc->pause_resume_lsn_cursor = misc->lsn_cursor;
      kill_player (misc);
      return;
   } // if

   misc->current = misc->playing = misc->pause_resume_playing;
   misc->pause_resume_playing = -1;
   misc->lsn_cursor = misc->pause_resume_lsn_cursor;
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
   {
      misc->player_pid = play_track (misc, sound_devices,
                                     misc->pause_resume_lsn_cursor - 75 * 4);
      return;
   } // if
   
   if (misc->pause_seconds > 5)
      misc->pause_seconds -= 3;
   go_to_time (misc, daisy, my_attribute, sound_devices, misc->pause_seconds);
} // pause_resume

void store_to_disk (misc_t *misc, my_attribute_t *my_attribute,
                    daisy_t *daisy, audio_info_t *sound_devices)
{
   char *str;
   int i, current, playing;

   playing = misc->playing;
   if (playing > -1)
      pause_resume (misc, my_attribute, daisy, sound_devices);
   wclear (misc->screenwin);
   current = misc->current;
   str = malloc (strlen (daisy[current].label) + 10);
   strcpy (str, daisy[misc->current].label);
   wprintw (misc->screenwin,
            "\nStoring \"%s\" as \"%s.wav\" into your home-folder...",
            daisy[current].label, str);
   wrefresh (misc->screenwin);
   for (i = 0; str[i] != 0; i++)
      if (str[i] == '/')
         str[i] = '-';
   write_wav (misc, my_attribute, daisy, sound_devices, str);
   if (playing > -1)
      pause_resume (misc, my_attribute, daisy, sound_devices);
   free (str);
   view_screen (misc, daisy);
} // store_to_disk

void help (misc_t *misc, my_attribute_t *my_attribute, daisy_t *daisy, 
           audio_info_t *sound_devices)
{
   int y, x, playing;

   playing = misc->playing;
   if (playing > -1)
      pause_resume (misc, my_attribute, daisy, sound_devices);
   getyx (misc->screenwin, y, x);
   wclear (misc->screenwin);
   wprintw (misc->screenwin, "\n%s\n", gettext
            ("These commands are available in this version:"));
   wprintw (misc->screenwin, "========================================");
   wprintw (misc->screenwin, "========================================\n\n");
   wprintw (misc->screenwin, "%s\n", gettext
            ("cursor down,2   - move cursor to the next item"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("cursor up,8     - move cursor to the previous item"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("cursor right,6  - skip 10 seconds forwards"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("cursor left,4   - skip 10 seconds backwards"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("page-down,3     - view next page"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("page-up,9       - view previous page"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("enter           - start playing"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("space,0         - pause/resume playing"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("home,*          - play on normal speed"));
   wprintw (misc->screenwin, "\n%s", gettext
            ("Press any key for next page..."));
   nodelay (misc->screenwin, FALSE);
   wgetch (misc->screenwin);
   nodelay (misc->screenwin, TRUE);
   wclear (misc->screenwin);
   wprintw (misc->screenwin, "\n%s\n", gettext
            ("/               - search for a label"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("d               - store current item to disk"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("D,-             - decrease playing speed"));
   wprintw (misc->screenwin, "%s\n", gettext
      ("e,.             - quit daisy-player, place a bookmark and eject"));
   wprintw (misc->screenwin, "%s\n", gettext
 ("f               - find the currently playing item and place the cursor there"));
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      wprintw (misc->screenwin, "%s\n", gettext
               ("g               - go to time in this song (MM:SS)"));
   else
      wprintw (misc->screenwin, "%s\n", gettext
               ("g               - go to time in this item (MM:SS)"));
   if (misc->total_pages)
      wprintw (misc->screenwin, "%s\n", gettext
               ("G               - go to page number"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("h,?             - give this help"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("j,5             - just play current item"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("l               - switch to next level"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("L               - switch to previous level"));
   wprintw (misc->screenwin, "\n%s", gettext
            ("Press any key for next page..."));
   nodelay (misc->screenwin, FALSE);
   wgetch (misc->screenwin);
   nodelay (misc->screenwin, TRUE);
   wclear (misc->screenwin);
   wprintw (misc->screenwin, "\n%s\n", gettext
            ("m               - mute sound output on/off"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("n               - search forwards"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("N               - search backwards"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("o               - select next output sound device"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("p               - place a bookmark"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("q               - quit daisy-player and place a bookmark"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("s               - stop playing"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("T               - display the time passing during playback on/off"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("U,+             - increase playing speed"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("v,1             - decrease playback volume"));
   wprintw (misc->screenwin, "%s\n", gettext
         ("V,7             - increase playback volume (beware of Clipping)"));
   wprintw (misc->screenwin, "\n%s", gettext
            ("Press any key to leave help..."));
   nodelay (misc->screenwin, FALSE);
   wgetch (misc->screenwin);
   nodelay (misc->screenwin, TRUE);
   view_screen (misc, daisy);
   wmove (misc->screenwin, y, x);
   pause_resume (misc, my_attribute, daisy, sound_devices);
} // help

void previous_item (misc_t *misc, daisy_t *daisy)
{      
   if (misc->current == 0)
      return;
   while (daisy[misc->current].level > misc->level)
      misc->current--;
   if (misc->playing == -1)
      misc->displaying = misc->current;
   view_screen (misc, daisy);
   wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
} // previous_item

void next_item (misc_t *misc, daisy_t *daisy)
{      
   if (misc->current >= misc->total_items - 1)
   {
      beep ();
      return;
   } // if
   while (daisy[++misc->current].level > misc->level)
   {
      if (misc->current >= misc->total_items - 1)
      {
         beep ();
         previous_item (misc, daisy);
         return;
      } // if
   } // while
   view_screen (misc, daisy);
   wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
} // next_item

void calculate_times_3 (misc_t *misc, my_attribute_t *my_attribute,
                   daisy_t *daisy)
{
   int x;
   xmlTextReaderPtr parse;
   htmlDocPtr doc;

   misc->total_time = 0;
   if (misc->verbose)
   {
      printw ("\n\r\nCalculate times ");
      fflush (stdout);
   } // if
   for (x = 0; x < misc->total_items; x++)
   {
      if (misc->verbose)
      {
         printf (". ");
         fflush (stdout);
      } // if
      daisy[x].duration = 0;
      if (! *daisy[x].smil_file)
         continue;
      doc = htmlParseFile (daisy[x].smil_file, "UTF-8");
      if (! (parse = xmlReaderWalker (doc)))
      {
         endwin ();
         beep ();
         printf ("\n");
         printf (gettext ("Cannot read %s"), daisy[x].smil_file);
         printf ("\n");
         fflush (stdout);
         remove_tmp_dir (misc);
         _exit (EXIT_FAILURE);
      } // if

// parse this smil
      if (*daisy[x].smil_anchor)
      {
         while (1)
         {
            if (! get_tag_or_label (misc, my_attribute, parse))
               break;
            if (strcasecmp (daisy[x].smil_anchor, my_attribute->id) == 0)
               break;
         } // while
      } // if

      while (1)
      {
         if (! get_tag_or_label (misc, my_attribute, parse))
            break;
// get misc->clip_begin
         if (strcasecmp (misc->tag, "audio") == 0)
         {
            misc->has_audio_tag = 1;
            get_clips (misc, my_attribute);
            daisy[x].begin = misc->clip_begin;
            daisy[x].duration += misc->clip_end - misc->clip_begin;

// get clip_end
            while (1)
            {
               if (! get_tag_or_label (misc, my_attribute, parse))
                  break;
               if (x + 1 < misc->total_items)
               {
                  if (*daisy[x + 1].smil_anchor)
                  {
                     if (strcasecmp
                          (my_attribute->id, daisy[x + 1].smil_anchor) == 0)
                     {
                        break;
                     } // if
                  } // if
               } // if
               if (strcasecmp (misc->tag, "audio") == 0)
               {
                  get_clips (misc, my_attribute);
                  daisy[x].duration += misc->clip_end - misc->clip_begin;
               } // IF
            } // while
            if (x < misc->total_items - 1 && *daisy[x + 1].smil_anchor)
               if (strcasecmp
                           (my_attribute->id, daisy[x + 1].smil_anchor) == 0)
                  break;
         } // if (strcasecmp (misc->tag, "audio") == 0)
      } // while
      misc->total_time += daisy[x].duration;
      xmlTextReaderClose (parse);
      xmlFreeDoc (doc);
   } // for
   if (misc->total_time == 0)
   {
      beep ();
      quit_daisy_player (misc, my_attribute, daisy);
      printf ("%s\n", gettext (
        "This book has no audio. Play this book with \"eBook-speaker\""));
      remove_tmp_dir (misc);
      _exit (EXIT_FAILURE);
   } // if
} // calculate_times_3

void change_level (misc_t *misc, my_attribute_t *my_attribute,
                   daisy_t *daisy, char key)
{
   int c, l;

   if (misc->depth == 1)
      return;
   if (key == 'l')
      if (++misc->level > misc->depth)
         misc->level = 1;
   if (key == 'L')
      if (--misc->level < 1)
         misc->level = misc->depth;
   mvwprintw (misc->titlewin, 1, 0,
              gettext ("Please wait... -------------------------"));
   wprintw (misc->titlewin, "----------------------------------------");
   wrefresh (misc->titlewin);
   c = misc->current;
   l = misc->level;
   if (strcasestr (misc->daisy_version, "2.02"))
      fill_daisy_struct_2 (misc, my_attribute, daisy);
   if (strcasestr (misc->daisy_version, "3"))
      calculate_times_3 (misc, my_attribute, daisy);
   misc->current = c;
   misc->level = l;
   if (daisy[misc->current].level > misc->level)
      previous_item (misc, daisy);
   view_screen (misc, daisy);
   wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
} // change_level

void load_xml (misc_t *misc, my_attribute_t *my_attribute)
{
// read the preferences from $PWD/.daisy-player.xml
   char str[MAX_STR];
   struct passwd *pw;;
   xmlTextReaderPtr reader;
   htmlDocPtr doc;

   misc->elapsed_seconds = time (NULL);
   pw = getpwuid (geteuid ());
   snprintf (str, MAX_STR - 1, "%s/.daisy-player.xml", pw->pw_dir);
   doc = htmlParseFile (str, "UTF-8");
   if (! (reader = xmlReaderWalker (doc)))
      return;
   do
   {
      if (! get_tag_or_label (misc, my_attribute, reader))
         break;
      if (xmlTextReaderIsEmptyElement (reader))
         continue;
   } while (strcasecmp (misc->tag, "prefs") != 0);
   xmlTextReaderClose (reader);
   xmlFreeDoc (doc);
   if (misc->cddb_flag != 'n' && misc->cddb_flag != 'y')
      misc->cddb_flag = 'y';
} // load_xml

void save_xml (misc_t *misc)
{
   struct passwd *pw;
   char str[MAX_STR];
   xmlTextWriterPtr writer;

   pw = getpwuid (geteuid ());
   snprintf (str, MAX_STR - 1, "%s/.daisy-player.xml", pw->pw_dir);
   if (! (writer = xmlNewTextWriterFilename (str, 0)))
      failure (misc, str, errno);
   xmlTextWriterStartDocument (writer, NULL, NULL, NULL);
   xmlTextWriterStartElement (writer, BAD_CAST "prefs");
   xmlTextWriterWriteFormatAttribute
      (writer, BAD_CAST "current-sink", "%d", misc->current_sink);
   xmlTextWriterWriteFormatAttribute (writer, BAD_CAST "speed", "%lf",
                                      misc->speed);
   xmlTextWriterWriteFormatAttribute
      (writer, BAD_CAST "cd_dev", "%s", misc->cd_dev);
   xmlTextWriterWriteFormatAttribute
      (writer, BAD_CAST "cddb_flag", "%c", misc->cddb_flag);
   xmlTextWriterEndElement (writer);
   xmlTextWriterEndDocument (writer);
   xmlFreeTextWriter (writer);
} // save_xml

void quit_daisy_player (misc_t *misc, my_attribute_t *my_attribute,
                        daisy_t *daisy)
{
   kill_player (misc);
   endwin ();
   system ("reset");
   if (! misc->discinfo)
   {
      put_bookmark (misc);
      save_xml (misc);
   } // if
   if (misc->tmp_wav_fd > -1)
      close (misc->tmp_wav_fd);
   unlink (misc->tmp_wav);
   puts ("");
   remove_tmp_dir (misc);
   free_all (misc, my_attribute, daisy);
   if (misc->mounted_by_daisy_player == 0)
      return;
   snprintf (misc->cmd, MAX_CMD,
             "udisksctl unmount -b %s --force > /dev/null", misc->cd_dev);
   system (misc->cmd);
} // quit_daisy_player

void search (misc_t *misc, my_attribute_t *my_attribute, daisy_t *daisy,
             audio_info_t *sound_devices, int start, char mode)
{
   int c, found = 0, flag = 0;

   if (misc->playing > -1)
   {
      kill_player (misc);
      misc->player_pid = -2;
      misc->playing = misc->just_this_item = -1;
      view_screen (misc, daisy);
      flag = 1;
   } // if
   if (mode == '/')
   {
      misc->playing = misc->just_this_item = -1;
      mvwprintw (misc->titlewin, 1, 0, "----------------------------------------");
      wprintw (misc->titlewin, "----------------------------------------");
      mvwprintw (misc->titlewin, 1, 0, "%s ",
                 gettext ("What do you search?"));
      echo ();
      wgetnstr (misc->titlewin, misc->search_str, 25);
      noecho ();
   } // if
   if (mode == '/' || mode == 'n')
   {
      for (c = start; c < misc->total_items; c++)
      {
         if (strcasestr (daisy[c].label, misc->search_str))
         {
            found = 1;
            break;
         } // if
      } // for
      if (! found)
      {
         for (c = 0; c < start; c++)
         {
            if (strcasestr (daisy[c].label, misc->search_str))
            {
               found = 1;
               break;
            } // if
         } // for
      } // if
   }
   else
   { // mode == 'N'
      for (c = start; c >= 0; c--)
      {
         if (strcasestr (daisy[c].label, misc->search_str))
         {
            found = 1;
            break;
         } // if
      } // for
      if (! found)
      {
         for (c = misc->total_items - 1; c > start; c--)
         {
            if (strcasestr (daisy[c].label, misc->search_str))
            {
               found = 1;
               break;
            } // if
         } // for
      } // if
   } // if
   if (found)
   {
      misc->playing = misc->displaying = misc->current = c;
      misc->clip_begin = daisy[misc->current].begin;
      misc->just_this_item = -1;
      misc->current_page_number = daisy[misc->current].page_number;
      view_screen (misc, daisy);
      misc->elapsed_seconds = time (NULL);
      if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      {
         misc->player_pid = play_track (misc, sound_devices,
                                        daisy[misc->current].first_lsn);
      }
      else
      {
         go_to_time (misc, daisy, my_attribute, sound_devices, 0);
      } // if
   }
   else
   {
      beep ();
      view_screen (misc, daisy);
      if (! flag)
         return;
      misc->playing = misc->displaying;
      if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      {
         kill_player (misc);
         misc->playing = -1;
      }
      else
      {
         kill_player (misc);
      } // if
   } // if
} // search

void go_to_time (misc_t *misc, daisy_t *daisy,
                 my_attribute_t *my_attribute, audio_info_t *sound_devices, 
                 time_t flag)
{
   char time_str[10];
   int secs;
   float sub_dur, old_dur, offset;

   kill_player (misc);
   if (misc->cd_type != CDIO_DISC_MODE_CD_DA)
      misc->player_pid = -2;
   misc->playing = misc->displaying = misc->current;
   if (flag == -1)
   {
      mvwprintw (misc->titlewin, 1, 0,
                      "----------------------------------------");
      wprintw (misc->titlewin,
                      "----------------------------------------");
      *time_str = 0;
      while (1)
      {
         mvwprintw (misc->titlewin, 1, 0, "%s ",
                 gettext ("Go to time (MM:SS):"));
         echo ();
         wgetnstr (misc->titlewin, time_str, 5);
         noecho ();
         if (misc->term_signaled)
         {
            time_str[0] = 0;
            break;
         } // if
         if (strlen (time_str) == 0 || strlen (time_str) == 5)
         {
            if (strlen (time_str) == 0)
               beep ();
            break;
         }
         else
            beep ();
      } // while
      if (strlen (time_str) == 0)
         secs = 0;
      else
      {
         secs = (time_str[0] - '0') * 600 + (time_str[1] - '0')* 60 +
             (time_str[3] - '0') * 10 + (time_str[4] - '0');
      } // if
   }
   else
      secs = flag;
   if (secs >= daisy[misc->current].duration / misc->speed)
   {
      beep ();
      secs = 0;
   } // if
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
   {
      misc->player_pid = play_track (misc, sound_devices,
                         daisy[misc->current].first_lsn + (secs * 75));
      misc->elapsed_seconds = time (NULL) - secs;        
      free (misc->prev_id);      misc->prev_id = strdup (misc->current_id);
      return;
   } // if

   misc->clip_begin = 0;
   open_smil_file (misc, my_attribute, daisy[misc->current].smil_file,
                    daisy[misc->current].smil_anchor);
   free (misc->current_id);
   misc->current_id = strdup (daisy[misc->current].first_id);
   sub_dur = old_dur = 0;
   do
   {
      misc->clip_begin = misc->clip_end = -1;
      get_next_audio_tag (misc, my_attribute, daisy);
      if (misc->clip_begin == -1 && misc->clip_end == -1)
         continue;
      old_dur = sub_dur;
      sub_dur += (misc->clip_end - misc->clip_begin);
   } while (sub_dur < secs);
   offset = secs - old_dur;
   misc->clip_begin += offset;
   misc->elapsed_seconds = time (NULL) - (time_t) secs;
   view_time (misc, daisy);
   start_playing (misc, daisy, sound_devices);
   view_screen (misc, daisy);
} // go_to_time

void skip_left (misc_t *misc, my_attribute_t *my_attribute,
                daisy_t *daisy, audio_info_t *sound_devices)
{
   time_t secs;

   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      return;
   if (misc->playing < 0) // not playing
   {
      beep ();
      return;
   } // if not playing
   misc->current = misc->displaying = misc->playing;
   wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
   wrefresh (misc->screenwin);
   secs = time (NULL) - misc->elapsed_seconds;
   if (secs < 10)
   {
      if (misc->just_this_item >= 0 &&
          misc->playing > 0 &&
          daisy[misc->playing].level <= misc->level)
      {
         beep ();
         misc->just_this_item = misc->playing = -1;
         kill_player (misc);
         view_screen (misc, daisy);
         return;
      }
      else
      {
         while (1)
         {
            misc->current = misc->displaying = --misc->playing;
            if (misc->current == -1)
            {
               misc->current = 0;
               misc->playing = -1;
               kill_player (misc);
               misc->player_pid = -2;
               view_screen (misc, daisy);
               return;
            } // if
            if (daisy[misc->current].duration > 10)
               break;
         } // while
         go_to_time (misc, daisy, my_attribute, sound_devices,
                     daisy[misc->current].duration - 10);
         return;
      } // if
   } // if
   go_to_time (misc, daisy, my_attribute, sound_devices, secs - 10);
} // skip_left

void browse (misc_t *misc, my_attribute_t *my_attribute,
             daisy_t *daisy, audio_info_t *sound_devices)
{
   int old_screen, i;
   time_t next_time_clear = (time_t) -1;

   for (misc->current = 0; misc->current < misc->total_items; misc->current++)
   {
      daisy[misc->current].screen = misc->current / misc->max_y;
      daisy[misc->current].y =
                misc->current - daisy[misc->current].screen * misc->max_y;
      if (strlen (daisy[misc->current].label) + daisy[misc->current].x > 60)
         daisy[misc->current].label[60 - daisy[misc->current].x] = 0;
   } // for
   misc->current = 0;
   misc->pause_resume_playing = misc->just_this_item = -1;
   misc->label_len = 0;
   get_list_of_sound_devices (misc, sound_devices);
   if (misc->ignore_bookmark == 1)
   {
      if (misc->option_d == NULL)
         select_next_output_device (misc, daisy, sound_devices);
   }
   else
   {
      get_bookmark (misc, my_attribute);
      load_xml (misc, my_attribute);
      go_to_time (misc, daisy, my_attribute, sound_devices, misc->elapsed_seconds);
   } //if
   if (misc->option_d)
   {
      int sink;

      if (! strchr (misc->option_d, ':'))
      {
         beep ();
         endwin ();
         printf ("\n");
         usage (EXIT_FAILURE);
      } // if
      get_list_of_sound_devices (misc, sound_devices);
      for (sink = 0; sink < misc->total_sinks; sink++)
      {
         char dt[50];

         sprintf (dt, "%s:%s", sound_devices[sink].device,
                  sound_devices[sink].type);
         if (strcasecmp (misc->option_d, dt) == 0)
         {
            misc->current_sink = sink;
            break;
         } // if
      } // for
   } // if
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
   {
      for (i = 0; i < misc->total_items; i++)
      {
         daisy[i].level = 1;           
         daisy[i].page_number = 0;
      } // for
      misc->depth = 1;
      misc->player_pid = play_track (misc, sound_devices,
                                     daisy[misc->current].first_lsn + (misc->seconds * 75));
      misc->elapsed_seconds = time (NULL) - misc->seconds;
   } // if
   view_screen (misc, daisy);
   nodelay (misc->screenwin, TRUE);
   wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
   if (misc->cd_type != CDIO_DISC_MODE_CD_DA && misc->has_audio_tag == 0)
   {
      beep ();
      quit_daisy_player (misc, my_attribute, daisy);
      printf ("%s\n", gettext (
        "This book has no audio. Play this book with \"eBook-speaker\""));
      _exit (EXIT_FAILURE);
   } // if

   while (! misc->term_signaled) // forever if not signaled
   {
      switch (wgetch (misc->screenwin))
      {
      case 13: // ENTER
         misc->just_this_item = -1;
         misc->playing = misc->displaying = misc->current;
         misc->current_page_number = daisy[misc->current].page_number;
         view_screen (misc, daisy);
         free (misc->current_id);
         misc->current_id = strdup ("");
         misc->current_page_number = daisy[misc->playing].page_number;
         if (misc->player_pid > -1)
            kill_player (misc);
         misc->player_pid = -2;
         if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
         {
            misc->player_pid = play_track (misc, sound_devices,
                              daisy[misc->current].first_lsn);
            misc->elapsed_seconds = time (NULL);
            break;
         } // if

         open_smil_file (misc, my_attribute,
                daisy[misc->current].smil_file,
                daisy[misc->current].smil_anchor);
         misc->elapsed_seconds = time (NULL);
         break;
      case '/':
         if (misc->discinfo)
         {
            beep ();
            break;
         } // if
         search (misc, my_attribute, daisy, sound_devices, misc->current + 1, '/');
         break;
      case ' ':
      case KEY_IC:
      case '0':
         if (misc->discinfo)
         {
            beep ();
            break;
         } // if
         if (! misc->update_time)
         {
            if (misc->playing > -1)
               view_time (misc, daisy);
            else
               view_duration (misc, daisy, misc->displaying);
         } // if
         pause_resume (misc, my_attribute, daisy, sound_devices);
         break;
      case 'd':
         if (misc->discinfo)
         {
            beep ();
            break;
         } // if
         store_to_disk (misc, my_attribute, daisy, sound_devices);
         view_screen (misc, daisy);
         break;
      case 'e':
      case '.':
      case KEY_DC:
         if (misc->discinfo)
         {
            beep ();
            break;
         } // if
         quit_daisy_player (misc, my_attribute, daisy);
         snprintf (misc->cmd, MAX_CMD, "eject -mp %s", misc->cd_dev);
         system (misc->cmd);
         _exit (0);
      case 'f':
         if (misc->playing <= -1)
         {
            beep ();
            break;
         } // if
         misc->current = misc->playing;
         previous_item (misc, daisy);
         view_screen (misc, daisy);
         break;
      case 'g':
         if (misc->just_this_item != misc->current)
            misc->just_this_item = -1;
         go_to_time (misc, daisy, my_attribute, sound_devices, -1);
         break;
      case 'G':
         if (misc->discinfo)
         {
            beep ();
            break;
         } // if
         if (misc->total_pages)
            go_to_page_number (misc, my_attribute, daisy, sound_devices);
         else
            beep ();
         break;
      case 'h':
      case '?':
         help (misc, my_attribute, daisy, sound_devices);
         break;
      case 'j':
      case '5':
      case KEY_B2:
         if (misc->just_this_item >= 0)
            misc->just_this_item = -1;
         else
         {
            if (misc->playing >= 0)
               misc->just_this_item = misc->playing;
            else
            {
               misc->just_this_item = misc->current;
               misc->elapsed_seconds = time (NULL);
            } // if
         } // if
         if (misc->playing == -1)
         {
            misc->just_this_item = misc->displaying = misc->playing =
                                   misc->current;
            kill_player (misc);
            misc->player_pid = -2;
            if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
            {
               misc->player_pid = play_track (misc, sound_devices,
                                daisy[misc->current].first_lsn);
            }
            else
            {
               open_smil_file (misc, my_attribute,
                 daisy[misc->current].smil_file,
                 daisy[misc->current].smil_anchor);
            } // if
         } // if
         wattron (misc->screenwin, A_BOLD);
         if (misc->just_this_item != -1 &&
             daisy[misc->displaying].screen == daisy[misc->playing].screen)
         {
            mvwprintw (misc->screenwin, daisy[misc->displaying].y, 0, "J");
         }
         else
            mvwprintw (misc->screenwin, daisy[misc->displaying].y, 0, " ");
         wrefresh (misc->screenwin);
         wattroff (misc->screenwin, A_BOLD);
         misc->current_page_number = daisy[misc->playing].page_number;
         break;
      case 'l':
         if (misc->discinfo)
         {
            beep ();
            break;
         } // if
         change_level (misc, my_attribute, daisy, 'l');
         break;
      case 'L':
         if (misc->discinfo)
         {
            beep ();
            break;
         } // if
         change_level (misc, my_attribute, daisy, 'L');
         break;
      case 'm':
         if (strcmp (sound_devices[misc->current_sink].type, "alsa") == 0)
         {
            sprintf (misc->cmd,
                     "LANGUAGE=C /usr/bin/amixer -q -D %s set Master playback toggle",
                     sound_devices[misc->current_sink].device);
            system (misc->cmd);
         }
         else
         {
            if (fork () == 0)
            {
               reset_term_signal_handlers_after_fork ();
               pactl ("set-sink-mute",
                      sound_devices[misc->current_sink].device,
                      "toggle");
               _exit (EXIT_SUCCESS);
            } // if
         } // if
         break;
      case 'n':
         if (misc->discinfo)
         {
            beep ();
            break;
         } // if
         search (misc, my_attribute, daisy, sound_devices, misc->current + 1, 'n');
         break;
      case 'N':
         if (misc->discinfo)
         {
            beep ();
            break;
         } // if
         search (misc, my_attribute, daisy, sound_devices, misc->current - 1, 'N');
         break;
      case 'o':
         if (misc->playing > -1)
         {
            pause_resume (misc, my_attribute, daisy, sound_devices);
            misc->player_pid = -2;
            misc->playing = misc->just_this_item = -1;
            view_screen (misc, daisy);
         } // if
         select_next_output_device (misc, daisy, sound_devices);
         misc->playing = -1;
         pause_resume (misc, my_attribute, daisy, sound_devices);
         break;
      case 'p':
         put_bookmark (misc);
         save_xml (misc);
         break;
      case 'q':
         quit_daisy_player (misc, my_attribute, daisy);
         return;
      case 's':
         kill_player (misc);
         if (misc->cd_type != CDIO_DISC_MODE_CD_DA)
            misc->player_pid = -2;
         misc->playing = misc->just_this_item = -1;
         misc->pause_resume_playing = -1;
         view_screen (misc, daisy);
         wmove (misc->screenwin, daisy[misc->current].y,
                                 daisy[misc->current].x);
         break;
      case 'T':
         misc->update_time = 1 - misc->update_time;
         break;
      case KEY_DOWN:
      case '2':
         next_item (misc, daisy);
         break;
      case KEY_UP:
      case '8':
         if (misc->current == 0)
         {
            beep ();
            break;
         } // if
         misc->current--;
         wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
         previous_item (misc, daisy);
         break;
      case KEY_RIGHT:
      case '6':
         if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
         {
            if (misc->playing == -1)
            {
               beep ();
               break;
            } // if
            kill_player (misc);
            misc->lsn_cursor += 8 * 75;
            if (misc->lsn_cursor >= daisy[misc->playing].last_lsn)
            {
               view_screen (misc, daisy);
               if (misc->just_this_item > -1)
               {
                  beep ();
                  misc->displaying = misc->current = misc->playing;
                  misc->playing = -1;
                  break;
               }
               else
               {
                  misc->displaying = misc->current = ++misc->playing;
                  view_screen (misc, daisy);
               } // if
               if (misc->current >= misc->total_items)
               {
                  struct passwd *pw;
                  int len;
                   char *str;

                  pw = getpwuid (geteuid ());
                  quit_daisy_player (misc, my_attribute, daisy);
                  len = strlen (pw->pw_dir) +
                        strlen (misc->bookmark_title) +
                        strlen (get_mcn (misc) + 100);
                  str = malloc (len);
                  snprintf (str, len, "%s/.daisy-player/%s%s",
                            pw->pw_dir, misc->bookmark_title, get_mcn (misc));
                  unlink (str);
                  free (str);
                  _exit (EXIT_SUCCESS);
               } // if
            } // if
            misc->player_pid = play_track (misc, sound_devices,
                               misc->lsn_cursor);
            break;
         } // if
         skip_right (misc, daisy, my_attribute, sound_devices);
         if (! misc->update_time)
         {
            view_time (misc, daisy);
            next_time_clear = time (NULL) + 2;
         } // if
         break;
      case KEY_LEFT:
      case '4':
         if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
         {
            if (misc->playing == -1)
            {
               beep ();
               break;
            } // if
            kill_player (misc);
            misc->lsn_cursor -= 12 * 75;
            if (misc->lsn_cursor < daisy[misc->playing].first_lsn)
            {
               if (misc->playing > 0)
               {
                  if (misc->just_this_item > -1)
                  {
                     beep ();
                     misc->lsn_cursor = daisy[misc->playing].first_lsn;
                  }
                  else
                     misc->current = misc->displaying = --misc->playing;
               }
               else
               {
                  if (misc->lsn_cursor < daisy[misc->playing].first_lsn)
                  {
                     beep ();
                     misc->lsn_cursor = daisy[misc->playing].first_lsn;
                  } // if
               } // if
            } // if
            misc->player_pid = play_track (misc, sound_devices,
                        misc->lsn_cursor);
            break;
         } // if
         skip_left (misc, my_attribute, daisy, sound_devices);
         if (! misc->update_time)
         {
            view_time (misc, daisy);
            next_time_clear = time (NULL) + 2;
         } // if
         break;
      case KEY_NPAGE:
      case '3':
         if (daisy[misc->current].screen == daisy[misc->total_items - 1].screen)
         {
            beep ();
            break;
         } // if
         old_screen = daisy[misc->current].screen;
         while (daisy[++misc->current].screen == old_screen);
         view_screen (misc, daisy);
         wmove (misc->screenwin, daisy[misc->current].y,
                daisy[misc->current].x);
         break;
      case KEY_PPAGE:
      case '9':
         if (daisy[misc->current].screen == 0)
         {
            beep ();
            break;
         } // if
         old_screen = daisy[misc->current].screen;
         while (daisy[--misc->current].screen == old_screen);
         misc->current -= misc->max_y - 1;
         view_screen (misc, daisy);
         wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
         break;
      case ERR:
         break;
      case 'U':
      case '+':
         if (misc->speed >= 2)
         {
            beep ();
            break;
         } // if
         pause_resume (misc, my_attribute, daisy, sound_devices);
         misc->speed += 0.1;
         pause_resume (misc, my_attribute, daisy, sound_devices);
         view_screen (misc, daisy);
         break;
      case 'D':
      case '-':
         if (misc->speed <= 0.3)
         {
            beep ();
            break;
         } // if
         pause_resume (misc, my_attribute, daisy, sound_devices);
         misc->speed -= 0.1;
         pause_resume (misc, my_attribute, daisy, sound_devices);
         view_screen (misc, daisy);
         break;
      case KEY_HOME:
      case '*':
         pause_resume (misc, my_attribute, daisy, sound_devices);
         misc->speed = 1;
         pause_resume (misc, my_attribute, daisy, sound_devices);
         view_screen (misc, daisy);
         break;
      case 'v':
      case '1':
         if (strcmp (sound_devices[misc->current_sink].type, "alsa") == 0)
         {
            sprintf (misc->cmd,
                     "LANGUAGE=C /usr/bin/amixer -q -D %s set Master playback 5%%-",
                     sound_devices[misc->current_sink].device);
            system (misc->cmd);
         }
         else
         {
            if (fork () == 0)
            {
               reset_term_signal_handlers_after_fork ();
               pactl ("set-sink-volume",
                      sound_devices[misc->current_sink].device,
                      "-5%");
               _exit (EXIT_SUCCESS);
            } // if
         } // if
         break;
      case 'V':
      case '7':
         if (strcmp (sound_devices[misc->current_sink].type, "alsa") == 0)
         {
            sprintf (misc->cmd,
                     "LANGUAGE=C /usr/bin/amixer -q -D %s set Master playback 5%%+",
                     sound_devices[misc->current_sink].device);
            system (misc->cmd);
         }
         else
         {
            if (fork () == 0)
            {
               reset_term_signal_handlers_after_fork ();
               pactl ("set-sink-volume",
                      sound_devices[misc->current_sink].device,
                      "+5%");
               _exit (EXIT_SUCCESS);
            } // if
         } // if
         break;
      default:
         beep ();
         break;
      } // switch

      if (misc->playing > -1 && misc->cd_type != CDIO_DISC_MODE_CD_DA)
      {
         if (kill (misc->player_pid, 0) != 0)
         {
// if not playing
            misc->player_pid = -2;
            get_next_audio_tag (misc, my_attribute, daisy);
            start_playing (misc, daisy, sound_devices);
         } //   if
         if (misc->update_time)
            view_time (misc, daisy);
         else
         {
            if (next_time_clear == (time_t) -1 ||
                time (NULL) >= next_time_clear)
            {
               view_duration (misc, daisy, misc->playing);
               next_time_clear = (time_t) -1;
            } // if
         } // if   
      } // if
      if (misc->playing == -1 || misc->cd_type != CDIO_DISC_MODE_CD_DA)
      {
         fd_set rfds;
         struct timeval tv;

         FD_ZERO (&rfds);
         FD_SET (0, &rfds);
         tv.tv_sec = 0;
         tv.tv_usec = 100000;
// pause till a key has been pressed or 0.1 misc->elapsed_seconds are elapsed
         select (1, &rfds, NULL, NULL, &tv);
      } // if
      if (misc->playing > -1 && misc->cd_type == CDIO_DISC_MODE_CD_DA)
      {
         int16_t *p_readbuf;

         if (! (p_readbuf = paranoia_read (misc->par, NULL)))
            break;
         switch (write (misc->pipefd[1], p_readbuf, CDIO_CD_FRAMESIZE_RAW))
         {
         default:
            break;
         } // switch
         misc->lsn_cursor++;
         if (misc->lsn_cursor > daisy[misc->playing].last_lsn)
         {
            misc->current = misc->displaying = ++misc->playing;
            misc->lsn_cursor = daisy[misc->playing].first_lsn;
            if (misc->current >= misc->total_items)
            {
               struct passwd *pw;
               int len;
               char *str;

               pw = getpwuid (geteuid ());
               quit_daisy_player (misc, my_attribute, daisy);
               len = strlen (pw->pw_dir) +
                     strlen (misc->bookmark_title) +
                     strlen (get_mcn (misc)) + 100;
               str = malloc (len);
               snprintf (str, len, "%s/.daisy-player/%s%s",
                         pw->pw_dir, misc->bookmark_title, get_mcn (misc));
               unlink (str);
               free (str);
               return;
            } // if
            if (misc->just_this_item > -1)
            {
               kill_player (misc);
               misc->playing = misc->just_this_item = -1;
            } // if
            view_screen (misc, daisy);
            misc->elapsed_seconds = time (NULL);
         } // if
         view_time (misc, daisy);
      } // if
   } // while
   quit_daisy_player (misc, my_attribute, daisy);
} // browse

void usage (int ret)
{
   printf (gettext ("Daisy-player - Version %s %s"), PACKAGE_VERSION, "\n");
   puts ("(C)2003-2021 J. Lemmens\n");
   printf (gettext
    ("Usage: %s [directory_with_a_Daisy-structure] | [Daisy_book_archive]"),
    PACKAGE);
   printf ("\n%s ",
           gettext ("[-c cdrom_device] [-d audio_device:audio_type]"));
   printf ("[-h] [-i] [-T] [-n | -y] [-v]\n");
   fflush (stdout);
   _exit (ret);
} // usage

char *get_mount_point (misc_t *misc)
{
   FILE *proc;
   size_t len = 0;
   char *str = NULL;

   if (! (proc = fopen ("/proc/mounts", "r")))
      failure (misc, gettext ("Cannot read /proc/mounts."), errno);
   while (1)
   {
      str = malloc (len + 1);
      if (getline (&str, &len, proc) == -1)
         break;
      if (strcasestr (str, "iso9660") || strcasestr (str, "udf"))
         break;
   } // while
   fclose (proc);
   if (strcasestr (str, "iso9660") || strcasestr (str, "udf"))
   {
      misc->daisy_mp = strdup (strchr (str, ' ') + 1);
      *strchr (misc->daisy_mp, ' ') = 0;
      free (str);
      return misc->daisy_mp;
   } // if
   free (str);
   return NULL;
} // get_mount_point

void handle_discinfo (misc_t *misc, my_attribute_t *my_attribute,
                      daisy_t *daisy, char *discinfo_html)
{
   int i, n, total_items;
   xmlTextReaderPtr di;
   htmlDocPtr doc;
   int hours, minutes, seconds;
   float time, total_time;
   char str[MAX_STR];

   keypad (misc->screenwin, TRUE);
   meta (misc->screenwin, TRUE);
   nonl ();
   noecho ();
   wattron (misc->titlewin, A_BOLD);
   snprintf (str, MAX_STR - 1, gettext (
        "Daisy-player - Version %s - (C)2021 J. Lemmens"), PACKAGE_VERSION);
   mvwprintw (misc->titlewin, 0, 0, str);

   total_time = time = hours = minutes = seconds = 0;
   mvwprintw (misc->titlewin, 1, 0,
                "----------------------------------------");
   wprintw (misc->titlewin, "----------------------------------------");
   wrefresh (misc->titlewin);
   wclear (misc->screenwin);

   misc->discinfo = 1;
   *misc->discinfo_title = 0;
   doc = htmlParseFile (discinfo_html, "UTF-8");
   if (! (di = xmlReaderWalker (doc)))
   {
      int e;
      char str[MAX_STR];

      e = errno;
      snprintf (str, MAX_STR, gettext ("Cannot read %s"), discinfo_html);
      xmlTextReaderClose (di);
      xmlFreeDoc (doc);
      failure (misc, str, e);
   } // if (! (di = xmlReaderWalker (doc)
   i = 0;
   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, di))
         break;
      if (*misc->discinfo_title == 0 && strcasecmp (misc->tag, "title") == 0)
      {
         while (1)
         {
            if (! get_tag_or_label (misc, my_attribute, di))
               break;
            if (*misc->label)
            {
               strcpy (misc->discinfo_title, misc->label);
               mvwprintw (misc->titlewin, 0,
                       80 - strlen (misc->discinfo_title),
                       misc->discinfo_title);
               wrefresh (misc->titlewin);
               break;
            } // if
         } // while
      } // if (strcasecmp (misc->tag, "title") == 0)
      if (strcasecmp (misc->tag, "a") == 0)
      {
         htmlDocPtr doc2;
         xmlTextReaderPtr href;

         free (daisy[i].filename);
         daisy[i].filename = malloc (strlen (misc->daisy_mp) +
                             strlen (my_attribute->href) + 5);
         get_realpath_name (misc->daisy_mp, my_attribute->href,
                        daisy[i].filename);
         doc2 = htmlParseFile (daisy[i].filename, "UTF-8");
         if (! (href = xmlReaderWalker (doc2)))
         {
            int e;
            char str[MAX_STR];

            e = errno;
            snprintf (str, MAX_STR,
               gettext ("Cannot read %s"), daisy[i].filename);
            failure (misc, str, e);
         } // if

         *misc->ncc_totalTime = 0;
         while (1)
         {
            if (! get_tag_or_label (misc, my_attribute, href))
               break;
            if (*misc->ncc_totalTime)
            {
               daisy[i].duration = read_time (misc->ncc_totalTime);
               total_time += daisy[i].duration;
               break;
            } // if
         } // while
         xmlTextReaderClose (href);
         xmlFreeDoc (doc2);

         while (1)
         {
            if (! get_tag_or_label (misc, my_attribute, di))
               break;
            if (*misc->label)
            {
               strcpy (daisy[i].label, misc->label);
               break;
            } // if
         } // while

         daisy[i].level = 1;
         daisy[i].x = 0;
         daisy[i].y = misc->displaying;
         daisy[i].screen = misc->current / misc->max_y;
         i++;
      } // if (strcasecmp (misc->tag, "a") == 0)
   } // while
   xmlTextReaderClose (di);
   xmlFreeDoc (doc);
   total_items = i;
   misc->depth = 1;
   hours   = total_time / 3600;
   minutes = (total_time - hours * 3600) / 60;
   seconds = total_time - (hours * 3600 + minutes * 60);
   mvwprintw (misc->titlewin, 1, 47, " ");
   wprintw (misc->titlewin, gettext
        ("total length: %02d:%02d:%02d"), hours, minutes,seconds);
   wprintw (misc->titlewin, " ");
   wrefresh (misc->titlewin);

   wclear (misc->screenwin);
   mvwprintw (misc->screenwin, 3, 0, gettext ("Select an Audio_book:"));
   for (n = 0; n < total_items; n++)
   {
      mvwprintw (misc->screenwin, n + 5, 3, "%d %s ", n, daisy[n].label);
      time = daisy[n].duration;
      hours = time / 3600;
      time -= hours * 3600;
      minutes = time / 60;
      time -= minutes * 60;
      seconds = time;
      mvwprintw (misc->screenwin, n + 5, 70, "%02d:%02d:%02d",
                 hours, minutes, seconds);
   } // for

   wmove (misc->screenwin, 5, 2);
   wrefresh (misc->screenwin);
   nodelay (misc->screenwin, FALSE);
   n = 0;

   for (;;)
   {
      switch (wgetch (misc->screenwin))
      {
      case 13:
         misc->discinfo = 0;
         wclear (misc->titlewin);
         view_screen (misc, daisy);
         nodelay (misc->screenwin, TRUE);
         return;
      case KEY_DOWN:
         if (++n >= total_items)
            n = 0;
         wmove (misc->screenwin, n + 5, 2);
         break;
      case KEY_UP:
         if (--n < 0)
            n = total_items - 1;
         wmove (misc->screenwin, n + 5, 2);
         break;
      case 'q':
         quit_daisy_player (misc, my_attribute, daisy);
         _exit (EXIT_SUCCESS);
      default:
         beep ();
         break;
      } // switch

      fd_set rfds;
      struct timeval tv;

      FD_ZERO (&rfds);
      FD_SET (0, &rfds);
      tv.tv_sec = 0;
      tv.tv_usec = 100000;
// pause till a key has been pressed or 0.1 seconds are elapsed
      select (1, &rfds, NULL, NULL, &tv);
   } // for
} // handle_discinfo

// pointer to misc->term_signaled, whether we received a termination signal
static int *daisy_player_term_signaled;

static void term_daisy_player (int sig)
{
sig = sig;
   *daisy_player_term_signaled = 1;
} // term_daisy_player

int main (int argc, char *argv[])
{
   int opt;
   int test_mode = 0;
   char str[MAX_STR], DISCINFO_HTML[MAX_STR];
   char *c_opt, cddb_opt;
   misc_t misc;
   my_attribute_t my_attribute;
   daisy_t *daisy;
   audio_info_t *sound_devices;
   struct sigaction usr_action;

   misc.main_pid = getpid ();
   daisy = NULL;
   sound_devices = (audio_info_t *) calloc (15, sizeof (audio_info_t));
   misc.tmp_dir = misc.label = NULL;
   misc.speed = 1;
   misc.playing = misc.just_this_item = -1;
   misc.discinfo = 0;
   misc.cd_type = -1;
   misc.ignore_bookmark = 0;
   misc.current_sink = 0;
   misc.update_time = 1;
   *misc.bookmark_title = 0;
   misc.daisy_mp = malloc (10);
   *misc.daisy_mp = 0;
   misc.current_id = strdup ("");
   misc.prev_id = strdup ("");
   misc.audio_id = strdup ("");
   misc.current_audio_file = strdup ("");
   misc.pause_resume_id = strdup ("");
   *misc.search_str = 0;
   misc.total_time = 0;
   *misc.daisy_title = 0;
   *misc.ncc_html = 0;
   misc.term_signaled = 0;
   daisy_player_term_signaled = &misc.term_signaled;
   strncpy (misc.cd_dev, "/dev/sr0", MAX_STR - 1);
   my_attribute.id = strdup ("");
   my_attribute.idref = strdup ("");
   my_attribute.src = strdup ("");
   sigfillset (&usr_action.sa_mask);
   usr_action.sa_handler = player_ended;
   usr_action.sa_flags = SA_RESTART;
   sigaction (SIGCHLD, &usr_action, NULL);
   usr_action.sa_handler = term_daisy_player;
   usr_action.sa_flags = SA_RESETHAND;
   sigaction (SIGINT, &usr_action, NULL);
   sigaction (SIGTERM, &usr_action, NULL);
// we get two hangups (one from the terminal app, and one from the kernel
// closing the tty), so don't reset that one
   usr_action.sa_flags = 0;
   sigaction (SIGHUP, &usr_action, NULL);
   *misc.xmlversion = 0;
   make_tmp_dir (&misc);
   misc.cddb_flag = 'y';
   if (! setlocale (LC_ALL, ""))
      failure (&misc, "setlocale ()", errno);
   if (! setlocale (LC_NUMERIC, "C"))
      failure (&misc, "setlocale ()", errno);
   textdomain (PACKAGE);
   snprintf (str, MAX_STR, "%s/", LOCALEDIR);
   bindtextdomain (PACKAGE, str);
   opterr = 0;
   misc.verbose = 0;
   misc.use_OPF = 0;
   misc.use_NCX = 0;
   c_opt = NULL;
   cddb_opt = 0;
   misc.option_d = NULL;
   while ((opt = getopt (argc, argv, "c:d:hijnvyONTD")) != -1)
   {
      switch (opt)
      {
      case 'c':
         strncpy (misc.cd_dev, optarg, MAX_STR - 1);
         c_opt = strdup (misc.cd_dev);
         break;
      case 'd':
      {
         int sink;

         misc.option_d = strdup (optarg);
         if (! strchr (optarg, ':'))
         {
            beep ();
            endwin ();
            printf ("\n");
            usage (EXIT_FAILURE);
         } // if
         get_list_of_sound_devices (&misc, sound_devices);
         for (sink = 0; sink < misc.total_sinks; sink++)
         {
            char dt[50];

            sprintf (dt, "%s:%s", sound_devices[sink].device,
                     sound_devices[sink].type);
            if (strcasecmp (optarg, dt) == 0)
            {
               misc.current_sink = sink;
               break;
            } // if
         } // for
         break;
      }
      case 'h':
         remove_tmp_dir (&misc);
         usage (EXIT_SUCCESS);
         break;
      case 'i':
         misc.ignore_bookmark = 1;
         break;
      case 'n':
         misc.cddb_flag = 'n';
         cddb_opt = misc.cddb_flag;
         break;
      case 'T':
         misc.update_time = 0;
         break;
      case 'y':
      case 'j':
         misc.cddb_flag = 'y';
         cddb_opt = misc.cddb_flag;
         switch (system ("cddbget -c null > /dev/null 2>&1"))
// if cddbget is installed
         {
         case 0:
            break;
         default:
            misc.cddb_flag = 'n';
            cddb_opt = misc.cddb_flag;
         } // switch
         break;
      case 'v':
         misc.verbose = 1;
         break;
      case 'N':
         misc.use_NCX = 1;
         misc.use_OPF = 0;
         break;
      case 'O':
         misc.use_OPF = 1;
         misc.use_NCX = 0;
         break;
      case 'D':
         test_mode = 1;
         break;
      default:
         beep ();
         remove_tmp_dir (&misc);
         usage (EXIT_FAILURE);
      } // switch
   } // while
   if (c_opt)
      strncpy (misc.cd_dev, c_opt, MAX_STR - 1);
   if (cddb_opt)
      misc.cddb_flag = cddb_opt;
   fclose (stderr);
   system ("clear");
   printf ("(C)2003-2021 J. Lemmens\n");
   printf (gettext ("Daisy-player - Version %s %s"), PACKAGE_VERSION, "");
   printf ("\n");
   printf (gettext ("A parser to play Daisy CD's with Linux"));
   printf ("\n");
   printf (gettext ("Scanning for a Daisy CD..."));
   fflush (stdout);
   misc.total_pages = misc.mounted_by_daisy_player = 0;

   if (argv[optind])
// if there is an argument
   {
// look if arg exists
      if (access (argv[optind], R_OK) == -1)
      {
         int e;

         e = errno;
         system ("clear");
         printf (gettext ("Daisy-player - Version %s %s"),
                 PACKAGE_VERSION, "\n");
         puts ("(C)2003-2021 J. Lemmens");
         beep ();
         remove_tmp_dir (&misc);
         printf ("\n%s: %s\n", argv[optind], strerror (e));
         _exit (EXIT_FAILURE);
      } // if

// determine filetype
      magic_t myt;

      myt = magic_open (MAGIC_CONTINUE | MAGIC_SYMLINK | MAGIC_DEVICES);
      magic_load (myt, NULL);
      if (magic_file (myt, argv[optind]) == NULL)
      {
         int e;

         e = errno;
         printf ("%s: %s\n", argv[optind], strerror (e));
         beep ();
         fflush (stdout);
         remove_tmp_dir (&misc);
         usage (EXIT_FAILURE);
      } // if
      if (strcasestr (magic_file (myt, argv[optind]), "directory"))
      {
         if (*argv[optind] == '/')
         {
// absolute path
            misc.daisy_mp = strdup (argv[optind]);
         }
         else
         {
// relative path
            misc.daisy_mp = realloc (misc.daisy_mp,
               strlen (get_current_dir_name ()) + strlen (argv[optind]) + 5);
            sprintf (misc.daisy_mp, "%s/%s",
                             get_current_dir_name (), argv[optind]);
         } // if
      } // directory
      else
      if (strcasestr (magic_file (myt, argv[optind]), "Zip archive") ||
          strcasestr (magic_file (myt, argv[optind]), "tar archive") ||
          strcasestr (magic_file (myt, argv[optind]), "RAR archive data") ||
          strcasestr (magic_file (myt, argv[optind]),
                      "Microsoft Cabinet archive data") ||
          strcasestr (magic_file (myt, argv[optind]),
                      "gzip compressed data") ||
          strcasestr (magic_file (myt, argv[optind]),
                      "bzip2 compressed data") ||
          strcasestr (magic_file (myt, argv[optind]), "ISO 9660") ||
          strcasestr (magic_file (myt, argv[optind]), "EPUB document"))
      {
         snprintf (misc.cmd, MAX_CMD - 1,
                   "/usr/bin/unar -q \"%s\" -o %s",
                   argv[optind], misc.tmp_dir);
         if (system (misc.cmd) == 0x7f00)
         {
            endwin ();
            printf ("\7\n\n%s\n",
                  "Be sure the package unar is installed onto your system.");
            _exit (EXIT_FAILURE);
         } // if

         DIR *dir;
         struct dirent *dirent;
         int entries = 0;

         if (! (dir = opendir (misc.tmp_dir)))
            failure (&misc, misc.tmp_dir, errno);
         while ((dirent = readdir (dir)) != NULL)
         {
            if (strcasecmp (dirent->d_name, ".") == 0 ||
                strcasecmp (dirent->d_name, "..") == 0)
               continue;
            misc.daisy_mp = realloc (misc.daisy_mp,
                     strlen (misc.tmp_dir) + strlen (dirent->d_name) + 5);
            sprintf (misc.daisy_mp, "%s/%s", misc.tmp_dir, dirent->d_name);
            entries++;
         } // while
         if (entries > 1)
            misc.daisy_mp = strdup (misc.tmp_dir);
         closedir (dir);
      } // if unar
      else
      {
         printf ("\n%s\n", gettext ("No DAISY-CD or Audio-cd found"));
         beep ();
         remove_tmp_dir (&misc);
         usage (EXIT_FAILURE);
      } // if
      magic_close (myt);
   } // if there is an argument
   else
// try misc.cd_dev
   {
      time_t start;
      CdIo_t *cd;
      struct stat buf;

      if (access (misc.cd_dev, R_OK) == -1)
      {
         int e;

         e = errno;
         printf (gettext ("Daisy-player - Version %s %s"),
                          PACKAGE_VERSION, "\n");
         puts ("(C)2003-2021 J. Lemmens");
         beep ();
         remove_tmp_dir (&misc);
         snprintf (misc.str, MAX_STR, gettext ("Cannot read %s"),
                   misc.cd_dev);
         printf ("\n%s: %s\n", misc.str, strerror (e));
         fflush (stdout);
         _exit (EXIT_FAILURE);
      } // if
      if (stat (misc.cd_dev, &buf) == -1)
      {
         failure (&misc, misc.cd_dev, errno);
      } // if
      if (((buf.st_mode & S_IFMT) == S_IFBLK) != 1)
      {
         printf (gettext ("Daisy-player - Version %s %s"),
                          PACKAGE_VERSION, "\n");
         puts ("(C)2003-2021 J. Lemmens");
         beep ();
         remove_tmp_dir (&misc);
         printf ("\n%s is not a cd device\n", misc.cd_dev);
         fflush (stdout);
         _exit (EXIT_FAILURE);
      } // if
      snprintf (misc.cmd, MAX_CMD, "eject -tp %s", misc.cd_dev);
      system (misc.cmd);
      start = time (NULL);

      misc.daisy_mp = strdup (misc.cd_dev);
      cd = NULL;
      cdio_init ();
      do
      {
         if (time (NULL) - start >= 60)
         {
            printf ("%s\n", gettext ("No Daisy CD in drive."));
            remove_tmp_dir (&misc);
            _exit (EXIT_FAILURE);
         } // if
         cd = cdio_open (misc.cd_dev, DRIVER_UNKNOWN);
      } while (cd == NULL);
      start = time (NULL);
      do
      {
         if (time (NULL) - start >= 20)
         {
            printf ("%s\n", gettext ("No Daisy CD in drive."));
            remove_tmp_dir (&misc);
            _exit (EXIT_FAILURE);
         } // if
         switch (misc.cd_type = cdio_get_discmode (cd))
         {
         case CDIO_DISC_MODE_CD_DATA:  /**< CD-ROM form 1 */
         case CDIO_DISC_MODE_CD_XA: /**< CD-ROM XA form2 */
         case CDIO_DISC_MODE_DVD_ROM: /**< DVD ROM (e.g. movies) */
         case CDIO_DISC_MODE_DVD_RAM: /**< DVD-RAM */
         case CDIO_DISC_MODE_DVD_R: /**< DVD-R */
         case CDIO_DISC_MODE_DVD_RW: /**< DVD-RW */
         case CDIO_DISC_MODE_HD_DVD_ROM: /**< HD DVD-ROM */
         case CDIO_DISC_MODE_HD_DVD_RAM: /**< HD DVD-RAM */
         case CDIO_DISC_MODE_HD_DVD_R: /**< HD DVD-R */
         case CDIO_DISC_MODE_DVD_PR: /**< DVD+R */
         case CDIO_DISC_MODE_DVD_PRW: /**< DVD+RW */
         case CDIO_DISC_MODE_DVD_PRW_DL: /**< DVD+RW DL */
         case CDIO_DISC_MODE_DVD_PR_DL: /**< DVD+R DL */
         case CDIO_DISC_MODE_CD_MIXED:
         {
            if (get_mount_point (&misc) == NULL)
            {
// if not found a mounted cd, try to mount one
               do
               {
                  if (time (NULL) - start >= 10)
                  {
                     printf ("%s\n", gettext ("No Daisy CD in drive."));
                     remove_tmp_dir (&misc);
                     _exit (EXIT_FAILURE);
                  } // if
                  snprintf (misc.cmd, MAX_CMD,
                         "udisksctl mount -b %s > /dev/null", misc.cd_dev);
                  system (misc.cmd);
                  misc.mounted_by_daisy_player = 1;
               } while (get_mount_point (&misc) == NULL);
            } // if
            break;
         } // TRACK_COUNT_DATA"
         case CDIO_DISC_MODE_CD_DA: /**< CD-DA */
         {
// probably an Audio-CD
            printf ("\n%s ", gettext ("Found an Audio-CD."));
            fflush (stdout);
            if (misc.cddb_flag == 'y')
            {
               printf ("\n");
               printf (gettext ("Get titles from freedb.freedb.org..."));
               fflush (stdout);
            } // if
            refresh ();
            strncpy (misc.bookmark_title, "Audio-CD", MAX_STR - 1);
            strncpy (misc.daisy_title, "Audio-CD", MAX_STR - 1);
            init_paranoia (&misc);
            daisy = get_number_of_tracks (&misc);
            get_toc_audiocd (&misc, daisy);
            misc.daisy_mp = strdup ("/tmp");
            for (misc.current = 0; misc.current < misc.total_items; misc.current++)
            {
               daisy[misc.current].smil_file = strdup ("");
               daisy[misc.current].smil_anchor = strdup ("");
            } // for
            break;
         } //  TRACK_COUNT_AUDIO
         case CDIO_DISC_MODE_CD_I:
         case CDIO_DISC_MODE_DVD_OTHER:
         case CDIO_DISC_MODE_NO_INFO:
         case CDIO_DISC_MODE_ERROR:
            printf ("\n%s\n", gettext ("No DAISY-CD or Audio-cd found"));
            remove_tmp_dir (&misc);
            return 0;
         } // switch
      } while (misc.cd_type == -1);
   } // if use misc.cd_dev
   misc.player_pid = -2;
   if (chdir (misc.daisy_mp) == -1)
   {
      int e;
      char str[MAX_STR];

      e = errno;
      snprintf (str, MAX_STR, "daisy_mp %s", misc.daisy_mp);
      remove_tmp_dir (&misc);
      failure (&misc, str, e);
   } // if
   misc.current = 0;
   misc.reader = FALSE;
   misc.max_x = misc.max_y = 0;

   initscr ();
   if (! (misc.titlewin = newwin (2, 80,  0, 0)) ||
       ! (misc.screenwin = newwin (23, 80, 2, 0)))
      failure (&misc, "No curses", errno);
   getmaxyx (misc.screenwin, misc.max_y, misc.max_x);

   if (misc.cd_type != CDIO_DISC_MODE_CD_DA)
   {
      daisy = create_daisy_struct (&misc, &my_attribute, daisy);
      for (misc.current = 0; misc.current < misc.total_items; misc.current++)
      {
         daisy[misc.current].smil_file = strdup ("");
         daisy[misc.current].smil_anchor = strdup ("");
      } // for
      snprintf (DISCINFO_HTML, MAX_STR - 1,
                "%s/discinfo.html", misc.daisy_mp);
      if (access (DISCINFO_HTML, R_OK) == 0)
         handle_discinfo (&misc, &my_attribute, daisy, DISCINFO_HTML);
      if (misc.discinfo == 0)
      {
         misc.has_audio_tag = 0;
         if (access (misc.ncc_html, R_OK) == 0)
         {
// this is daisy2
            htmlDocPtr doc;
            xmlTextReaderPtr reader;

            if (! (doc = htmlParseFile (misc.ncc_html, "UTF-8")))
               failure (&misc, misc.ncc_html, errno);
            if (! (reader = xmlReaderWalker (doc)))
            {
               int e;
               e = errno;
               snprintf (misc.str, MAX_STR,
                           gettext ("Cannot read %s"), misc.ncc_html);
               remove_tmp_dir (&misc);
               failure (&misc, misc.str, e);
            } // if
            while (1)
            {
               if (*misc.bookmark_title)
// if misc.bookmark_title already is set
                  break;
               if (! get_tag_or_label (&misc, &my_attribute, reader))
                  break;
               if (strcasecmp (misc.tag, "title") == 0)
               {
                  if (! get_tag_or_label (&misc, &my_attribute, reader))
                     break;
                  if (*misc.label)
                  {
                     strncpy (misc.bookmark_title, misc.label, MAX_STR - 1);
                     break;
                  } // if
               } // if
            } // while
            xmlTextReaderClose (reader);
            xmlFreeDoc (doc);
            fill_daisy_struct_2 (&misc, &my_attribute, daisy);
         } // ncc_html
         else
         {
// this is daisy3
            int i;

            strncpy (misc.daisy_version, "3", 2);
            misc.ncx_failed = misc.opf_failed = 0;
            read_daisy_3 (&misc, &my_attribute, daisy);
            if (misc.verbose)
            {
               printf ("\rRead ID's ");
               fflush (stdout);
            } // if
            for (i = 0; i < misc.total_items; i++)
            {
               htmlDocPtr doc;
               xmlTextReaderPtr last;

               if (i < misc.total_items - 1)
               {
                  if (! *daisy[i + 1].smil_anchor)
                     continue;
               } // if
               if (! (doc = htmlParseFile (daisy[i].smil_file, "UTF-8")))
               {
                  failure (&misc, daisy[i].smil_file, errno);
               } // if
               if (! (last = xmlReaderWalker (doc)))
               {
                  failure (&misc, daisy[i].smil_file, errno);
               } // if
               while (1)
               {
                  if (! get_tag_or_label (&misc, &my_attribute, last))
                     break;
                  if (i < misc.total_items - 1)
                     if (strcasecmp
                         (my_attribute.id, daisy[i + 1].smil_anchor) == 0)
                        break;
                  if (*my_attribute.id)
                  {
                     strcpy (daisy[i].last_id, my_attribute.id);
                  } // if
               } // while
               xmlTextReaderClose (last);
               xmlFreeDoc (doc);
               if (misc.verbose)
               {
                  printf (". ");
                  fflush (stdout);
               } // if
            } // for
            fill_page_numbers (&misc, daisy, &my_attribute);
            calculate_times_3 (&misc, &my_attribute, daisy);
         } // if daisy3
         misc.verbose = 0;
         if (misc.total_items == 0)
            misc.total_items = 1;
      } // if (misc.discinfo == 0);
   } // if misc.audiocd == 0
   if (*misc.bookmark_title == 0)
   {
      strcpy (misc.bookmark_title, misc.daisy_title);
      if (strchr (misc.bookmark_title, '/'))
      {
         int i = 0;

         while (misc.bookmark_title[i] != 0)
            misc.bookmark_title[i++] = '-';
      } // if
   } // if

   /* TEST only: prints out the "spine" of the document.  Used by the tests to
    * verify that the parsing keeps working */
   if (test_mode)
   {
      printf("\n\r\nSPINE (%d):\n", misc.total_items);
      for (int i = 0; i < misc.total_items; i++)
      {
         printf("% 4d: %s#%s\n", i + 1, daisy[i].smil_file, daisy[i].smil_anchor);
         fflush(stdout);
      }
      remove_tmp_dir (&misc);
      return 0;
   }

   misc.level = 1;
   snprintf (misc.tmp_wav, MAX_STR, "%s/daisy-player.wav", misc.tmp_dir);
   if ((misc.tmp_wav_fd = mkstemp (misc.tmp_wav)) == 01)
      failure (&misc, "mkstemp ()", errno);
   misc.pause_resume_playing = -1;
   keypad (misc.screenwin, TRUE);
   meta (misc.screenwin, TRUE);
   nonl ();
   noecho ();
   wattron (misc.titlewin, A_BOLD);
   snprintf (str, MAX_STR - 1, gettext (
        "Daisy-player - Version %s - (C)2021 J. Lemmens"), PACKAGE_VERSION);
   mvwprintw (misc.titlewin, 0, 0, str);
   wrefresh (misc.titlewin);

   if (strlen (misc.daisy_title) + strlen (str) >= 80)
      mvwprintw (misc.titlewin, 0,
                 80 - strlen (misc.daisy_title) - 4, "... ");
   mvwprintw (misc.titlewin, 0, 80 - strlen (misc.daisy_title),
              "%s", misc.daisy_title);
   wrefresh (misc.titlewin);
   mvwprintw (misc.titlewin, 1, 0,
              "----------------------------------------");
   wprintw (misc.titlewin, "----------------------------------------");
   mvwprintw (misc.titlewin, 1, 0, "%s ", gettext ("Press 'h' for help"));
   browse (&misc, &my_attribute, daisy, sound_devices);
   return EXIT_SUCCESS;
} // main
