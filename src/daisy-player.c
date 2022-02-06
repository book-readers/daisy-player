/* daisy-player - A parser to play Daisy cd's.
 *
 *  Copyright (C)2003-2014 J. Lemmens
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

#define DAISY_PLAYER_VERSION "8.5.1"

int discinfo = 0, displaying = 0, phrase_nr, tts_no;
int playing, just_this_item, current_playorder = 1, audiocd;
int current_page_number, ignore_ncx, sector_ptr;
float speed, total_time;;
WINDOW *screenwin, *titlewin;
xmlTextReaderPtr reader;
char label[max_phrase_len], bookmark_title[MAX_STR], search_str[MAX_STR];
char tag[MAX_TAG], element[MAX_TAG];
char daisy_version[MAX_STR], cddb_flag;
pid_t player_pid, daisy_player_pid;
float clip_begin, clip_end;
char daisy_title[MAX_STR], prog_name[MAX_STR], daisy_language[MAX_STR];
int current, max_y, max_x, total_items, level, displaying;
char NCC_HTML[MAX_STR], ncc_totalTime[MAX_STR];
char sound_dev[MAX_STR], daisy_mp[MAX_STR], cd_dev[MAX_STR];
char ncx_name[MAX_STR], opf_name[MAX_STR];
char current_audio_file[MAX_STR];
time_t seconds;
float start_time;
float read_time (char *);
daisy_t daisy[2000];
my_attribute_t my_attribute;
int depth, total_pages;
int pipefd[2], tmp_wav_fd;
char tmp_wav[MAX_STR + 1];
cdrom_paranoia_t *par;
cdrom_drive_t *drv;
CdIo_t *p_cdio;
lsn_t lsn_cursor;

void parse_smil_3 ();
int get_tag_or_label (xmlTextReaderPtr);
void get_clips (char *, char *);
void save_rc ();
void quit_daisy_player ();
void get_page_number_3 (xmlTextReaderPtr);
void open_smil_file (char *, char *);
void read_daisy_3 (char *);
void get_toc_audiocd (char *);
pid_t play_track (char *, char *, char *, lsn_t, float);
char *get_mcn ();
void set_drive_speed (int);
void init_paranoia (char *);
void search (int , char);
void skip_left ();

void playfile (char *in_file, char *in_type,
               char *out_file, char *out_type, char *tempo)
{
   sox_format_t *sox_in, *sox_out;
   sox_effects_chain_t *chain;
   sox_effect_t *e;
   char *args[MAX_STR], str[MAX_STR];

   sox_globals.verbosity = 0;
   sox_globals.stdout_in_use_by = NULL;
   sox_init ();
   if ((sox_in = sox_open_read (in_file, NULL, NULL, in_type)) == NULL)
   {
      int e;

      e = errno;
      endwin ();
      printf ("sox_open_read: %s: %s\n", in_file, strerror (e));
      fflush (stdout);
      beep ();
      kill (getppid (), SIGQUIT);
   } // if
   if ((sox_out = sox_open_write (out_file, &sox_in->signal,
           NULL, out_type, NULL, NULL)) == NULL)
   {
      endwin ();
      printf ("%s: %s\n", out_file, gettext (strerror (EINVAL)));
      strncpy (sound_dev, "hw:0", MAX_STR - 1);
      save_rc ();
      beep ();
      fflush (stdout);
      kill (getppid (), SIGQUIT);
   } // if
   if (strcmp (in_type, "cdda") == 0)
   {
      sox_in->encoding.encoding = SOX_ENCODING_SIGN2;
      sox_in->encoding.bits_per_sample = 16;

      int a, b;

      a = SOX_LIB_VERSION_CODE >> 16;
      b = (SOX_LIB_VERSION_CODE - (a << 16)) >> 8;
      if (a >= 14 && b >= 4)
         sox_in->encoding.reverse_bytes = sox_option_no;
// libSoX version < 14.4 use:
//         sox_in->encoding.reverse_bytes = SOX_OPTION_NO;
   } // if

   chain = sox_create_effects_chain (&sox_in->encoding, &sox_out->encoding);

   e = sox_create_effect (sox_find_effect ("input"));
   args[0] = (char *) sox_in, sox_effect_options (e, 1, args);
   sox_add_effect (chain, e, &sox_in->signal, &sox_in->signal);

/* Don't use the sox trim effect. It works nice, but is far too slow
   char str2[MAX_STR];
   snprintf (str,  MAX_STR - 1, "%f", clip_begin);
   snprintf (str2, MAX_STR - 1, "%f", clip_end - clip_begin);
   e = sox_create_effect (sox_find_effect ("trim"));
   args[0] = str;
   args[1] = str2;
   sox_effect_options (e, 2, args);
   sox_add_effect (chain, e, &sox_in->signal, &sox_in->signal);
*/

   e = sox_create_effect (sox_find_effect ("tempo"));
   args[0] = tempo, sox_effect_options (e, 1, args);
   sox_add_effect (chain, e, &sox_in->signal, &sox_in->signal);

   snprintf (str, MAX_STR - 1, "%lf", sox_out->signal.rate);
   e = sox_create_effect (sox_find_effect ("rate"));
   args[0] = str, sox_effect_options (e, 1, args);
   sox_add_effect (chain, e, &sox_in->signal, &sox_in->signal);

   snprintf (str, MAX_STR - 1, "%i", sox_out->signal.channels);
   e = sox_create_effect (sox_find_effect ("channels"));
   args[0] = str, sox_effect_options (e, 1, args);
   sox_add_effect (chain, e, &sox_in->signal, &sox_in->signal);

   e = sox_create_effect (sox_find_effect ("output"));
   args[0] = (char *) sox_out, sox_effect_options (e, 1, args);
   sox_add_effect (chain, e, &sox_in->signal, &sox_out->signal);

   sox_flow_effects (chain, NULL, NULL);
   sox_delete_effects_chain (chain);
   sox_close (sox_out);
   sox_close (sox_in);
   sox_quit ();
} // playfile

void put_bookmark ()
{
   char str[MAX_STR];
   xmlTextWriterPtr writer;
   struct passwd *pw;;

   pw = getpwuid (geteuid ());
   snprintf (str, MAX_STR - 1, "%s/.daisy-player", pw->pw_dir);
   mkdir (str, 0755);
   snprintf (str, MAX_STR - 1, "%s/.daisy-player/%s%s",
             pw->pw_dir, bookmark_title, get_mcn ());
   if (! (writer = xmlNewTextWriterFilename (str, 0)))
      return;
   xmlTextWriterStartDocument (writer, NULL, NULL, NULL);
   xmlTextWriterStartElement (writer, BAD_CAST "bookmark");
   if (playing >= 0)
      xmlTextWriterWriteFormatAttribute
         (writer, BAD_CAST "item", "%d", playing);
   else
      xmlTextWriterWriteFormatAttribute
         (writer, BAD_CAST "item", "%d", current);
   if (audiocd == 0)
      xmlTextWriterWriteFormatAttribute
         (writer, BAD_CAST "clip-begin", "%f", clip_begin);
   else
      xmlTextWriterWriteFormatAttribute
         (writer, BAD_CAST "seconds", "%d", (int) (time (NULL) - seconds));
   xmlTextWriterWriteFormatAttribute (writer, BAD_CAST "level", "%d", level);
   xmlTextWriterEndElement (writer);
   xmlTextWriterEndDocument (writer);
   xmlFreeTextWriter (writer);
} // put_bookmark

void get_bookmark ()
{
   char str[MAX_STR];
   float begin;
   xmlTextReaderPtr local;
   xmlDocPtr doc;
   struct passwd *pw;;

   pw = getpwuid (geteuid ());
   if (! *bookmark_title)
      return;
   snprintf (str, MAX_STR - 1, "%s/.daisy-player/%s%s",
             pw->pw_dir, bookmark_title, get_mcn ());
   doc = xmlRecoverFile (str);
   if (! (local = xmlReaderWalker (doc)))
   {
      xmlTextReaderClose (local);
      xmlFreeDoc (doc);
      return;
   } // if
   do
   {
      if (! get_tag_or_label (local))
         break;
   } while (strcasecmp (tag, "bookmark") != 0);
   xmlTextReaderClose (local);
   xmlFreeDoc (doc);
   if (current <= 0)
      current = 0;
   if (current >= total_items)
      current = 0;
   displaying = playing = current;
   if (audiocd == 1)
   {
      player_pid = play_track (cd_dev, sound_dev, "alsa",
            daisy[current].first_lsn + (seconds * 75), speed);
      seconds = time (NULL) - seconds;
      return;
   } // if
   get_clips (my_attribute.clip_begin, my_attribute.clip_end);
   begin = clip_begin;
   open_smil_file (daisy[current].smil_file, daisy[current].anchor);
   while (1)
   {
      if (! get_tag_or_label (reader))
         return;
      if (strcasecmp (tag, "audio") == 0)
      {
         strncpy (current_audio_file, my_attribute.src, MAX_STR - 1);
         get_clips (my_attribute.clip_begin, my_attribute.clip_end);
         if (clip_begin == begin)
            break;
      } // if
   } // while
   if (level < 1)
      level = 1;
   current_page_number = daisy[playing].page_number;
   just_this_item = -1;
   play_now ();
} // get_bookmark

void get_page_number_2 (char *p)
{
// function for daisy 2.02
   char *anchor = 0, *id1, *id2;
   xmlTextReaderPtr page;
   xmlDocPtr doc;
   
   doc = xmlRecoverFile (daisy[playing].smil_file);
   if (! (page = xmlReaderWalker (doc)))
   {
      endwin ();
      beep ();
      printf (gettext ("\nCannot read %s\n"), daisy[playing].smil_file);
      fflush (stdout);
      _exit (1);
   } // if
   id1 = strdup (p);
   do
   {
      if (! get_tag_or_label (page))
      {
         free (id1);
         return;
      } // if
   } while (strcasecmp (my_attribute.id, id1) != 0);
   do
   {
      if (! get_tag_or_label (page))
         return;
   } while (strcasecmp (tag, "text") != 0);
   id2 = strdup (my_attribute.id);
   xmlTextReaderClose (page);
   xmlFreeDoc (doc);
   doc = xmlRecoverFile (NCC_HTML);
   if (! (page = xmlReaderWalker (doc)))
   {
      endwin ();
      beep ();
      printf (gettext ("\nCannot read %s\n"), NCC_HTML);
      fflush (stdout);
      _exit (1);
   } // if
   while (1)
   {
      if (! get_tag_or_label (page))
         return;
      if (strcasecmp (tag, "a") == 0)
      {
         if (strchr (my_attribute.href, '#'))
            anchor = strdup (strchr (my_attribute.href, '#') + 1);
         if (strcasecmp (anchor, id2) == 0)
            break;
      } // if
   } // while
   do
   {
      if (! get_tag_or_label (page))
         return;
   } while (! *label);
   xmlTextReaderClose (page);
   xmlFreeDoc (doc);
   current_page_number = atoi (label);
} // get_page_number_2

int get_next_clips ()
{
   while (1)
   {
      if (! get_tag_or_label (reader))
         return 0;
      if (strcasecmp (my_attribute.id, daisy[playing + 1].anchor) == 0 &&
          playing + 1 != total_items)
         return 0;
      if (strcasecmp (my_attribute.class, "pagenum") == 0)
         if (strcasestr (daisy_version, "3"))
            get_page_number_3 (reader);
      if (strcasecmp (tag, "audio") == 0)
      {
         strncpy (current_audio_file, my_attribute.src, MAX_STR - 1);
         get_clips (my_attribute.clip_begin, my_attribute.clip_end);
         return 1;
      } // if (strcasecmp (tag, "audio") == 0)
   } // while
} // get_next_clips

int compare_playorder (const void *p1, const void *p2)
{
   return (*(int *)p1 - *(int*)p2);
} // compare_playorder

void parse_smil_2 ()
{
// function for daisy 2.02
   int x;
   xmlTextReaderPtr parse;

   total_time = 0;
   for (x = 0; x < total_items; x++)
   {
      if (*daisy[x].smil_file == 0)
         continue;
      xmlDocPtr doc = xmlRecoverFile (daisy[x].smil_file);
      if (! (parse = xmlReaderWalker (doc)))
      {
         endwin ();
         beep ();
         printf (gettext ("\nCannot read %s\n"), daisy[x].smil_file);
         fflush (stdout);
         _exit (1);
      } // if

// parse this smil
      do
      {
         if (! get_tag_or_label (parse))
            break;
      } while (strcasecmp (my_attribute.id, daisy[x].anchor) != 0);
      daisy[x].duration = 0;
      while (1)
      {
         if (! get_tag_or_label (parse))
            break;
         if (strcasecmp (tag, "audio") == 0)
         {
            strncpy (current_audio_file, my_attribute.src, MAX_STR - 1);
            get_clips (my_attribute.clip_begin, my_attribute.clip_end);
            daisy[x].begin = clip_begin;
            daisy[x].duration += clip_end - clip_begin;
            while (1)
            {
               if (! get_tag_or_label (parse))
                  break;
               if (*daisy[x + 1].anchor)
                  if (strcasecmp (my_attribute.id, daisy[x + 1].anchor) == 0)
                     break;
               if (strcasecmp (tag, "audio") == 0)
               {
                  get_clips (my_attribute.clip_begin, my_attribute.clip_end);
                  daisy[x].duration += clip_end - clip_begin;
               } // if (strcasecmp (tag, "audio") == 0)
            } // while
            if (*daisy[x + 1].anchor)
               if (strcasecmp (my_attribute.id, daisy[x + 1].anchor) == 0)
                  break;
         } // if (strcasecmp (tag, "audio") == 0)
      } // while
      xmlTextReaderClose (parse);
      xmlFreeDoc (doc);
      total_time += daisy[x].duration;
   } // for
} // parse_smil_2    

void view_page (char *id)
{
   if (playing == -1)
      return;
   if (daisy[playing].screen != daisy[current].screen)
      return;
   if (total_pages == 0)
      return;
   wattron (screenwin, A_BOLD);
   if (strcasestr (daisy_version, "2.02"))
      get_page_number_2 (id);
   if (current_page_number)
      mvwprintw (screenwin, daisy[playing].y, 62, "(%3d)",
                 current_page_number);
   else
      mvwprintw (screenwin, daisy[playing].y, 62, "     ");
   wattroff (screenwin, A_BOLD);
   wmove (screenwin, daisy[current].y, daisy[current].x);
   wrefresh (screenwin);
} // view_page

void view_time ()
{
   float remain_time = 0, curr_time = 0;

   if (playing == -1)
      return;
   wattron (screenwin, A_BOLD);
   if (audiocd == 0)
      curr_time = start_time + (float) (time (NULL) - seconds);
   if (audiocd == 1)
      curr_time = (lsn_cursor - daisy[playing].first_lsn) / 75;
   mvwprintw (screenwin, daisy[playing].y, 68, "%02d:%02d",
              (int) curr_time / 60, (int) curr_time % 60);
   if (audiocd == 0)
      remain_time =  daisy[playing].duration - curr_time;
   if (audiocd == 1)
      remain_time =  (daisy[playing].last_lsn - daisy[playing].first_lsn) / 75
                     - curr_time;
   mvwprintw (screenwin, daisy[playing].y, 74, "%02d:%02d",
              (int) remain_time / 60, (int) remain_time % 60);
   wattroff (screenwin, A_BOLD);
   wmove (screenwin, daisy[current].y, daisy[current].x);
   wrefresh (screenwin);
} // view_time

void view_screen ()
{
   int i, x, x2;

   mvwaddstr (titlewin, 1, 0, "----------------------------------------");
   waddstr (titlewin, "----------------------------------------");
   mvwprintw (titlewin, 1, 0, gettext ("'h' for help -"));
   if (total_pages)
      wprintw (titlewin, gettext (" %d pages "), total_pages);
   mvwprintw (titlewin, 1, 28, gettext (" level: %d of %d "), level, depth);
   int hours   = total_time / 3600;
   int minutes = (total_time - hours * 3600) / 60;
   int seconds = total_time - (hours * 3600 + minutes * 60);
   mvwprintw (titlewin, 1, 47, gettext (" total length: %02d:%02d:%02d "),
              hours, minutes, seconds);
   mvwprintw (titlewin, 1, 74, " %d/%d ",
              daisy[current].screen + 1, daisy[total_items - 1].screen + 1);
   wrefresh (titlewin);
   wclear (screenwin);
   for (i = 0; daisy[i].screen != daisy[current].screen; i++);
   do
   {
      mvwprintw (screenwin, daisy[i].y, daisy[i].x + 1, daisy[i].label);
      x = strlen (daisy[i].label) + daisy[i].x;
      if (x / 2 * 2 != x)
         waddstr (screenwin, " ");
      for (x2 = x; x2 < 59; x2 = x2 + 2)
         waddstr (screenwin, " .");
      if (daisy[i].page_number)
         mvwprintw (screenwin, daisy[i].y, 62, "(%3d)", daisy[i].page_number);
      if (daisy[i].level <= level)
      {
         int x = i, dur = 0;

         do
         {
            dur += daisy[x].duration;
         } while (daisy[++x].level > level);
         mvwprintw (screenwin, daisy[i].y, 74, "%02d:%02d",
                    (int) (dur + .5) / 60, (int) (dur + .5) % 60);
      } // if
      if (i >= total_items - 1)
         break;
   } while (daisy[++i].screen == daisy[current].screen);
   if (just_this_item != -1 && daisy[current].screen == daisy[playing].screen)
      mvwprintw (screenwin, daisy[displaying].y, 0, "J");
   wmove (screenwin, daisy[current].y, daisy[current].x);
   wrefresh (screenwin);
} // view_screen

void read_daisy_2 ()
{
// function for daisy 2.02
   int indent = 0, found_page_normal = 0;
   xmlTextReaderPtr ncc;

   xmlDocPtr doc = xmlRecoverFile (NCC_HTML);
   if (! (ncc = xmlReaderWalker (doc)))
   {
      endwin ();
      beep ();
      printf (gettext ("\nCannot read %s\n"), NCC_HTML);
      fflush (stdout);
      _exit (1);
   } // if
   current = displaying = 0;
   while (1)
   {
      if (! get_tag_or_label (ncc))
         break;
      if (strcasecmp (tag, "h1") == 0 ||
          strcasecmp (tag, "h2") == 0 ||
          strcasecmp (tag, "h3") == 0 ||
          strcasecmp (tag, "h4") == 0 ||
          strcasecmp (tag, "h5") == 0 ||
          strcasecmp (tag, "h6") == 0)
      {
         int l;

         found_page_normal = 0;
         daisy[current].page_number = 0;
         l = tag[1] - '0';
         daisy[current].level = l;
         daisy[current].x = l + 3 - 1;
         indent = daisy[current].x = (l - 1) * 3 + 1;
         do
         {
            if (! get_tag_or_label (ncc))
               break;
         } while (strcasecmp (tag, "a") != 0);
         strncpy (daisy[current].smil_file, my_attribute.href, MAX_STR - 1);
         if (strchr (daisy[current].smil_file, '#'))
         {
            strncpy (daisy[current].anchor,
                     strchr (daisy[current].smil_file, '#') + 1, MAX_STR - 1);
            *strchr (daisy[current].smil_file, '#') = 0;
         } // if
         do
         {
            if (! get_tag_or_label (ncc))
               break;
         } while (*label == 0);
         get_label (current, indent);
         current++;
         displaying++;
      } // if (strcasecmp (tag, "h1") == 0 || ...
      if (strcasecmp (my_attribute.class, "page-normal") == 0 &&
          found_page_normal == 0)
      {
         found_page_normal = 1;
         do
         {
            if (! get_tag_or_label (ncc))
               break;
         } while (*label == 0);
         daisy[current - 1].page_number = atoi (label);
      } // if (strcasecmp (my_attribute.class, "page-normal")
   } // while
   xmlTextReaderClose (ncc);
   xmlFreeDoc (doc);
   total_items = current;
   parse_smil_2 ();
} // read_daisy_2

void player_ended ()
{
   wait (NULL);
} // player_ended

void play_now ()
{
   if (playing == -1)
      return;

   seconds = time (NULL);
   start_time = clip_begin;
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

   char tempo_str[15], cmd[MAX_CMD];
   setsid ();

   view_page (my_attribute.id);
   lseek (tmp_wav_fd, SEEK_SET, 0);
   snprintf (cmd, MAX_CMD - 1, "madplay -Q %s -s %f -t %f -o wav:\"%s\"",
                  current_audio_file, clip_begin,
                  clip_end - clip_begin, tmp_wav);
   if (system (cmd) != 0)
      _exit (0);
   snprintf (tempo_str, 10, "%lf", speed);
   playfile (tmp_wav, "wav", sound_dev, "alsa", tempo_str);
   _exit (0);
} // play_now

void open_smil_file (char *smil_file, char *anchor)
{
   if (audiocd == 1)
      return;
   xmlTextReaderClose (reader);
   xmlDocPtr doc = xmlRecoverFile (smil_file);
   if (! (reader = xmlReaderWalker (doc)))
   {
      endwin ();
      printf ("open_smil_file(): %s: %s\n", smil_file, strerror (errno));
      beep ();
      fflush (stdout);
      _exit (1);
   } // if

// skip to anchor
   while (1)
   {
      if (! get_tag_or_label (reader))
         break;;
      if (strcasecmp (my_attribute.id, anchor) == 0)
         break;
   } // while
} // open_smil_file

void pause_resume ()
{        
   if (playing > -1)
   {
      playing = -1;
      if (audiocd == 0)
      {
         kill (player_pid, SIGKILL);
         player_pid = -2;
      }
      else
      {
         kill (player_pid, SIGKILL);
      } // if
   }
   else
   {
      playing = displaying;
      if (audiocd == 0)
      {
         skip_left ();
         skip_right ();
      }
      else
      {
         lsn_t start;

         start = (lsn_t) (start_time + (float) (time (NULL) - seconds) * 75) +
                 daisy[playing].first_lsn;
         player_pid = play_track (cd_dev, sound_dev, "alsa",
                            start, speed);
      } // if
   } // if
} // pause_resume

void write_wav (char *outfile)
{
   char str[MAX_STR];
   struct passwd *pw;

   pw = getpwuid (geteuid ());
   snprintf (str, MAX_STR - 1, "%s/%s", pw->pw_dir, outfile);

   set_drive_speed (100); // fastest
   if (audiocd == 1)
   {
      pid_t pid;
      int16_t *p_readbuf;

      pid = play_track (cd_dev, str, "wav",
                        daisy[current].first_lsn, 1);
      do
      {
         int ret;

         if (! (p_readbuf = paranoia_read (par, NULL)))
            break;
         ret = write (pipefd[1], p_readbuf, CDIO_CD_FRAMESIZE_RAW);
         ret = ret;
      } while (++lsn_cursor <= daisy[current].last_lsn);
      kill (pid, SIGQUIT);
      set_drive_speed (4);
      return;
   } // if

   char cmd[MAX_CMD];

   lseek (tmp_wav_fd, SEEK_SET, 0);
   snprintf (cmd, MAX_CMD, "madplay -Q \"%s\" -s %f -t %f -o wav:\"%s\"",
             current_audio_file,
             daisy[current].begin, daisy[current].duration, str);
   if (system (cmd) != 0)
      _exit (0);
   set_drive_speed (4);
} // write_wav

void store_to_disk ()
{
   char str[MAX_STR];
   int i, pause_flag;

   pause_flag = playing;
   if (pause_flag > -1)
      pause_resume ();
   wclear (screenwin);
   snprintf (str, MAX_STR - 1, "%s.wav", daisy[current].label);
   wprintw (screenwin,
            "\nStoring \"%s\" as \"%s\" into your home-folder...",
            daisy[current].label, str);
   wrefresh (screenwin);
   for (i = 0; str[i] != 0; i++)
      if (str[i] == '/')
         str[i] = '-';
   write_wav (str);
   if (pause_flag > -1)
      pause_resume ();
   view_screen ();
} // store_to_disk

void help ()
{
   int y, x;

   getyx (screenwin, y, x);
   wclear (screenwin);
   waddstr (screenwin, gettext ("\nThese commands are available in this version:\n"));
   waddstr (screenwin, "========================================");
   waddstr (screenwin, "========================================\n\n");
   waddstr (screenwin, gettext ("cursor down     - move cursor to the next item\n"));
   waddstr (screenwin, gettext ("cursor up       - move cursor to the previous item\n"));
   waddstr (screenwin, gettext ("cursor right    - skip to next phrase\n"));
   waddstr (screenwin, gettext ("cursor left     - skip to previous phrase\n"));
   waddstr (screenwin, gettext ("page-down       - view next page\n"));
   waddstr (screenwin, gettext ("page-up         - view previous page\n"));
   waddstr (screenwin, gettext ("enter           - start playing\n"));
   waddstr (screenwin, gettext ("space           - pause/resume playing\n"));
   waddstr (screenwin, gettext ("home            - play on normal speed\n"));
   waddstr (screenwin, "\n");
   waddstr (screenwin, gettext ("Press any key for next page..."));
   nodelay (screenwin, FALSE);
   wgetch (screenwin);
   nodelay (screenwin, TRUE);
   wclear (screenwin);
   waddstr (screenwin, gettext ("\n/               - search for a label\n"));
   waddstr (screenwin, gettext ("d               - store current item to disk\n"));
   waddstr (screenwin, gettext ("D               - decrease playing speed\n"));
   waddstr (screenwin, gettext (
"f               - find the currently playing item and place the cursor there\n"));
   if (audiocd == 0)
      waddstr (screenwin,
            gettext ("g               - go to page number (if any)\n"));
   else
      waddstr (screenwin,
            gettext ("g               - go to time in this song (MM:SS)\n"));
   waddstr (screenwin, gettext ("h or ?          - give this help\n"));
   waddstr (screenwin, gettext ("j               - just play current item\n"));
   waddstr (screenwin,
          gettext ("l               - switch to next level\n"));
   waddstr (screenwin, gettext ("L               - switch to previous level\n"));
   waddstr (screenwin, gettext ("n               - search forwards\n"));
   waddstr (screenwin, gettext ("N               - search backwards\n"));
   waddstr (screenwin, gettext ("o               - select next output sound device\n"));
   waddstr (screenwin, gettext ("p               - place a bookmark\n"));
   waddstr (screenwin, gettext ("q               - quit daisy-player and place a bookmark\n"));
   waddstr (screenwin, gettext ("s               - stop playing\n"));
   waddstr (screenwin, gettext ("U               - increase playing speed\n"));
   waddstr (screenwin, gettext ("\nPress any key to leave help..."));
   nodelay (screenwin, FALSE);
   wgetch (screenwin);
   nodelay (screenwin, TRUE);
   view_screen ();
   wmove (screenwin, y, x);
} // help

void previous_item ()
{
   if (current == 0)
      return;
   while (daisy[current].level > level)
      current--;
   if (playing == -1)
      displaying = current;
   view_screen ();
   wmove (screenwin, daisy[current].y, daisy[current].x);
} // previous_item

void next_item ()
{
   if (current >= total_items - 1)
   {
      beep ();
      return;
   } // if
   while (daisy[++current].level > level)
   {
      if (current >= total_items - 1)
      {
         beep ();
         previous_item ();
         return;
      } // if
   } // while
   view_screen ();
   wmove (screenwin, daisy[current].y, daisy[current].x);
} // next_item

void play_clip ()
{
   char str[MAX_STR];

   if (kill (player_pid, 0) == 0)
// if still playing
      return;
   player_pid = -2;
   if (get_next_clips ())
   {
      play_now ();
      return;
   } // if

// go to next item
   strncpy (my_attribute.clip_begin, "0", 2);
   clip_begin = 0;
   if (++playing >= total_items)
   {
      struct passwd *pw;

      pw = getpwuid (geteuid ());
      quit_daisy_player ();
      snprintf (str, MAX_STR - 1, "%s/.daisy-player/%s%s", pw->pw_dir,
                bookmark_title, get_mcn ());
      unlink (str);
      _exit (0);
   } // if
   if (daisy[playing].level <= level)
      displaying = current = playing;
   if (just_this_item != -1)
      if (daisy[playing].level <= level)
         playing = just_this_item = -1;
   if (playing > -1)
      open_smil_file (daisy[playing].smil_file, daisy[playing].anchor);
   view_screen ();
} // play_clip

void skip_left ()
{
   struct
   {
      char src[MAX_STR], clip_begin[MAX_STR], clip_end[MAX_STR];
   } curr, prev, orig;

   if (audiocd == 1)
      return;
   if (playing < 0)
   {
      beep ();
      return;
   } // if
   if (player_pid > 0)
      while (kill (player_pid, SIGKILL) != 0);
   if (audiocd == 1)
   {
      beep ();
      return;
   } // if
   get_clips (my_attribute.clip_begin, my_attribute.clip_end);
   if (daisy[playing].begin != clip_begin)
   {
// save current values in orig and curr arrays
      strncpy (orig.clip_begin, my_attribute.clip_begin, MAX_STR - 1);
      strncpy (orig.clip_end, my_attribute.clip_end, MAX_STR - 1);
      strncpy (orig.src, my_attribute.src, MAX_STR - 1);
      strncpy (curr.clip_begin, my_attribute.clip_begin, MAX_STR - 1);
      strncpy (curr.clip_end, my_attribute.clip_end, MAX_STR - 1);
      strncpy (curr.src, my_attribute.src, MAX_STR - 1);
      open_smil_file (daisy[playing].smil_file, daisy[playing].anchor);
      while (1)
      {
         if (! get_tag_or_label (reader))
            return;
         if (strcasecmp (my_attribute.class, "pagenum") == 0)
         {
            do
            {
               if (! get_tag_or_label (reader))
                  return;
            } while (strcasecmp (tag, "/par") != 0);
         } // if (strcasecmp (my_attribute.class, "pagenum") == 0)
         if (strcasecmp (tag, "audio") == 0)
         {
            strncpy (prev.clip_begin, curr.clip_begin, MAX_STR - 1);
            strncpy (prev.clip_end, curr.clip_end, MAX_STR - 1);
            strncpy (prev.src, curr.src, MAX_STR - 1);
            strncpy (curr.clip_begin, my_attribute.clip_begin, MAX_STR - 1);
            strncpy (curr.clip_end, my_attribute.clip_end, MAX_STR - 1);
            strncpy (curr.src, my_attribute.src, MAX_STR - 1);
            if (strcasecmp (orig.clip_begin, curr.clip_begin) == 0 &&
                strcasecmp (orig.src, curr.src) == 0)
            {
               open_smil_file (daisy[playing].smil_file, daisy[playing].anchor);
               while (1)
               {
                  if (! get_tag_or_label (reader))
                     return;
                  if (strcasecmp (tag , "audio") == 0)
                  {
                     if (strcasecmp (my_attribute.clip_begin,
                         prev.clip_begin) == 0 &&
                         strcasecmp (my_attribute.src, prev.src) == 0)
                     {
                        get_clips (my_attribute.clip_begin,
                                   my_attribute.clip_end);
                        play_now ();
                        return;
                     } // if (strcasecmp (my_attribute.clip_begin, prev.cli ...
                  } // if (strcasecmp (tag , "audio") == 0)
               } // while
            } // if (strcasecmp (orig.clip_begin, curr.clip_begin) == 0 &&
         } // if (strcasecmp (tag, "audio") == 0)
      } // while
   } // if (clip_begin != daisy[playing].begin)

// go to end of previous item
   current = displaying = --playing;
   if (current < 0)
   {
      beep ();
      current = displaying = playing = 0;
      return;
   } // if
   open_smil_file (daisy[current].smil_file, daisy[current].anchor);
   while (1)
   {
// search last clip
      if (! get_tag_or_label (reader))
         break;
      if (strcasecmp (my_attribute.class, "pagenum") == 0)
      {
         do
         {
            if (! get_tag_or_label (reader))
               return;
         } while (strcasecmp (tag, "/par") != 0);
      } // if (strcasecmp (my_attribute.class, "pagenum") == 0)
      if (strcasecmp (tag, "audio") == 0)
      {
         strncpy (curr.clip_begin, my_attribute.clip_begin, MAX_STR - 1);
         strncpy (curr.clip_end, my_attribute.clip_end, MAX_STR - 1);
         strncpy (curr.src, my_attribute.src, MAX_STR - 1);
      } // if (strcasecmp (tag, "audio") == 0)
      if (strcasecmp (my_attribute.id, daisy[current + 1].anchor) == 0)
         break;
   } // while
   just_this_item = -1;
   open_smil_file (daisy[current].smil_file, daisy[current].anchor);
   while (1)
   {
      if (! get_tag_or_label (reader))
         return;
      if (strcasecmp (tag , "audio") == 0)
      {
         if (strcasecmp (my_attribute.clip_begin, curr.clip_begin) == 0 &&
             strcasecmp (my_attribute.src, curr.src) == 0)
         {
            strncpy (current_audio_file, my_attribute.src, MAX_STR - 1);
            get_clips (my_attribute.clip_begin, my_attribute.clip_end);
            view_screen ();
            play_now ();
            return;
         } // if (strcasecmp (my_attribute.clip_begin, curr.clip_begin) == 0 &&
      } // if (strcasecmp (tag , "audio") == 0)
   } // while
} // skip_left

void skip_right ()
{
   if (audiocd == 1)
      return;
   if (playing == -1)
   {
      beep ();
      return; 
   } // if
   if (audiocd == 0)
   {
      kill (player_pid, SIGKILL);
      player_pid = -2;
   } // if
} // skip_right

void change_level (char key)
{
   int c, l;

   if (depth == 1)
      return;
   if (key == 'l')
      if (++level > depth)
         level = 1;
   if (key == 'L')
      if (--level < 1)
         level = depth;
   mvwprintw (titlewin, 1, 0, gettext ("Please wait... -------------------------"));
   wprintw (titlewin, "----------------------------------------");
   wrefresh (titlewin);
   c = current;
   l = level;
   if (strcasestr (daisy_version, "2.02"))
      read_daisy_2 (0);
   if (strcasestr (daisy_version, "3"))
   {
      read_daisy_3 (daisy_mp);
      parse_smil_3 ();
   } // if
   current = c;
   level = l;
   if (daisy[current].level > level)
      previous_item ();
   view_screen ();
   wmove (screenwin, daisy[current].y, daisy[current].x);
} // change_level

void read_rc ()
{
   char str[MAX_STR];
   struct passwd *pw;;
   xmlTextReaderPtr reader;
   xmlDocPtr doc;

   pw = getpwuid (geteuid ());
   snprintf (str, MAX_STR - 1, "%s/.daisy-player.rc", pw->pw_dir);
   doc = xmlRecoverFile (str);
   if (! (reader = xmlReaderWalker (doc)))
   {
      strncpy (sound_dev, "hw:0", MAX_STR - 1);
      return;
   } // if
   do
   {
      if (! get_tag_or_label (reader))
         break;
   } while (strcasecmp (tag, "prefs") != 0);
   xmlTextReaderClose (reader);
   xmlFreeDoc (doc);
   if (cddb_flag != 'n' && cddb_flag != 'y')
      cddb_flag = 'y';
} // read_rc

void save_rc ()
{
   struct passwd *pw;
   char str[MAX_STR];
   xmlTextWriterPtr writer;

   pw = getpwuid (geteuid ());
   snprintf (str, MAX_STR - 1, "%s/.daisy-player.rc", pw->pw_dir);
   if (! (writer = xmlNewTextWriterFilename (str, 0)))
      return;
   xmlTextWriterStartDocument (writer, NULL, NULL, NULL);
   xmlTextWriterStartElement (writer, BAD_CAST "prefs");
   xmlTextWriterWriteFormatAttribute
      (writer, BAD_CAST "sound_dev", "%s", sound_dev);
   xmlTextWriterWriteFormatAttribute (writer, BAD_CAST "speed", "%lf", speed);
   xmlTextWriterWriteFormatAttribute
      (writer, BAD_CAST "cd_dev", "%s", cd_dev);
   xmlTextWriterWriteFormatAttribute
      (writer, BAD_CAST "cddb_flag", "%c", cddb_flag);
   xmlTextWriterEndElement (writer);
   xmlTextWriterEndDocument (writer);
   xmlFreeTextWriter (writer);
} // save_rc

void quit_daisy_player ()
{
   endwin ();
   kill (player_pid, SIGKILL);
   player_pid = -2;
   put_bookmark ();
   save_rc ();

// reset the CD speed
   set_drive_speed (100);
   close (tmp_wav_fd);
   puts ("");
} // quit_daisy_player

void search (int start, char mode)
{
   int c, found = 0, flag = 0;

   if (playing != -1)
   {
      flag = 1;
      pause_resume ();
   } // if
   if (mode == '/')
   {
      mvwaddstr (titlewin, 1, 0, "----------------------------------------");
      waddstr (titlewin, "----------------------------------------");
      mvwprintw (titlewin, 1, 0, gettext ("What do you search? "));
      echo ();
      wgetnstr (titlewin, search_str, 25);
      noecho ();
   } // if
   if (mode == '/' || mode == 'n')
   {
      for (c = start; c <= total_items + 1; c++)
      {
         if (strcasestr (daisy[c].label, search_str))
         {
            found = 1;
            break;
         } // if
      } // for
      if (! found)
      {
         for (c = 0; c < start; c++)
         {
            if (strcasestr (daisy[c].label, search_str))
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
         if (strcasestr (daisy[c].label, search_str))
         {
            found = 1;
            break;
         } // if
      } // for
      if (! found)
      {
         for (c = total_items + 1; c > start; c--)
         {
            if (strcasestr (daisy[c].label, search_str))
            {
               found = 1;
               break;
            } // if
         } // for
      } // if
   } // if
   if (found)
   {
      playing = displaying= current = c;
      clip_begin = daisy[current].begin;
      just_this_item = -1;
      view_screen ();
      if (audiocd == 0)
         open_smil_file (daisy[current].smil_file, daisy[current].anchor);
      else
      {
         seconds = time (NULL);
         player_pid = play_track (cd_dev, sound_dev, "alsa",
                daisy[current].first_lsn, speed);
      } // if
   }
   else
   {
      beep ();
      view_screen ();
      if (! flag)
         return;
      playing = displaying;
      if (audiocd == 0)
      {
         skip_left ();
         skip_right ();
      }
      else
      {
         playing = -1;
         pause_resume ();
      } // if
   } // if
} // search

void go_to_page_number ()
{
   char filename[MAX_STR], *anchor = 0;
   char pn[15], href[MAX_STR], prev_href[MAX_STR];

   kill (player_pid, SIGKILL);
   player_pid = -2;
   mvwaddstr (titlewin, 1, 0, "----------------------------------------");
   waddstr (titlewin, "----------------------------------------");
   mvwprintw (titlewin, 1, 0, gettext ("Go to page number: "));
   echo ();
   wgetnstr (titlewin, pn, 5);
   noecho ();
   view_screen ();
   if (! *pn || atoi (pn) > total_pages || ! atoi (pn))
   {
      beep ();
      skip_left ();
      skip_right ();
      return;
   } // if
   if (strcasestr (daisy_version, "2.02"))
   {
      xmlTextReaderClose (reader);
      xmlDocPtr doc = xmlRecoverFile (NCC_HTML);
      if (! (reader = xmlReaderWalker (doc)))
      {
         endwin ();
         printf (gettext ("\nCannot read %s\n"), NCC_HTML);
         beep ();
         fflush (stdout);
         _exit (1);
      } // if
      *href = 0;
      while (1)
      {
         if (! get_tag_or_label (reader))
            return;
         if (strcasecmp (tag, "a") == 0)
         {
            strncpy (prev_href, href, MAX_STR - 1);
            strncpy (href, my_attribute.href, MAX_STR - 1);
            do
            {
               if (! get_tag_or_label (reader))
                  return;
            } while (! *label);
            if (strcasecmp (label, pn) == 0)
               break;
         } // if (strcasecmp (tag, "a") == 0)
      } // while
      if (*prev_href)
      {
         strncpy (filename, prev_href, MAX_STR - 1);
         if (strchr (filename, '#'))
         {
            anchor = strdup (strchr (filename, '#') + 1);
            *strchr (filename, '#') = 0;
         } // if
      } // if (*prev_href)
      for (current = total_items - 1; current >= 0; current--)
      {
         if (! daisy[current].page_number)
            continue;
         if (atoi (pn) >= daisy[current].page_number)
            break;
      } // for
      playing = displaying = current;
      view_screen ();
      current_page_number = atoi (pn);
      wmove (screenwin, daisy[current].y, daisy[current].x);
      just_this_item = -1;
      open_smil_file (daisy[current].smil_file, anchor);
      free (anchor);
      return;
   } // if (strcasestr (daisy_version, "2.02"))

   if (strcasestr (daisy_version, "3"))
   {
      char *anchor = 0;

      xmlTextReaderClose (reader);
      xmlDocPtr doc = xmlRecoverFile (NCC_HTML);
      if (! (reader = xmlReaderWalker (doc)))
      {
         endwin ();
         printf (gettext ("\nCannot read %s\n"), NCC_HTML);
         beep ();
         fflush (stdout);
         _exit (1);
      } // if
      do
      {
         if (! get_tag_or_label (reader))
         {
            xmlFreeDoc (doc);
            return;
         } // if
      } while (strcasecmp (tag, "pageTarget") != 0 ||
               strcasecmp (my_attribute.value, pn) != 0);
      do
      {
         if (! get_tag_or_label (reader))
         {
            xmlTextReaderClose (reader);
            xmlFreeDoc (doc);
            return;
         } // if
      } while (strcasecmp (tag, "content") != 0);
      xmlFreeDoc (doc);
      if (strchr (my_attribute.src, '#'))
      {
         anchor = strdup (strchr (my_attribute.src, '#') + 1);
         *strchr (my_attribute.src, '#') = 0;
      } // if
      // search current item
      for (current = total_items - 1; current >= 0; current--)
      {
         if (! daisy[current].page_number)
            continue;
         if (atoi (pn) >= daisy[current].page_number)
            break;
      } // for
      playing = displaying = current;
      view_screen ();
      current_page_number = atoi (pn);
      wmove (screenwin, daisy[current].y, daisy[current].x);
      just_this_item = -1;
      open_smil_file (my_attribute.src, anchor);
   } // if (strcasestr (daisy_version, "3"))
} // go_to_page_number

void go_to_time ()
{
   char time_str[10];
   int secs;

   kill (player_pid, SIGKILL);
   player_pid = -2;
   mvwaddstr (titlewin, 1, 0, "----------------------------------------");
   waddstr (titlewin, "----------------------------------------");
   mvwprintw (titlewin, 1, 0, gettext ("Go to time (MM:SS): "));
   echo ();
   wgetnstr (titlewin, time_str, 5);
   noecho ();
   view_screen ();
   secs = (time_str[0] - 48) * 600 + (time_str[1] - 48)* 60 +
             (time_str[3] - 48) * 10 + (time_str[4] - 48);
   player_pid = play_track (cd_dev, sound_dev, "alsa",
                      daisy[current].first_lsn + (secs * 75), speed);
   seconds = time (NULL) - secs;
   playing = displaying = current;
} // go_to_time

void select_next_output_device ()
{
   FILE *r;
   int n, y;
   size_t bytes;
   char *list[10], *trash;

   wclear (screenwin);
   wprintw (screenwin, "\nSelect a soundcard:\n\n");
   if (! (r = fopen ("/proc/asound/cards", "r")))
   {
      endwin ();
      beep ();
      puts (gettext ("Cannot read /proc/asound/cards"));
      fflush (stdout);
      _exit (1);
   } // if
   for (n = 0; n < 10; n++)
   {
      list[n] = (char *) malloc (1000);
      bytes = getline (&list[n], &bytes, r);
      if ((int) bytes == -1)
         break;
      trash = (char *) malloc (1000);
      bytes = getline (&trash, &bytes, r);
      free (trash);
      wprintw (screenwin, "   %s", list[n]);
      free (list[n]);
   } // for
   fclose (r);
   y = 3;
   nodelay (screenwin, FALSE);
   for (;;)
   {
      wmove (screenwin, y, 2);
      switch (wgetch (screenwin))
      {
      case 13:
         snprintf (sound_dev, 15, "hw:%i", y - 3);
         view_screen ();
         nodelay (screenwin, TRUE);
         return;
      case KEY_DOWN:
         if (++y == n + 3)
            y = 3;
         break;
      case KEY_UP:
         if (--y == 2)
           y = n + 2;
         break;
      default:
         view_screen ();
         nodelay (screenwin, TRUE);
         return;
      } // switch
   } // for
} // select_next_output_device

void browse (char *wd, char *prog_name)
{
   int old_screen;
   char str[MAX_STR];

   current = 0;
   just_this_item = playing = -1;
   get_bookmark ();
   view_screen ();
   nodelay (screenwin, TRUE);
   wmove (screenwin, daisy[current].y, daisy[current].x);
   if (audiocd == 1)
   {
      depth = 1;
      view_screen ();
   } // if

   for (;;)
   {
      switch (wgetch (screenwin))
      {
      case 13: // ENTER
         just_this_item = -1;
         view_screen ();
         playing = displaying = current;
         current_page_number = daisy[playing].page_number;
         if (player_pid > -1)
            kill (player_pid, SIGKILL);
         player_pid = -2;
         if (discinfo)
         {
            snprintf (str, MAX_STR - 1,
                      "cd \"%s\"; \"%s\" \"%s\"/\"%s\" -d %s",
                      wd, prog_name, daisy_mp, daisy[current].daisy_mp,
                      sound_dev);
            switch (system (str))
            {
            default:
               break;
            } // switch

            snprintf (str, MAX_STR - 1,
                      "cd \"%s\"; \"%s\" \"%s\" -d %s\n", wd, prog_name,
                      daisy_mp, sound_dev);
            switch (system (str))
            {
            default:
               break;
            } // switch
            quit_daisy_player ();
            _exit (0);
         } // if
         if (audiocd == 1)
         {
            player_pid = play_track (cd_dev, sound_dev, "alsa",
                daisy[current].first_lsn, speed);
            seconds = time (NULL);
            sector_ptr = 0;
         } // if
         if (audiocd == 0)
         {
            open_smil_file (daisy[current].smil_file,
                            daisy[current].anchor);
         } // if
         start_time = 0;
         break;
      case '/':
         if (discinfo)
         {
            beep ();
            break;
         } // if
         search (current + 1, '/');
         break;
      case ' ':
      case KEY_IC:
      case '0':
         if (discinfo)
         {
            beep ();
            break;
         } // if
         pause_resume ();
         break;
      case 'd':
         if (discinfo)
         {
            beep ();
            break;
         } // if
         store_to_disk ();
         break;
      case 'e':
      case '.':
      case KEY_DC:
         if (discinfo)
         {
            beep ();
            break;
         } // if
         xmlTextReaderClose (reader);
         quit_daisy_player ();
         while (kill (player_pid, 0) == 0);
         if (chdir ("/") == 0)
         {
            char str[MAX_STR + 1];

            snprintf (str, MAX_STR, "udisks --unmount %s > /dev/null", cd_dev);
            if (system (str) == -1)
               _exit (0);
            snprintf (str, MAX_STR, "udisks --eject %s > /dev/null", cd_dev);
            if (system (str) == -1)
               _exit (0);
         } // if
         _exit (0);
      case 'f':
         if (playing == -1)
         {
            beep ();
            break;
         } // if
         current = playing;
         previous_item ();
         view_screen ();
         break;
      case 'g':
         if (audiocd == 1)
         {
            go_to_time ();
            break;
         } // if
         if (total_pages)
            go_to_page_number ();
         else
            beep ();
         break;
      case 'h':
      case '?':
      {
         int flag = 0;

         if (playing < 0)
            flag = 1;
         if (! discinfo)
            pause_resume ();
         player_pid = -2;
         help ();
         if (flag)
            break;
         playing = displaying;
         if (audiocd == 0)
         {
            skip_left ();
            skip_right ();
         } // if
         if (audiocd == 1)
         {
            playing = -1;
            pause_resume ();
         } // if
         break;
      }
      case 'j':
      case '5':
      case KEY_B2:
         if (discinfo)
         {
            beep ();
            break;
         } // if
         if (just_this_item != -1)
            just_this_item = -1;
         else
            just_this_item = current;
         mvwprintw (screenwin, daisy[displaying].y, 0, " ");
         if (playing == -1)
         {
            just_this_item = displaying = playing = current;
            kill (player_pid, SIGKILL);
            player_pid = -2;
            if (audiocd == 1)
            {
               player_pid = play_track (cd_dev, sound_dev, "alsa",
                   daisy[current].first_lsn, speed);
            }
            else
               open_smil_file (daisy[current].smil_file, daisy[current].anchor);
         } // if
         wattron (screenwin, A_BOLD);
         if (just_this_item != -1 &&
             daisy[displaying].screen == daisy[playing].screen)
            mvwprintw (screenwin, daisy[displaying].y, 0, "J");
         else
            mvwprintw (screenwin, daisy[displaying].y, 0, " ");
         wrefresh (screenwin);
         wattroff (screenwin, A_BOLD);
         current_page_number = daisy[playing].page_number;
         break;
      case 'l':
         if (discinfo)
         {
            beep ();
            break;
         } // if
         change_level ('l');
         break;
      case 'L':
         if (discinfo)
         {
            beep ();
            break;
         } // if
         change_level ('L');
         break;
      case 'n':
         if (discinfo)
         {
            beep ();
            break;
         } // if
         search (current + 1, 'n');
         break;
      case 'N':
         if (discinfo)
         {
            beep ();
            break;
         } // if
         search (current - 1, 'N');
         break;
      case 'o':
         if (playing == -1)
         {
            beep ();
            break;
         } // if
         pause_resume ();
         select_next_output_device ();
         playing = displaying;
         if (audiocd == 0)
         {
            skip_left ();
            skip_right ();
         } // if
         if (audiocd == 1)
         {
            playing = -1;
            pause_resume ();
         } // if
         break;
      case 'p':
         put_bookmark ();
         break;
      case 'q':
         quit_daisy_player ();
         _exit (0);
      case 's':
         if (audiocd == 0)
         {
            kill (player_pid, SIGKILL);
            player_pid = -2;
         } // if
         if (audiocd == 1)
         {
            kill (player_pid, SIGKILL);
         } // if
         playing = just_this_item = -1;
         view_screen ();
         wmove (screenwin, daisy[current].y, daisy[current].x);
         break;
      case KEY_DOWN:
      case '2':
         next_item ();
         break;
      case KEY_UP:
      case '8':
         if (current == 0)
         {
            beep ();
            break;
         } // if
         current--;
         wmove (screenwin, daisy[current].y, daisy[current].x);
         previous_item ();
         break;
      case KEY_RIGHT:
      case '6':
         if (audiocd == 0)
            skip_right ();
         if (audiocd == 1)
         {
            int ret;

            ret = ftruncate (pipefd[0], 0);
            ret = ftruncate (pipefd[1], 0);
            paranoia_seek (par, 225, SEEK_CUR);
            lsn_cursor += 225;
            ret = ftruncate (pipefd[0], 0);
            ret = ftruncate (pipefd[1], 0);
            ret = ret;
         } // if
         break;
      case KEY_LEFT:
      case '4':
         if (audiocd == 0)
            skip_left ();
         if (audiocd == 1)
         {
            int ret;

            ret = ftruncate (pipefd[0], 0);
            ret = ftruncate (pipefd[1], 0);
            paranoia_seek (par, -225, SEEK_CUR);
            lsn_cursor -= 225;
            ret = ftruncate (pipefd[0], 0);
            ret = ftruncate (pipefd[1], 0);
            ret = ret;
         } // if
         break;
      case KEY_NPAGE:
      case '3':
         if (daisy[current].screen == daisy[total_items - 1].screen)
         {
            beep ();
            break;
         } // if
         old_screen = daisy[current].screen;
         while (daisy[++current].screen == old_screen);
         view_screen ();
         wmove (screenwin, daisy[current].y, daisy[current].x);
         break;
      case KEY_PPAGE:
      case '9':
         if (daisy[current].screen == 0)
         {
            beep ();
            break;
         } // if
         old_screen = daisy[current].screen;
         while (daisy[--current].screen == old_screen);
         current -= max_y - 1;
         view_screen ();
         wmove (screenwin, daisy[current].y, daisy[current].x);
         break;
      case ERR:
         break;
      case 'U':
      case '+':
         if (speed >= 100)
         {
            beep ();
            break;
         } // if
         speed += 0.1;
         if (audiocd == 1)
         {
            kill (player_pid, SIGKILL);
            lsn_t start;

            start = (lsn_t) (start_time + (float) (time (NULL) - seconds) * 75) +
                    daisy[playing].first_lsn;
            player_pid = play_track (cd_dev, sound_dev, "alsa",
                            start, speed);
            break;
         } // if
         skip_left ();
         kill (player_pid, SIGKILL);
         break;
      case 'D':
      case '-':
         if (speed <= 0.3)
         {
            beep ();
            break;
         } // if
         speed -= 0.1;
         if (audiocd == 1)
         {
            kill (player_pid, SIGKILL);
            lsn_t start;

            start = (lsn_t) (start_time + (float) (time (NULL) - seconds) * 75) +
                 daisy[playing].first_lsn;
            player_pid = play_track (cd_dev, sound_dev, "alsa",
                            start, speed);
            break;
         } // if
         skip_left ();
         kill (player_pid, SIGKILL);
         break;
      case KEY_HOME:
      case '*':
      case '7':
         speed = 1;
         if (audiocd == 1)
         {
            kill (player_pid, SIGKILL);
            lsn_t start;

            start = (lsn_t) (start_time + (float) (time (NULL) - seconds) * 75) +
                 daisy[playing].first_lsn;
            player_pid = play_track (cd_dev, sound_dev, "alsa",
                            start, speed);
            break;
         } // if
         kill (player_pid, SIGKILL);
         skip_left ();
         skip_right ();
         break;
      default:
         beep ();
         break;
      } // switch

      if (playing > -1 && audiocd == 0)
      {
         play_clip ();
         view_time ();
      } // if

      if (playing == -1 || audiocd == 0)
      {
         fd_set rfds;
         struct timeval tv;

         FD_ZERO (&rfds);
         FD_SET (0, &rfds);
         tv.tv_sec = 0;
         tv.tv_usec = 100000;
// pause till a key has been pressed or 0.1 seconds are elapsed
         select (1, &rfds, NULL, NULL, &tv);
      } // if

      if (playing > -1 && audiocd == 1)
      {
         int ret; // drop return-code
         int16_t *p_readbuf;

         if (! (p_readbuf = paranoia_read (par, NULL)))
            break;
         ret = write (pipefd[1], p_readbuf, CDIO_CD_FRAMESIZE_RAW);
         lsn_cursor++;
         ret = ret;
         if (lsn_cursor > daisy[playing].last_lsn)
         {
            start_time = 0;
            current = displaying = ++playing;
            lsn_cursor = daisy[playing].first_lsn;
            if (current >= total_items)
            {
               struct passwd *pw;

               pw = getpwuid (geteuid ());
               quit_daisy_player ();
               snprintf (str, MAX_STR - 1, "%s/.daisy-player/%s%s",
                         pw->pw_dir, bookmark_title, get_mcn ());
               unlink (str);
               _exit (0);
            } // if
            if (just_this_item > -1)
            {
               kill (player_pid, SIGKILL);
               playing = just_this_item = -1;
            } // if
            view_screen ();
            seconds = time (NULL);
         } // if
         view_time ();
      } // if
   } // for
} // browse

void usage (char *argv)
{
   printf (gettext ("Daisy-player - Version %s\n"), DAISY_PLAYER_VERSION);
   puts ("(C)2003-2014 J. Lemmens");
   printf (gettext ("\nUsage: %s [directory_with_a_Daisy-structure] "),
                     basename (argv));
   printf (gettext ("[-c cdrom_device] [-d ALSA_sound_device] [-n | -y]\n"));
   fflush (stdout);
   _exit (1);
} // usage

char *get_mount_point ()
{
   FILE *proc;
   size_t len = 0;
   char *str = NULL;

   if (! (proc = fopen ("/proc/mounts", "r")))
   {
      endwin ();
      beep ();
      puts (gettext ("\nCannot read /proc/mounts."));
      fflush (stdout);
      _exit (1);
   } // if
   do
   {
      str = malloc (len + 1);
      if (getline (&str, &len, proc) == -1)
         break;
   } while (! strcasestr (str, "iso9660"));
   fclose (proc);
   if (strcasestr (str, "iso9660"))
   {
      strncpy (daisy_mp, strchr (str, ' ') + 1, MAX_STR - 1);
      *strchr (daisy_mp, ' ') = 0;
      free (str);
      return daisy_mp;
   } // if
   free (str);
   return NULL;
} // get_mount_point

void handle_discinfo (char *discinfo_html)
{
   int h, m, s;
   float t = 0;
   xmlTextReaderPtr di, ncc;

   xmlDocPtr doc = xmlRecoverFile (discinfo_html);
   if (! (di = xmlReaderWalker (doc)))
   {
      endwin ();
      beep ();
      printf (gettext ("\nCannot read %s\n"), discinfo_html);
      fflush (stdout);
      _exit (1);
   } // if (! (di = xmlReaderWalker (doc)
   while (1)
   {
      if (! get_tag_or_label (di))
         break;
      if (strcasecmp (tag, "title") == 0)
      {
         do
         {
            if (! get_tag_or_label (di))
               break;
         } while ( !*label);
         strncpy (daisy_title, label, MAX_STR - 1);
      } // if (strcasecmp (tag, "title") == 0)
      if (strcasecmp (tag, "a") == 0)
      {
         strncpy (daisy[current].filename, my_attribute.href, MAX_STR - 1);
         xmlDocPtr doc = xmlRecoverFile (daisy[current].filename);
         if (! (ncc = xmlReaderWalker (doc)))
         {
            endwin ();
            beep ();
            printf (gettext ("\nCannot read %s\n"), daisy[current].filename);
            fflush (stdout);
            _exit (1);
         } // if
         do
         {
            *ncc_totalTime = 0;
            if (! get_tag_or_label (ncc))
               break;
         } while (! *ncc_totalTime);
         daisy[current].duration = read_time (ncc_totalTime);
         t += daisy[current].duration;
         xmlTextReaderClose (ncc);
         xmlFreeDoc (doc);
         do
         {
            if (! get_tag_or_label (di))
               break;
         } while (! *label);
         strncpy (daisy[current].label, label, MAX_STR - 1);
         strncpy (daisy[current].daisy_mp, dirname (daisy[current].filename),
                  MAX_STR - 1);
         daisy[current].level = 1;
         daisy[current].x = 0;
         daisy[current].y = displaying;
         daisy[current].screen = current / max_y;
         current++;
         displaying++;
      } // if (strcasecmp (tag, "a") == 0)
   } // while
   xmlTextReaderClose (di);
   xmlFreeDoc (doc);
   discinfo = 1;
   total_items = current;
   total_time = t;
   h = t / 3600;
   t -= h * 3600;
   m = t / 60;
   t -= m * 60;
   s = t;
   snprintf (ncc_totalTime, MAX_STR - 1, "%02d:%02d:%02d", h, m, s);
   depth = 1;
   view_screen ();
} // handle_discinfo

int main (int argc, char *argv[])
{
   int opt;
   char str[MAX_STR], DISCINFO_HTML[MAX_STR], start_wd[MAX_STR];


   LIBXML_TEST_VERSION // libxml2
   strncpy (prog_name, basename (argv[0]), MAX_STR - 1);
   daisy_player_pid = getpid ();
   speed = 1;
   strncpy (cd_dev, "/dev/sr0", 15);
   atexit (quit_daisy_player);
   signal (SIGCHLD, player_ended);
// jos   signal (SIGTERM, quit_daisy_player);
   read_rc ();
   setlocale (LC_ALL, "");
   setlocale (LC_NUMERIC, "C");
   textdomain (prog_name);
   strncpy (start_wd, get_current_dir_name (), MAX_STR - 1);
   opterr = 0;
   while ((opt = getopt (argc, argv, "c:d:ny")) != -1)
   {
      switch (opt)
      {
      case 'c':
         strncpy (cd_dev, optarg, 15);
         break;
      case 'd':
         strncpy (sound_dev, optarg, 15);
         break;
      case 'n':
         cddb_flag = 'n';
         break;
      case 'y':
      case 'j':
         cddb_flag = 'y';
         switch (system ("cddbget -c null > /dev/null 2>&1"))
         // if cddbget is installed
         {
         case 0:
            break;
         default:
            cddb_flag = 'n';
         } // switch
         break;
      default:
         usage (prog_name);
      } // switch
   } // while
   initscr ();
   if (! (titlewin = newwin (2, 80,  0, 0)) ||
       ! (screenwin = newwin (23, 80, 2, 0)))
   {
      endwin ();
      beep ();
      printf ("No curses\n");
      fflush (stdout);
      _exit (1);
   } // if
   fclose (stderr);
   getmaxyx (screenwin, max_y, max_x);
   printw ("(C)2003-2014 J. Lemmens\n");
   printw (gettext ("Daisy-player - Version %s\n"), DAISY_PLAYER_VERSION);
   printw (gettext ("A parser to play Daisy CD's with Linux\n"));
   if (system ("udisks -h > /dev/null") > 0)
   {
      endwin ();
      beep ();
      printf (gettext ("\nDaisy-player needs the \"udisks\" programme.\n"));
      printf (gettext ("Please install it and try again.\n"));
      fflush (stdout);
      _exit (1);
   } // if
   if (system ("madplay -h > /dev/null") > 0)
   {
      endwin ();
      beep ();
      printf (gettext ("\nDaisy-player needs the \"madplay\" programme.\n"));
      printf (gettext ("Please install it and try again.\n"));
      fflush (stdout);
      _exit (1);
   } // if

// set the CD speed so it makes less noise
   set_drive_speed (4);

   printw (gettext ("Scanning for a Daisy CD..."));
   refresh ();
   audiocd = 0;
   if (argv[optind])
// if there is an argument
   {
// determine filetype
      magic_t myt;

      myt = magic_open (MAGIC_CONTINUE | MAGIC_SYMLINK);
      magic_load (myt, NULL);
      if (magic_file (myt, argv[optind]) == NULL)
      {
         int e;

         e = errno;
         endwin ();
         printf ("%s: %s\n", argv[optind], strerror (e));
         beep ();
         fflush (stdout);
         usage (prog_name);
      } // if
      if (strcasestr (magic_file (myt, argv[optind]), "directory"))
         strncpy (daisy_mp, argv[optind], MAX_STR - 1);
      else
      if (strcasestr (magic_file (myt, argv[optind]), "Zip archive") ||
          strcasestr (magic_file (myt, argv[optind]), "RAR archive data") ||
          strcasestr (magic_file (myt, argv[optind]),
                      "Microsoft Cabinet archive data") ||
          strcasestr (magic_file (myt, argv[optind]),
                      "gzip compressed data") ||
          strcasestr (magic_file (myt, argv[optind]), "bzip2 compressed data"))
      {
         char *str, cmd[MAX_CMD];

         str = strdup ("/tmp/daisy-player.XXXXXX");
         if (! mkdtemp (str))
         {
            endwin ();
            printf ("mkdtemp ()\n");
            beep ();
            fflush (stdout);
            _exit (1);
         } // if
         if (system ("unar -h > /dev/null") != 0)
         {
            endwin ();
            beep ();
            printf (gettext (
                    "\nDaisy-player needs the \"unar\" programme.\n"));
            printf (gettext ("Please install it and try again.\n"));
            fflush (stdout);
            _exit (1);
         } // if
         snprintf (cmd, MAX_CMD - 1, "unar \"%s\" -o %s > /dev/null",
                   argv[optind], str);
         switch (system (cmd))
         {
         default:
            break;
         } // switch
         wrefresh (screenwin);

         DIR *dir;
         struct dirent *dirent;
         int entries = 0;

         if (! (dir = opendir (str)))
         {
            endwin ();
            beep ();
            printf ("\n%s\n", strerror (errno));
            fflush (stdout);
            _exit (1);
         } // if
         while ((dirent = readdir (dir)) != NULL)
         {
            if (strcasecmp (dirent->d_name, ".") == 0 ||
                strcasecmp (dirent->d_name, "..") == 0)
               continue;
            snprintf (daisy_mp, MAX_STR - 1, "%s/%s", str, dirent->d_name);
            entries++;
         } // while
         if (entries > 1)
            snprintf (daisy_mp, MAX_STR - 1, "%s", str);
         closedir (dir);
      } // if unar
      else
      {
         endwin ();
         printf (gettext ("\nNo DAISY-CD or Audio-cd found\n"));
         beep ();
         fflush (stdout);
         usage (prog_name);
      } // if
      magic_close (myt);
   }
   else
// when no mount-point is given try to mount the cd
   {
      FILE *r;
      char cd[MAX_STR + 1];
      double start;
      magic_t myt;

      myt = magic_open (MAGIC_CONTINUE | MAGIC_SYMLINK);
      magic_load (myt, NULL);
      if (magic_file (myt, cd_dev) == NULL)
      {
         endwin ();
         beep ();
         printf ("%s: %s\n", cd_dev, gettext (strerror (ENOTBLK)));
         fflush (stdout);
         usage (prog_name);
      } // if
      if (! strcasestr (magic_file (myt, cd_dev), "block special"))
      {
         endwin ();
         beep ();
         printf ("%s: %s\n", cd_dev, gettext (strerror (ENOTBLK)));
         fflush (stdout);
         usage (prog_name);
      } // if
      magic_close (myt);
      start = time (NULL);
      snprintf (cd, MAX_STR - 1, "udisks --mount %s > /dev/null", cd_dev);
      while (1)
      {
         if (system (cd) == 256)
         {
            if (time (NULL) - start >= 20)
            {
               endwin ();
               puts (gettext ("No Daisy CD in drive."));
               fflush (stdout);
               beep ();
               _exit (0);
            } // if
         }
         else
            break;
      } // while
      if (! get_mount_point ())
// try the default block device
      {
         if (access ("/run/udev/data/b11:0", R_OK) == -1)
         // block 11 = cdrom
         {
            int e;

            e = errno;
            endwin ();
            beep ();
            printf ("%s: %s\n\n", "No cd-player found",
                    gettext (strerror (e)));
            fflush (stdout);
            usage (prog_name);
         } // if
         start = time (NULL);
         r = fopen ("/run/udev/data/b11:0", "r");
         while (1)
         {
            char *str;;
            size_t s;

            str = (char *) malloc (MAX_STR);
            s = MAX_STR - 1;
            switch (getline (&str, &s, r))
            {
            default:
               break;
            } // switch
            if (strcasestr (str, "TRACK_COUNT_DATA"))
            {
               if (! get_mount_point ())
// if not found a mounted cd, try to mount one
               {
                  char str[MAX_STR + 1];

                  snprintf (str, MAX_STR,
                            "udisks --mount %s > /dev/null", cd_dev);
                  if (system (str) == -1)
                  {
                     endwin ();
                     beep ();
                     puts (gettext ("\nCannot use udisks command."));
                     fflush (stdout);
                     _exit (1);
                  } // if

// try again to mount one
                  if (get_mount_point ())
                     break;
               } // if
            } // if "TRACK_COUNT_DATA"))
            if (strcasestr (str, "TRACK_COUNT_AUDIO"))
            {
// probably an Audio-CD
               audiocd = 1;
               printw (gettext ("\nFound an Audio-CD. "));
               if (cddb_flag == 'y')
                  printw (gettext ("Get titles from freedb.freedb.org..."));
               refresh ();
               strncpy (bookmark_title, "Audio-CD", MAX_STR - 1);
               strncpy (daisy_title, "Audio-CD", MAX_STR - 1);
               init_paranoia (cd_dev);
               get_toc_audiocd (cd_dev);
               strncpy (daisy_mp, "/tmp", MAX_STR - 1);
               break;
            } // if TRACK_COUNT_AUDIO
            if (feof (r))
            {
               fclose (r);
               sleep (0.2);
               r = fopen ("/run/udev/data/b11:0", "r");
            } // if
            if (time (NULL) - start >= 20)
            {
               endwin ();
               puts (gettext ("No Daisy CD in drive."));
               fflush (stdout);
               beep ();
               _exit (0);
            } // if
         } // while
         fclose (r);
      } // if
   } // if no mount arg
   keypad (screenwin, TRUE);
   meta (screenwin,       TRUE);
   nonl ();
   noecho ();
   player_pid = -2;
   if (chdir (daisy_mp) == -1)
   {
      endwin ();
      beep ();
      printf (gettext ("No such directory: %s\n"), daisy_mp);
      fflush (stdout);
      _exit (1);
   } // if
   if (audiocd == 0)
   {
      snprintf (DISCINFO_HTML, MAX_STR - 1, "discinfo.html");
      if (access (DISCINFO_HTML, R_OK) == 0)
         handle_discinfo (DISCINFO_HTML);
      if (! discinfo)
      {
         snprintf (NCC_HTML, MAX_STR - 1, "ncc.html");
         if (access (NCC_HTML, R_OK) == 0)
         {
            xmlDocPtr doc = xmlRecoverFile (NCC_HTML);
            if (! (reader = xmlReaderWalker (doc)))
            {
               endwin ();
               beep ();
               printf (gettext ("\nCannot read %s\n"), NCC_HTML);
               fflush (stdout);
               _exit (1);
            } // if
            while (1)
            {
               if (! get_tag_or_label (reader))
                  break;
               if (strcasecmp (tag, "title") == 0)
               {
                  if (! get_tag_or_label (reader))
                     break;
                  if (*label)
                  {
                     int x;

                     for (x = strlen (label) - 1; x >= 0; x--)
                     {
                        if (isspace (label[x]))
                           label[x] = 0;
                        else
                           break;
                     } // for
                     strncpy (bookmark_title, label, MAX_STR - 1);
                     break;
                  } // if
               } // if
            } // while
            read_daisy_2 (1);
         }
         else
         {
            ignore_ncx = 0;
            read_daisy_3 (daisy_mp);
            strncpy (daisy_version, "3", 2);
         } // if
         if (total_items == 0)
            total_items = 1;
      } // if (! discinfo);
   } // if audiocd == 0
   wattron (titlewin, A_BOLD);
   snprintf (str, MAX_STR - 1, gettext
             ("Daisy-player - Version %s - (C)2014 J. Lemmens"),
             DAISY_PLAYER_VERSION);
   mvwprintw (titlewin, 0, 0, str);
   wrefresh (titlewin);

   if (strlen (daisy_title) + strlen (str) >= 80)
      mvwprintw (titlewin, 0, 80 - strlen (daisy_title) - 4, "... ");
   mvwprintw (titlewin, 0, 80 - strlen (daisy_title), "%s", daisy_title);
   wrefresh (titlewin);
   mvwaddstr (titlewin, 1, 0, "----------------------------------------");
   waddstr (titlewin, "----------------------------------------");
   mvwprintw (titlewin, 1, 0, gettext ("Press 'h' for help "));
   level = 1;
   *search_str = 0;
   snprintf (tmp_wav, MAX_STR, "/tmp/daisy-player_XXXXXX");
   if ((tmp_wav_fd = mkstemp (tmp_wav)) == 01)
   {
      int e;

      e = errno;
      endwin ();
      beep ();
      printf ("%s\n", strerror (e));
      fflush (stdout);
      _exit (0);
   } // if
   browse (start_wd, argv[0]);
   return 0;
} // main
