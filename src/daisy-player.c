/* daisy-player
 *
 * Copyright (C)2003-2018 J. Lemmens
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
   struct passwd *pw;;
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
   if (misc->cd_type != CDIO_DISC_MODE_CD_DA)
   {
      xmlTextWriterWriteFormatAttribute
         (writer, BAD_CAST "id", "%s", misc->current_id);
   }
   else
      xmlTextWriterWriteFormatAttribute
         (writer, BAD_CAST "seconds", "%d",
                  (int) (time (NULL) - misc->elapsed_seconds));
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
   strncpy (begin_str, orig_begin,  MAX_STR - 1);
   begin = begin_str;
   while (! isdigit (*begin))
      begin++;
   if (strchr (begin, 's'))
      *strchr (begin, 's') = 0;
   if (! strchr (begin, ':'))
      misc->clip_begin = (float) atof (begin);
   else
      misc->clip_begin = read_time (begin);

// fill end
   while (! isdigit (*end))
      end++;
   if (strchr (end, 's'))
      *strchr (end, 's') = 0;
   if (! strchr (end, ':'))
      misc->clip_end = (float) atof (end);
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

void get_bookmark (misc_t *misc, my_attribute_t *my_attribute,
                   daisy_t *daisy)
{
   xmlTextReaderPtr local_reader;
   htmlDocPtr local_doc;
   struct passwd *pw;
   char *id, *name;
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
      misc->current = 0;
   misc->displaying = misc->playing = misc->current;
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
   {
      misc->elapsed_seconds = time (NULL) - misc->elapsed_seconds;
      return;
   } // if
   id = my_attribute->id;
   open_clips_file (misc, my_attribute, daisy[misc->current].clips_file,
                    daisy[misc->current].clips_anchor);
   while (1)
   {
      if (strcmp (misc->current_id, id) == 0)
         break;
      get_next_clips (misc, my_attribute, daisy);
   } // while
   if (misc->level < 1)
      misc->level = 1;
   misc->current_page_number = daisy[misc->playing].page_number - 1;
   misc->just_this_item = -1;
   pause_resume (misc, my_attribute, daisy);
   pause_resume (misc, my_attribute, daisy);
   view_screen (misc, daisy);
} // get_bookmark

void get_next_clips (misc_t *misc, my_attribute_t *my_attribute,
                    daisy_t *daisy)
{
   int eof;

   while (1)
   {
      eof = 1 - get_tag_or_label (misc, my_attribute, misc->reader);
      if (strcasecmp (misc->tag, "audio") == 0)
      {
         misc->current_audio_file =
                         convert_URL_name (misc, my_attribute->src);
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
                       daisy[misc->playing + 1].clips_anchor) == 0) ||
          eof)
      {
         if (*daisy[misc->playing + 1].clips_anchor || eof)
         {
// go to next item
            strncpy (my_attribute->clip_begin, "0", 2);
            misc->clip_begin = 0;
            if (++misc->playing >= misc->total_items)
            {
               struct passwd *pw;
               int len;
               char *name;

               pw = getpwuid (geteuid ());
               quit_daisy_player (misc, daisy);
               len = strlen (pw->pw_dir) + strlen (misc->bookmark_title) +
                     strlen (get_mcn (misc)) + 100;
               name = malloc (len);
               snprintf (name, len, "%s/.daisy-player/%s%s",
                         pw->pw_dir, misc->bookmark_title, get_mcn (misc));
               unlink (name);
               _exit (-1);
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
            if (misc->playing > -1)
            {
               open_clips_file (misc, my_attribute,
                                daisy[misc->playing].clips_file,
                                daisy[misc->playing].clips_anchor);
            } // if
         } // if
      } // if
   } // while
} // get_next_clips            

void view_page (misc_t *misc, daisy_t *daisy)
{
   if (misc->playing == -1)
      return;
   if (daisy[misc->playing].screen != daisy[misc->current].screen)
      return;
   if (misc->total_pages == 0)
      return;
   wattron (misc->screenwin, A_BOLD);
   if (misc->current_page_number)
      mvwprintw (misc->screenwin, daisy[misc->playing].y, 62, "(%3d)",
                 misc->current_page_number);
   wattroff (misc->screenwin, A_BOLD);
   wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
   wrefresh (misc->screenwin);
} // view_page

void view_time (misc_t *misc, daisy_t *daisy)
{
   float remain_seconds = 0, elapsed_seconds = 0;

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
      struct stat buf;

      if (stat (misc->tmp_wav, &buf))
         return;
      elapsed_seconds = (misc->clip_begin - daisy[misc->playing].begin) +
                        (time (NULL) - buf.st_atime);
      remain_seconds = daisy[misc->playing].duration - elapsed_seconds;
   } // if
   elapsed_seconds /= misc->speed;
   wattron (misc->screenwin, A_BOLD);
   mvwprintw (misc->screenwin, daisy[misc->playing].y, 68, "%02d:%02d",
              (int) elapsed_seconds / 60, (int) elapsed_seconds % 60);
   remain_seconds /= misc->speed;
   mvwprintw (misc->screenwin, daisy[misc->playing].y, 74, "%02d:%02d",
              (int) remain_seconds / 60, (int) remain_seconds % 60);
   wattroff (misc->screenwin, A_BOLD);
   wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
   wrefresh (misc->screenwin);
} // view_time

void view_screen (misc_t *misc, daisy_t *daisy)
{
   int i, x, x2;
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
   int hours   = time / 3600;
   int minutes = (time - hours * 3600) / 60;
   int seconds = time - (hours * 3600 + minutes * 60);
   mvwprintw (misc->titlewin, 1, 47, " ");
   wprintw (misc->titlewin, gettext ("total length: %02d:%02d:%02d"),
              hours, minutes,seconds);
   wprintw (misc->titlewin, " ");
   mvwprintw (misc->titlewin, 1, 74, " %d/%d ",
              daisy[misc->current].screen + 1,
              daisy[misc->total_items - 1].screen + 1);
   wrefresh (misc->titlewin);
   wclear (misc->screenwin);
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
      if (daisy[i].level <= misc->level)
      {
         int x, dur = 0;

         x = i;
         do
         {
            dur += daisy[x].duration;
            if (++x >= misc->total_items)
               break;
         } while (daisy[x].level > misc->level);
         dur /= misc->speed;
         mvwprintw (misc->screenwin, daisy[i].y, 74, "%02d:%02d",
                    (int) (dur + .5) / 60, (int) (dur + .5) % 60);
      } // if
      if (i >= misc->total_items - 1)
         break;
      i++;
   } while (daisy[i].screen == daisy[misc->current].screen);
   if (misc->just_this_item != -1 &&
       daisy[misc->current].screen == daisy[misc->playing].screen)
      mvwprintw (misc->screenwin, daisy[misc->displaying].y, 0, "J");
   wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
   wrefresh (misc->screenwin);
   view_page (misc, daisy);
   view_time (misc, daisy);
} // view_screen

void start_playing (misc_t *misc, daisy_t *daisy)
{
   if (strcasecmp (misc->tag, "audio") != 0)
      return;
   if (misc->playing == -1)
      return;
   if (misc->clip_end - misc->clip_begin <= 0)
      return;
   misc->elapsed_seconds = time (NULL);
   switch (misc->player_pid = fork ())
   {
   case -1:
      failure (misc, "fork ()", errno);
   case 0: // child
      break;
   default: // parent
      return;
   } // switch

   char tempo_str[15];
   char begin[20], duration[20];

   view_page (misc, daisy);
   lseek (misc->tmp_wav_fd, SEEK_SET, 0);
   snprintf (begin, 20, "%f", misc->clip_begin);
   snprintf (duration, 20, "%f", misc->clip_end - misc->clip_begin);
   madplay (misc->current_audio_file, begin, duration, misc->tmp_wav);
   snprintf (tempo_str, 10, "%lf", misc->speed);
   misc->player_pid = setpgrp ();
   playfile (misc, misc->tmp_wav, "wav", misc->sound_dev, "pulseaudio",
             tempo_str);
   _exit (0);
} // start_playing

void open_clips_file (misc_t *misc, my_attribute_t *my_attribute,
                     char *clips_file, char *anchor)
{
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      return;
   if (! (misc->doc = htmlParseFile (clips_file, "UTF-8")))
   {
      int e;
      char str[MAX_STR];

      e = errno;
      snprintf (str, MAX_STR, "htmlParseFile (%s)", clips_file);
      failure (misc,  str, e);
   } // if
   if (! (misc->reader = xmlReaderWalker (misc->doc)))
   {
      int e;
      char str[MAX_STR];

      e = errno;
      snprintf (str, MAX_STR, "open_clips_file(): %s", clips_file);
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
} // open_clips_file

void write_wav (misc_t *misc, my_attribute_t *my_attribute,
                daisy_t *daisy, char *label)
{
   char *out_file, *out_cdr, *complete_cdr;
   struct passwd *pw;
   int old_playing, old_displaying, old_current, old_just_this_item;
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
      pid = play_track (misc, out_file, "wav",
                        daisy[misc->current].first_lsn);
      misc->speed = sp;
      do
      {
         if (! (p_readbuf = paranoia_read (misc->par, NULL)))
            break;
         switch (write (misc->pipefd[1], p_readbuf, CDIO_CD_FRAMESIZE_RAW));
      } while (++misc->lsn_cursor <= daisy[misc->current].last_lsn);
      if (misc->par)
         paranoia_free (misc->par);
// The .wav file is not correctly closed; I don't know the solution, sorry.
      free (out_file);
      kill (pid, SIGQUIT);
      return;
   } // if

   old_playing = misc->playing;
   old_displaying = misc->displaying;
   old_current = misc->current;
   old_just_this_item = misc->just_this_item;
   misc->just_this_item = misc->playing = misc->current;

   out_cdr = malloc (strlen (misc->tmp_dir) + 10);
   sprintf (out_cdr, "%s/out.cdr", misc->tmp_dir);
   complete_cdr = malloc (strlen (misc->tmp_dir) + 20);
   sprintf (complete_cdr, "%s/complete.cdr", misc->tmp_dir);
   w = open (complete_cdr, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
   while (1)
   {
#define BUF_SIZE 8192
      int r;
      char buffer[BUF_SIZE];
      ssize_t in, out;

      open_clips_file (misc, my_attribute, daisy[misc->current].clips_file,
                       daisy[misc->current].clips_anchor);
      get_next_clips (misc, my_attribute, daisy);
      snprintf (begin, 20, "%f", daisy[misc->current].begin);
      snprintf (duration, 20, "%f", daisy[misc->current].duration);
      if (access (misc->current_audio_file, R_OK) == -1)
      {
         int e;

         e= errno;
         endwin ();
         beep ();
         printf ("%s: %s\n", misc->current_audio_file, strerror (e));
         _exit (-1);
      } // if
      madplay (misc->current_audio_file, begin, duration, out_cdr);
      r = open (out_cdr, O_RDONLY);
      while ((in = read (r, &buffer, BUF_SIZE)) > 0)
      {
         out = write (w, &buffer, in);
         if (out != in)
            failure (misc, "read/write", errno);
      } // while
      close (r);
      if (misc->current + 1 >= misc->total_items)
         break;
      if (daisy[misc->current + 1].level <= misc->level)
         break;
      misc->current += 1;
   } // while
   close (w);        
   playfile (misc, complete_cdr, "cdr", out_file, "wav", "1");
   free (out_file);
   free (out_cdr);
   free (complete_cdr);
   misc->playing = old_playing;
   misc->displaying = old_displaying;
   misc->current= old_current;
   misc->just_this_item = old_just_this_item;
   wmove (misc->screenwin, daisy[misc->playing].y, daisy[misc->playing].x);
} // write_wav

void pause_resume (misc_t *misc, my_attribute_t *my_attribute, daisy_t *daisy)
{
   if (misc->playing < 0 && misc->pause_resume_playing < 0)
      return;
   if (misc->playing > -1)
   {
      misc->pause_resume_playing = misc->playing;
      misc->pause_resume_id = misc->current_id;
      misc->playing = -1;
      misc->pause_resume_lsn_cursor = misc->lsn_cursor;
      kill_player (misc);
      return;
   } // if

   misc->playing = misc->pause_resume_playing;
   misc->lsn_cursor = misc->pause_resume_lsn_cursor;
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
   {
      misc->player_pid = play_track (misc, misc->sound_dev, "pulseaudio",
                                     misc->pause_resume_lsn_cursor - 75 * 4);
      return;
   } // if
   open_clips_file (misc, my_attribute, daisy[misc->playing].clips_file,
                    daisy[misc->playing].clips_anchor);
   while (1)
   {
      get_next_clips (misc, my_attribute, daisy);
      if (strcmp (misc->pause_resume_id, misc->current_id) == 0)
      {
         start_playing (misc, daisy);
         view_screen (misc, daisy);
         return;
      } // if
   } // while
} // pause_resume

void store_to_disk (misc_t *misc, my_attribute_t *my_attribute,
                    daisy_t *daisy)
{
   char *str;
   int i, current, playing;

   playing =misc->playing;
   if (playing > -1)
      pause_resume (misc, my_attribute, daisy);
   wclear (misc->screenwin);
   current = misc->current;
   str = malloc (strlen (daisy[current].label) + 10);
   snprintf (str, MAX_STR - 1, "%s", daisy[misc->current].label);
   wprintw (misc->screenwin,
            "\nStoring \"%s\" as \"%s.wav\" into your home-folder...",
            daisy[current].label, str);
   wrefresh (misc->screenwin);
   for (i = 0; str[i] != 0; i++)
      if (str[i] == '/')
         str[i] = '-';
   write_wav (misc, my_attribute, daisy, str);
   if (playing > -1)
      pause_resume (misc, my_attribute, daisy);
   view_screen (misc, daisy);
} // store_to_disk

void help (misc_t *misc, my_attribute_t *my_attribute, daisy_t *daisy)
{
   int y, x, playing;

   playing = misc->playing;
   if (playing > -1)
      pause_resume (misc, my_attribute, daisy);
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
            ("cursor right,6  - skip to next phrase"));
   wprintw (misc->screenwin, "%s\n", gettext
            ("cursor left,4   - skip to previous phrase"));
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
   pause_resume (misc, my_attribute, daisy);
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
   for (x = 0; x < misc->total_items; x++)
   {
      daisy[x].duration = 0;
      if (! *daisy[x].clips_file)
         continue;
      doc = htmlParseFile (daisy[x].clips_file, "UTF-8");
      if (! (parse = xmlReaderWalker (doc)))
      {
         endwin ();
         beep ();
         printf ("\n");
         printf (gettext ("Cannot read %s"), daisy[x].clips_file);
         printf ("\n");
         fflush (stdout);
         _exit (1);
      } // if

// parse this smil
      if (*daisy[x].clips_anchor)
      {
         while (1)
         {
            if (! get_tag_or_label (misc, my_attribute, parse))
               break;
            if (strcasecmp (daisy[x].clips_anchor, my_attribute->id) == 0)
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
                  if (*daisy[x + 1].clips_anchor)
                  {
                     if (strcasecmp
                          (my_attribute->id, daisy[x + 1].clips_anchor) == 0)
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
            if (x < misc->total_items - 1 && *daisy[x + 1].clips_anchor)     
               if (strcasecmp
                           (my_attribute->id, daisy[x + 1].clips_anchor) == 0)
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
      quit_daisy_player (misc, daisy);
      printf ("%s\n", gettext (
        "This book has no audio. Play this book with eBook-speaker"));
      _exit (-1);
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

   pw = getpwuid (geteuid ());
   snprintf (str, MAX_STR - 1, "%s/.daisy-player.xml", pw->pw_dir);
   doc = xmlRecoverFile (str);
   if (! (reader = xmlReaderWalker (doc)))
   {
      strncpy (misc->sound_dev, "0", MAX_STR - 1);
      return;
   } // if
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
      return;
   xmlTextWriterStartDocument (writer, NULL, NULL, NULL);
   xmlTextWriterStartElement (writer, BAD_CAST "prefs");
   xmlTextWriterWriteFormatAttribute
      (writer, BAD_CAST "sound_dev", "%s", misc->sound_dev);
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

void quit_daisy_player (misc_t *misc, daisy_t *daisy)
{
   view_screen (misc, daisy);
   endwin ();
   if (misc->cd_type != CDIO_DISC_MODE_CD_DA)
      xmlTextReaderClose (misc->reader);
   kill_player (misc);
   put_bookmark (misc);
   save_xml (misc);
   close (misc->tmp_wav_fd);
   unlink (misc->tmp_wav);
   puts ("");
   remove_tmp_dir (misc);
} // quit_daisy_player

void search (misc_t *misc, my_attribute_t *my_attribute, daisy_t *daisy,
             int start, char mode)
{
   int c, found = 0, flag = 0;

   if (misc->playing > -1)
   {
      pause_resume (misc, my_attribute, daisy);
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
      if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      {
         misc->elapsed_seconds = time (NULL);
         misc->player_pid = play_track (misc, misc->sound_dev, "pulseaudio",
                daisy[misc->current].first_lsn);
      }
      else
         open_clips_file (misc, my_attribute, daisy[misc->current].clips_file,
                         daisy[misc->current].clips_anchor);
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
         misc->playing = -1;
         pause_resume (misc, my_attribute, daisy);
      }
      else
      {
         pause_resume (misc, my_attribute, daisy);
      } // if
   } // if
} // search

void go_to_time (misc_t *misc, daisy_t *daisy, my_attribute_t *my_attribute)
{
   char time_str[10];
   int secs;
                                
   kill_player (misc);
   if (misc->cd_type != CDIO_DISC_MODE_CD_DA)
      misc->player_pid = -2;
   misc->just_this_item = -1;
   misc->playing = misc->displaying = misc->current;
   view_screen (misc, daisy);
   mvwprintw (misc->titlewin, 1, 0, "----------------------------------------");
   wprintw (misc->titlewin, "----------------------------------------");
   *time_str = 0;
   while (1)
   {
      mvwprintw (misc->titlewin, 1, 0, "%s ",
                 gettext ("Go to time (MM:SS):"));
      echo ();
      wgetnstr (misc->titlewin, time_str, 5);
      noecho ();
      if (strlen (time_str) == 0 || strlen (time_str) == 5)
      {
         if (strlen (time_str) == 0)
            beep ();
         break;
      }
      else
         beep ();
   } // while
   view_screen (misc, daisy);
   if (strlen (time_str) == 0)
      secs = 0;
   else
   {
      secs = (time_str[0] - 48) * 600 + (time_str[1] - 48)* 60 +
             (time_str[3] - 48) * 10 + (time_str[4] - 48);
   } // if
   if (secs >= daisy[misc->current].duration / misc->speed)
   {
      beep ();
      secs = 0;
   } // if
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
   {
      misc->player_pid = play_track (misc, misc->sound_dev, "pulseaudio",
                         daisy[misc->current].first_lsn + (secs * 75));
      misc->elapsed_seconds = time (NULL) - secs;
      misc->prev_id = strdup (misc->current_id);
      return;
   } // if

   misc->clip_begin = 0;
   open_clips_file (misc, my_attribute, daisy[misc->current].clips_file,

                    daisy[misc->current].clips_anchor);
   misc->current_id = strdup (daisy[misc->current].first_id);
   do     
   {
      get_next_clips (misc, my_attribute, daisy);
   } while (misc->clip_begin / misc->speed < secs);
   start_playing (misc, daisy);
   view_screen (misc, daisy);
} // go_to_time

void skip_left (misc_t *misc, my_attribute_t *my_attribute,
                daisy_t *daisy)
{
   char *prev_id;

   prev_id = strdup (misc->prev_id);
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      return;
   if (misc->playing < 0) // not playing
   {
      beep ();
      return;
   } // if not playing
   if (misc->playing == 0) // first item
   {
      if (strcmp (daisy[misc->playing].first_id, misc->audio_id) == 0)
      {
         beep ();
         return;
      } // if
   } // if first item
   if (misc->player_pid > -1)
   {
      kill_player (misc);
      misc->player_pid = -2;
   } // if
   if (misc->reader)
      xmlTextReaderClose (misc->reader);
   if (misc->doc)
      xmlFreeDoc (misc->doc);
   if (strcmp (daisy[misc->playing].first_id, misc->audio_id) == 0)
   {
      if (misc->just_this_item > -1 &&
          daisy[misc->playing].level  <= misc->level)
      {
         beep ();
         misc->current = misc->displaying = misc->playing;
         misc->playing = misc->just_this_item = -1;
         view_screen (misc, daisy);
         wmove (misc->screenwin, daisy[misc->current].y,
                daisy[misc->current].x);
         return;
      } // if misc->just_this_item
// go to previous item
      misc->playing--;
      prev_id = strdup (daisy[misc->playing].last_id);
   } // go to previous item
   misc->current = misc->displaying = misc->playing;
   misc->current_page_number = daisy[misc->current].page_number;
   open_clips_file (misc, my_attribute, daisy[misc->playing].clips_file,
                    daisy[misc->playing].clips_anchor);
   misc->current_id = strdup (daisy[misc->playing].first_id);
   while (1)
   {
      if (strcmp (misc->current_id, prev_id) == 0)
         break;
      get_next_clips (misc, my_attribute, daisy);
   } // while
   start_playing (misc, daisy);
   view_screen (misc, daisy);
   return;
} // skip_left

void browse (misc_t *misc, my_attribute_t *my_attribute,
             daisy_t *daisy, char *wd)
{
   int old_screen, i;

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
   get_bookmark (misc, my_attribute, daisy);

// convert ALSA device name into pulseaudio device name
   if (strncmp (misc->sound_dev, "hw:", 3) == 0)
      misc->sound_dev += 3;

   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
   {
      for (i = 0; i < misc->total_items; i++)
      {
         daisy[i].level = 1;
         daisy[i].page_number = 0;
      } // for
      misc->depth = 1;
      misc->player_pid = play_track (misc, misc->sound_dev, "pulseaudio",
                      daisy[misc->current].first_lsn + (misc->seconds * 75));
      misc->elapsed_seconds = time (NULL) - misc->seconds;
   } // if
   view_screen (misc, daisy);
   nodelay (misc->screenwin, TRUE);
   wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
   if (misc->cd_type != CDIO_DISC_MODE_CD_DA && misc->has_audio_tag == 0)
   {
      beep ();
      quit_daisy_player (misc, daisy);
      printf ("%s\n", gettext (
        "This book has no audio. Play this book with eBook-speaker"));
      _exit (-1);
   } // if

   for (;;)
   {
      switch (wgetch (misc->screenwin))
      {
      case 13: // ENTER
         misc->just_this_item = -1;
         view_screen (misc, daisy);
         misc->playing = misc->displaying = misc->current;
         misc->current_id = strdup ("");
         misc->current_page_number = daisy[misc->playing].page_number;
         if (misc->player_pid > -1)
            kill_player (misc);
         misc->player_pid = -2;
         if (misc->discinfo)
         {
            int len;
            char *str;

            len = strlen (wd) + strlen ( PACKAGE) +
                  strlen (misc->daisy_mp) +
                  strlen (daisy[misc->current].daisy_mp) +
                  strlen (misc->sound_dev) + 100;
            str = malloc (len);
            snprintf (str, len,
                      "cd \"%s\"; \"%s\" \"%s\"/\"%s\" -d %s",
                      wd, PACKAGE, misc->daisy_mp,
                      daisy[misc->current].daisy_mp, misc->sound_dev);
            switch (system (str));
            snprintf (str, len,
                      "cd \"%s\"; \"%s\" \"%s\" -d %s\n", wd, PACKAGE,
                      misc->daisy_mp, misc->sound_dev);
            switch (system (str));
            quit_daisy_player (misc, daisy);
            _exit (0);
         } // if
         if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
         {
            misc->player_pid = play_track (misc, misc->sound_dev, "pulseaudio",
                daisy[misc->current].first_lsn);
            misc->elapsed_seconds = time (NULL);
            break;
         } // if
         open_clips_file (misc, my_attribute,
                daisy[misc->current].clips_file,
                daisy[misc->current].clips_anchor);
         misc->elapsed_seconds = time (NULL);
         break;
      case '/':
         if (misc->discinfo)
         {
            beep ();
            break;
         } // if
         search (misc, my_attribute, daisy, misc->current + 1, '/');
         break;
      case ' ':
      case KEY_IC:
      case '0':
         if (misc->discinfo)
         {
            beep ();
            break;
         } // if
         pause_resume (misc, my_attribute, daisy);
         break;
      case 'd':
         if (misc->discinfo)
         {
            beep ();
            break;
         } // if
         store_to_disk (misc, my_attribute, daisy);
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
         quit_daisy_player (misc, daisy);
         snprintf (misc->cmd, MAX_CMD, "eject -mp %s", misc->cd_dev);
         switch  (system (misc->cmd));
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
         go_to_time (misc, daisy, my_attribute);
         break;
      case 'G':
         if (misc->discinfo)
         {
            beep ();
            break;
         } // if
         if (misc->total_pages)
            go_to_page_number (misc, my_attribute, daisy);
         else
            beep ();
         break;
      case 'h':
      case '?':
         help (misc, my_attribute, daisy);
         break;
      case 'j':
      case '5':
      case KEY_B2:
         if (misc->discinfo)
         {
            beep ();
            break;
         } // if
         if (misc->just_this_item != -1)
            misc->just_this_item = -1;
         else
            misc->just_this_item = misc->current;
         mvwprintw (misc->screenwin, daisy[misc->displaying].y, 0, " ");
         if (misc->playing == -1)
         {
            misc->just_this_item = misc->displaying = misc->playing =
                                   misc->current;
            kill_player (misc);
            misc->player_pid = -2;
            if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
            {
               misc->player_pid = play_track (misc, misc->sound_dev, "pulseaudio",
                   daisy[misc->current].first_lsn);
            }
            else
            {
               open_clips_file (misc, my_attribute,
                 daisy[misc->current].clips_file,
                 daisy[misc->current].clips_anchor);
            } // if
         } // if
         wattron (misc->screenwin, A_BOLD);
         if (misc->just_this_item != -1 &&
             daisy[misc->displaying].screen == daisy[misc->playing].screen)
            mvwprintw (misc->screenwin, daisy[misc->displaying].y, 0, "J");
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
      case 'n':
         if (misc->discinfo)
         {
            beep ();
            break;
         } // if
         search (misc, my_attribute, daisy, misc->current + 1, 'n');
         break;
      case 'N':
         if (misc->discinfo)
         {
            beep ();
            break;
         } // if
         search (misc, my_attribute, daisy, misc->current - 1, 'N');
         break;
      case 'o':
         select_next_output_device (misc, my_attribute, daisy);
         break;
      case 'p':
         put_bookmark (misc);
         save_xml (misc);
         break;
      case 'q':
         quit_daisy_player (misc, daisy);
         _exit (0);
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
            kill_player (misc);
            misc->lsn_cursor += 8 * 75;
            if (misc->lsn_cursor >= daisy[misc->playing].last_lsn)
            {
               if (misc->just_this_item > -1)
               {
                  misc->displaying = misc->current = misc->playing;
                  misc->playing = -1;
                  view_screen (misc, daisy);
                  break;
               }
               else
                  misc->displaying = misc->current = ++misc->playing;
               if (misc->current >= misc->total_items)
               {
                  struct passwd *pw;
                  int len;
                  char *str;

                  pw = getpwuid (geteuid ());
                  quit_daisy_player (misc, daisy);
                  len = strlen (pw->pw_dir) +
                        strlen (misc->bookmark_title) +
                        strlen (get_mcn (misc) + 100);
                  str = malloc (len);
                  snprintf (str, len, "%s/.daisy-player/%s%s",
                            pw->pw_dir, misc->bookmark_title, get_mcn (misc));
                  unlink (str);
                  _exit (0);
               } // if
            } // if
            misc->player_pid = play_track (misc, misc->sound_dev, "pulseaudio",
                               misc->lsn_cursor);
            break;
         } // if
         skip_right (misc, daisy);
         break;
      case KEY_LEFT:
      case '4':
         if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
         {
            kill_player (misc);
            misc->lsn_cursor -= 12 * 75;
            if (misc->lsn_cursor < daisy[misc->playing].first_lsn)
            {
               if (misc->playing > 0)
                  if (misc->just_this_item > -1)
                     misc->lsn_cursor = daisy[misc->playing].first_lsn;
                  else
                     misc->current = misc->displaying = --misc->playing;
               else
                  if (misc->lsn_cursor < daisy[misc->playing].first_lsn)
                     misc->lsn_cursor = daisy[misc->playing].first_lsn;
            } // if
            misc->player_pid = play_track (misc, misc->sound_dev, "pulseaudio",
                        misc->lsn_cursor);
            break;
         } // if
         skip_left (misc, my_attribute, daisy);
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
         pause_resume (misc, my_attribute, daisy);
         misc->speed += 0.1;
         pause_resume (misc, my_attribute, daisy);
         view_screen (misc, daisy);
         break;
      case 'D':
      case '-':
         if (misc->speed <= 0.3)
         {
            beep ();
            break;
         } // if
         pause_resume (misc, my_attribute, daisy);
         misc->speed -= 0.1;
         pause_resume (misc, my_attribute, daisy);
         view_screen (misc, daisy);
         break;
      case KEY_HOME:
      case '*':
         pause_resume (misc, my_attribute, daisy);
         misc->speed = 1;
         pause_resume (misc, my_attribute, daisy);
         view_screen (misc, daisy);
         break;
      case 'v':
      case '1':
         get_volume (misc);
         if (misc->volume <= misc->min_vol)
         {
            beep ();
            break;
         } // if
         misc->volume -= 1;
         set_volume (misc);
         break;
      case 'V':
      case '7':
         get_volume (misc);
         if (misc->volume >= misc->max_vol)
         {
            beep ();
            break;
         } // if
         misc->volume += 1;
         set_volume (misc);
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
            get_next_clips (misc, my_attribute, daisy);
            start_playing (misc, daisy);
            view_screen (misc, daisy);
         } //   if
         view_time (misc, daisy);
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
               quit_daisy_player (misc, daisy);
               len = strlen (pw->pw_dir) +
                     strlen (misc->bookmark_title) +
                     strlen (get_mcn (misc)) + 100;
               str = malloc (len);
               snprintf (str, len, "%s/.daisy-player/%s%s",
                         pw->pw_dir, misc->bookmark_title, get_mcn (misc));
               unlink (str);
               _exit (0);
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
   } // for
} // browse

void usage (int e)
{
   printf (gettext ("Daisy-player - Version %s %s"), PACKAGE_VERSION, "\n");
   puts ("(C)2003-2018 J. Lemmens\n");
   printf (gettext
    ("Usage: %s [directory_with_a_Daisy-structure] | [Daisy_book_archive]"),
    PACKAGE);
   printf ("\n%s ", gettext ("[-c cdrom_device] [-d pulseaudio_sound_device]"));
   printf ("[-h] [-i] [-n | -y]\n");
   fflush (stdout);
   _exit (e);
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
   if (strcasestr (str, "iso9660") ||
       strcasestr (str, "udf"))
   {
      misc->daisy_mp = strdup (strchr (str, ' ') + 1);
      *strchr (misc->daisy_mp, ' ') = 0;
      return misc->daisy_mp;
   } // if
   return NULL;
} // get_mount_point

void handle_discinfo (misc_t *misc, my_attribute_t *my_attribute,
                      daisy_t *daisy, char *discinfo_html)
{
   int h, m, s;
   float t = 0;
   xmlTextReaderPtr di, ncc;
   htmlDocPtr doc;

   doc = htmlParseFile (discinfo_html, "UTF-8");
   if (! (di = xmlReaderWalker (doc)))
   {
      int e;
      char str[MAX_STR];

      e = errno;
      snprintf (str, MAX_STR, gettext ("Cannot read %s"), discinfo_html);
      failure (misc, str, e);
   } // if (! (di = xmlReaderWalker (doc)
   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, di))
         break;
      if (strcasecmp (misc->tag, "title") == 0)
      {
         do
         {
            if (! get_tag_or_label (misc, my_attribute, di))
               break;
         } while ( !*misc->label);
         strncpy (misc->daisy_title, misc->label, MAX_STR - 1);
      } // if (strcasecmp (misc->tag, "title") == 0)
      if (strcasecmp (misc->tag, "a") == 0)
      {
         strncpy (daisy[misc->current].filename, my_attribute->href,
                  MAX_STR - 1);
         htmlDocPtr doc =
                     htmlParseFile (daisy[misc->current].filename, "UTF-8");
         if (! (ncc = xmlReaderWalker (doc)))
         {
            int e;
            char str[MAX_STR];

            e = errno;
            snprintf (str, MAX_STR, 
               gettext ("Cannot read %s"), daisy[misc->current].filename);
            failure (misc, str, e);
         } // if
         do
         {
            *misc->ncc_totalTime = 0;
            if (! get_tag_or_label (misc, my_attribute, ncc))
               break;
         } while (! *misc->ncc_totalTime);
         daisy[misc->current].duration = read_time (misc->ncc_totalTime);
         t += daisy[misc->current].duration;
         xmlTextReaderClose (ncc);
         xmlFreeDoc (doc);
         do
         {
            if (! get_tag_or_label (misc, my_attribute, di))
               break;
         } while (! *misc->label);
         *daisy[misc->current].label = 0;
         strncpy (daisy[misc->current].daisy_mp,
                  dirname (daisy[misc->current].filename), MAX_STR - 1);
         daisy[misc->current].level = 1;
         daisy[misc->current].x = 0;
         daisy[misc->current].y = misc->displaying;
         daisy[misc->current].screen = misc->current / misc->max_y;
         misc->current++;
         misc->displaying++;
      } // if (strcasecmp (misc->tag, "a") == 0)
   } // while
   xmlTextReaderClose (di);
   xmlFreeDoc (doc);
   misc->total_items = misc->current;
   misc->total_time = t;
   h = t / 3600;
   t -= h * 3600;
   m = t / 60;
   t -= m * 60;
   s = t;
   snprintf (misc->ncc_totalTime, MAX_STR - 1, "%02d:%02d:%02d", h, m, s);
   misc->depth = 1;
   view_screen (misc, daisy);
} // handle_discinfo

int main (int argc, char *argv[])
{
   int opt;
   char str[MAX_STR], DISCINFO_HTML[MAX_STR], *start_wd;
   char *c_opt, *d_opt, cddb_opt;
   misc_t misc;
   my_attribute_t my_attribute;
   daisy_t *daisy;
   struct sigaction usr_action;

   daisy = NULL;
   misc.tmp_dir = misc.label = NULL;
   misc.speed = 1;
   misc.playing = misc.just_this_item = -1;
   misc.discinfo = 0;
   misc.cd_type = -1;
   misc.ignore_bookmark = 0;
   *misc.bookmark_title = 0;
   misc.current_id = strdup ("");
   misc.prev_id = misc.audio_id = strdup ("");
   misc.total_time = 0;
   *misc.daisy_title = 0;
   *misc.ncc_html = 0;
   strncpy (misc.cd_dev, "/dev/sr0", MAX_STR - 1);
   usr_action.sa_handler = player_ended;
   usr_action.sa_flags = 0;
   sigaction (SIGCHLD, &usr_action, NULL);
   *misc.xmlversion = 0;
// If option '-h' exit before load_xml ()
   make_tmp_dir (&misc);
   misc.sound_dev = strdup ("0");
   misc.cddb_flag = 'y';
   if (! setlocale (LC_ALL, ""))
      failure (&misc, "setlocale ()", errno);
   if (! setlocale (LC_NUMERIC, "C"))
      failure (&misc, "setlocale ()", errno);
   textdomain (PACKAGE);
   snprintf (str, MAX_STR, "%s/", LOCALEDIR);
   bindtextdomain (PACKAGE, str);
   start_wd = strdup (get_current_dir_name ());
   opterr = 0;
   misc.use_OPF = misc.use_NCX = 0;
   c_opt = d_opt = NULL;
   cddb_opt = 0;
   while ((opt = getopt (argc, argv, "c:d:hijnyON")) != -1)
   {                                           
      switch (opt)
      {
      case 'c':
         strncpy (misc.cd_dev, optarg, MAX_STR - 1);
         c_opt = strdup (misc.cd_dev);
         break;
      case 'd':
         misc.sound_dev = strdup (optarg);
         d_opt = strdup (misc.sound_dev);
         break;
      case 'h':
         free (start_wd);
         remove_tmp_dir (&misc);
         usage (0);
         break;
      case 'i':
         misc.ignore_bookmark = 1;
         break;
      case 'n':
         misc.cddb_flag = 'n';
         cddb_opt = misc.cddb_flag;
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
      case 'N':
         misc.use_NCX = 1;
         break;
      case 'O':
         misc.use_OPF = 1;
         break;
      default:
         beep ();             
         remove_tmp_dir (&misc);
         usage (1);
      } // switch
   } // while
   if (misc.ignore_bookmark == 0)
      load_xml (&misc, &my_attribute);
   if (c_opt)
      strncpy (misc.cd_dev, c_opt, MAX_STR - 1);
   if (d_opt)
      misc.sound_dev = strdup (d_opt);
   if (cddb_opt)
      misc.cddb_flag = cddb_opt;
   initscr ();
   if (! (misc.titlewin = newwin (2, 80,  0, 0)) ||
       ! (misc.screenwin = newwin (23, 80, 2, 0)))
      failure (&misc, "No curses", errno);
   fclose (stderr);
   getmaxyx (misc.screenwin, misc.max_y, misc.max_x);
   printw ("(C)2003-2018 J. Lemmens\n");
   printw (gettext ("Daisy-player - Version %s %s"), PACKAGE_VERSION, "");
   printw ("\n");
   printw (gettext ("A parser to play Daisy CD's with Linux"));
   printw ("\n");
   printw (gettext ("Scanning for a Daisy CD..."));
   refresh ();
   misc.total_pages = 0;

   if (argv[optind])
// if there is an argument
   {
// look if arg exists
      if (access (argv[optind], R_OK) == -1)
      {
         int e;

         e = errno;
         endwin ();
         printf (gettext ("Daisy-player - Version %s %s"),
                 PACKAGE_VERSION, "\n");
         puts ("(C)2003-2018 J. Lemmens");
         beep ();
         remove_tmp_dir (&misc);
         printf ("%s: %s\n", argv[optind], strerror (e));
         _exit (1);
      } // if

// determine filetype
      magic_t myt;

      myt = magic_open (MAGIC_CONTINUE | MAGIC_SYMLINK | MAGIC_DEVICES);
      magic_load (myt, NULL);
      if (magic_file (myt, argv[optind]) == NULL)
      {
         int e;

         e = errno;
         endwin ();
         printf ("%s: %s\n", argv[optind], strerror (e));
         beep ();
         fflush (stdout);
         remove_tmp_dir (&misc);
         usage (1);
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
            misc.daisy_mp = malloc (strlen (start_wd) +
                                    strlen (argv[optind]) + 5);
            strcpy (misc.daisy_mp, start_wd);
            strcat (misc.daisy_mp, "/");
            strcat (misc.daisy_mp, argv[optind]);
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
                   "/usr/bin/unar \"%s\" -o %s > /dev/null",
                   argv[optind], misc.tmp_dir);
         switch (system (misc.cmd));

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
            misc.daisy_mp =
                malloc (strlen (misc.tmp_dir) + strlen (dirent->d_name) + 5);
            sprintf (misc.daisy_mp, "%s/%s", misc.tmp_dir, dirent->d_name);
            entries++;
         } // while
         if (entries > 1)
            misc.daisy_mp = strdup (misc.tmp_dir);
         closedir (dir);
      } // if unar
      else
      {
         endwin ();
         printf ("\n%s\n", gettext ("No DAISY-CD or Audio-cd found"));
         beep ();
         remove_tmp_dir (&misc);
         usage (1);
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
         endwin ();
         printf (gettext ("Daisy-player - Version %s %s"),
                          PACKAGE_VERSION, "\n");
         puts ("(C)2003-2018 J. Lemmens");
         beep ();
         remove_tmp_dir (&misc);
         snprintf (misc.str, MAX_STR, gettext ("Cannot read %s"),
                   misc.cd_dev);
         printf ("\n%s: %s\n", misc.str, strerror (e));
         fflush (stdout);
         _exit (-1);
      } // if
      if (stat (misc.cd_dev, &buf) == -1)
         failure (&misc, misc.cd_dev, errno);
      if (((buf.st_mode & S_IFMT) == S_IFBLK) != 1)
      {
         endwin ();
         printf (gettext ("Daisy-player - Version %s %s"),
                          PACKAGE_VERSION, "\n");
         puts ("(C)2003-2018 J. Lemmens");
         beep ();
         remove_tmp_dir (&misc);
         printf ("\n%s is not a cd device\n", misc.cd_dev);
         fflush (stdout);
         _exit (-1);
      } // if
      snprintf (misc.cmd, MAX_CMD, "eject -tp %s", misc.cd_dev);
      switch (system (misc.cmd));
      start = time (NULL);

      misc.daisy_mp = strdup (misc.cd_dev);
      cd = NULL;
      cdio_init ();
      do
      {
         if (time (NULL) - start >= 60)
         {
            endwin ();
            printf ("%s\n", gettext ("No Daisy CD in drive."));
            remove_tmp_dir (&misc);
            _exit (0);
         } // if
         cd = cdio_open (misc.cd_dev, DRIVER_UNKNOWN);
      } while (cd == NULL);
      start = time (NULL);
      do
      {
         if (time (NULL) - start >= 20)
         {
            endwin ();
            printf ("%s\n", gettext ("No Daisy CD in drive."));
            remove_tmp_dir (&misc);
            _exit (0);
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
            endwin ();
            do
// if not found a mounted cd, try to mount one
            {
               if (time (NULL) - start >= 10)
               {
                  endwin ();
                  printf ("%s\n", gettext ("No Daisy CD in drive."));
                  remove_tmp_dir (&misc);
                  _exit (0);
               } // if
               snprintf (misc.cmd, MAX_CMD,
                         "udisksctl mount -b %s > /dev/null", misc.cd_dev);
               switch (system (misc.cmd));
               get_mount_point (&misc);
            } while (! get_mount_point (&misc));
            misc.titlewin = newwin (2, 80,  0, 0);
            misc.screenwin = newwin (23, 80, 2, 0);
            break;
         } // TRACK_COUNT_DATA"
         case CDIO_DISC_MODE_CD_DA: /**< CD-DA */
         {
// probably an Audio-CD
            printw ("\n%s ", gettext ("Found an Audio-CD."));
            if (misc.cddb_flag == 'y')
               printw (gettext ("Get titles from freedb.freedb.org..."));
            refresh ();
            strncpy (misc.bookmark_title, "Audio-CD", MAX_STR - 1);
            strncpy (misc.daisy_title, "Audio-CD", MAX_STR - 1);
            init_paranoia (&misc);
            daisy = get_number_of_tracks (&misc);
            get_toc_audiocd (&misc, daisy);
            misc.daisy_mp = strdup ("/tmp");
            break;
         } //  TRACK_COUNT_AUDIO
         case CDIO_DISC_MODE_CD_I:
         case CDIO_DISC_MODE_DVD_OTHER:
         case CDIO_DISC_MODE_NO_INFO:
         case CDIO_DISC_MODE_ERROR:
            endwin ();
            printf ("%s\n", gettext ("No DAISY-CD or Audio-cd found"));
            remove_tmp_dir (&misc);
            return 0;
         } // switch
      } while (misc.cd_type == -1);
   } // if use misc.cd_dev
   keypad (misc.screenwin, TRUE);
   meta (misc.screenwin, TRUE);
   nonl ();
   noecho ();
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
   if (misc.cd_type != CDIO_DISC_MODE_CD_DA)
   {
      daisy = create_daisy_struct (&misc, &my_attribute, daisy);
      snprintf (DISCINFO_HTML, MAX_STR - 1, "discinfo.html");
      if (access (DISCINFO_HTML, R_OK) == 0)
         handle_discinfo (&misc, &my_attribute, daisy, DISCINFO_HTML);
      if (! misc.discinfo)
      {
         misc.has_audio_tag = 0;
         if (access (misc.ncc_html, R_OK) == 0)
         {
// this is daisy2
            htmlDocPtr doc;

            doc = htmlParseFile (misc.ncc_html, "UTF-8");
            if (! (misc.reader = xmlReaderWalker (doc)))
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
               if (! get_tag_or_label (&misc, &my_attribute, misc.reader))
                  break;
               if (strcasecmp (misc.tag, "title") == 0)
               {
                  if (! get_tag_or_label (&misc, &my_attribute, misc.reader))
                     break;
                  if (*misc.label)
                  {
                     strncpy (misc.bookmark_title, misc.label, MAX_STR - 1);
                     break;
                  } // if
               } // if
            } // while
            fill_daisy_struct_2 (&misc, &my_attribute, daisy);
         }
         else
         {
// this is daisy3
            int i;

            strncpy (misc.daisy_version, "3", 2);
            read_daisy_3 (&misc, &my_attribute, daisy);
            for (i = 0; i < misc.total_items - 1; i++)
            {
               htmlDocPtr doc;
               xmlTextReaderPtr last;

               if (! (doc = htmlParseFile (daisy[i].xml_file, "UTF-8")))
                  failure (&misc, daisy[i].xml_file, errno);
               if (! (last = xmlReaderWalker (doc)))
                  failure (&misc, daisy[i].xml_file, errno);
               while (1)
               {
                  if (! get_tag_or_label (&misc, &my_attribute, last))
                     break;
                  if (strcasecmp (my_attribute.id, daisy[i + 1].anchor) == 0)
                     break;
                  if (*my_attribute.id)
                     strcpy (daisy[i].last_id, my_attribute.id);
               } // while
               xmlTextReaderClose (last);
               xmlFreeDoc (doc);
            } // for
            fill_page_numbers (&misc, daisy, &my_attribute);
            calculate_times_3 (&misc, &my_attribute, daisy);
         } // if
         if (misc.total_items == 0)
            misc.total_items = 1;
      } // if (! misc.discinfo);
   } // if misc.audiocd == 0
   if (*misc.bookmark_title == 0)
   {
      strncpy (misc.bookmark_title, misc.daisy_title, MAX_STR - 1);
      if (strchr (misc.bookmark_title, '/'))
      {
         int i = 0;

         while (misc.bookmark_title[i] != 0)
            misc.bookmark_title[i++] = '-';
      } // if
   } // if

   wattron (misc.titlewin, A_BOLD);
   snprintf (str, MAX_STR - 1, gettext (
        "Daisy-player - Version %s - (C)2018 J. Lemmens"), PACKAGE_VERSION);
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
   misc.level = 1;
   misc.search_str = misc.path_name = strdup ("");
   snprintf (misc.tmp_wav, MAX_STR, "%s/daisy-player.wav", misc.tmp_dir);
   if ((misc.tmp_wav_fd = mkstemp (misc.tmp_wav)) == 01)
      failure (&misc, "mkstemp ()", errno);
   browse (&misc, &my_attribute, daisy, start_wd);
   return 0;
} // main
