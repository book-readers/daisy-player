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
#undef PACKAGE
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_URL
#undef PACKAGE_VERSION
#undef VERSION
#include "config.h"

daisy_t daisy[2000];  
misc_t misc;
my_attribute_t my_attribute;

extern void parse_smil_3 ();
extern float read_time (char *);
extern int get_tag_or_label (xmlTextReaderPtr);
extern void get_clips (char *, char *);
void save_rc ();
void quit_daisy_player ();
extern void get_page_number_3 ();
void open_smil_file (char *, char *);
extern void read_daisy_3 ();
extern void get_toc_audiocd (char *);
extern pid_t play_track (char *, char *, lsn_t);
extern char *get_mcn ();
void set_drive_speed (int);
void init_paranoia (char *);
void skip_left ();
void play_now ();
void skip_right ();
extern void get_label (int, int);

void playfile (char *in_file, char *in_type, char *out_file, char *out_type,
               char *tempo)
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
      printf ("playfile %s: %s\n", out_file, gettext (strerror (EINVAL)));
      strncpy (misc.sound_dev, "hw:0", MAX_STR - 1);
      save_rc (misc);
      beep ();
      fflush (stdout);
      kill (getppid (), SIGQUIT);
   } // if
   if (strcmp (in_type, "cdda") == 0)
   {
      sox_in->encoding.encoding = SOX_ENCODING_SIGN2;
      sox_in->encoding.bits_per_sample = 16;

#ifdef SOX_OPTION_NO
      /* libsox < 14.4 */
      sox_in->encoding.reverse_bytes = SOX_OPTION_NO;
#endif
#ifdef sox_option_no
      /* libsox >= 14.4 */
      sox_in->encoding.reverse_bytes = sox_option_no;
#endif
   } // if

   chain = sox_create_effects_chain (&sox_in->encoding, &sox_out->encoding);

   e = sox_create_effect (sox_find_effect ("input"));
   args[0] = (char *) sox_in, sox_effect_options (e, 1, args);
   sox_add_effect (chain, e, &sox_in->signal, &sox_in->signal);

/* Don't use the sox trim effect. It works nice, but is far too slow
   char str2[MAX_STR];
   snprintf (str,  MAX_STR - 1, "%f", misc.clip_begin);
   snprintf (str2, MAX_STR - 1, "%f", misc.clip_end - misc.clip_begin);
   e = sox_create_effect (sox_find_effect ("trim"));
   args[0] = str;
   args[1] = str2;
   sox_effect_options (e, 2, args);
   sox_add_effect (chain, e, &sox_in->signal, &sox_in->signal);
*/

   e = sox_create_effect (sox_find_effect ("tempo"));
   args[0] = "-s";
   args[1] = tempo;
   sox_effect_options (e, 2, args);
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
             pw->pw_dir, misc.bookmark_title, get_mcn (misc));
   if (! (writer = xmlNewTextWriterFilename (str, 0)))
      return;
   xmlTextWriterStartDocument (writer, NULL, NULL, NULL);
   xmlTextWriterStartElement (writer, BAD_CAST "bookmark");
   if (misc.playing >= 0)
      xmlTextWriterWriteFormatAttribute
         (writer, BAD_CAST "item", "%d", misc.playing);
   else
      xmlTextWriterWriteFormatAttribute
         (writer, BAD_CAST "item", "%d", misc.current);
   if (misc.audiocd == 0)
      xmlTextWriterWriteFormatAttribute
         (writer, BAD_CAST "clip-begin", "%f", misc.clip_begin);
   else
      xmlTextWriterWriteFormatAttribute
         (writer, BAD_CAST "seconds", "%d", (int) (time (NULL) - misc.seconds));
   xmlTextWriterWriteFormatAttribute (writer, BAD_CAST "level", "%d", misc.level);
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
   struct passwd *pw;

   pw = getpwuid (geteuid ());
   if (! *misc.bookmark_title)
      return;
   snprintf (str, MAX_STR - 1, "%s/.daisy-player/%s%s",
             pw->pw_dir, misc.bookmark_title, get_mcn (misc));
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
   } while (strcasecmp (misc.tag, "bookmark") != 0);
   xmlTextReaderClose (local);
   xmlFreeDoc (doc);
   if (misc.current <= 0)
      misc.current = 0;
   if (misc.current >= misc.total_items)
      misc.current = 0;
   misc.displaying = misc.playing = misc.current;
   if (misc.audiocd == 1)
   {
      misc.player_pid = play_track (misc.sound_dev, "alsa",
            daisy[misc.current].first_lsn + (misc.seconds * 75));
      misc.seconds = time (NULL) - misc.seconds;
      return;
   } // if
   get_clips (my_attribute.clip_begin, my_attribute.clip_end);
   begin = misc.clip_begin;
   open_smil_file (daisy[misc.current].smil_file,
                   daisy[misc.current].anchor);
   while (1)
   {
      if (! get_tag_or_label (misc.reader))
         return;
      if (strcasecmp (misc.tag, "audio") == 0)
      {
         strncpy (misc.current_audio_file, my_attribute.src, MAX_STR - 1);
         get_clips (my_attribute.clip_begin, my_attribute.clip_end);
         if (misc.clip_begin == begin)
            break;
      } // if
   } // while
   if (misc.level < 1)
      misc.level = 1;
   misc.current_page_number = daisy[misc.playing].page_number;
   misc.just_this_item = -1;
   play_now (misc);
} // get_bookmark

void get_page_number_2 (char *p)
{
// function for daisy 2.02
   char *anchor = 0, *id1, *id2;
   xmlTextReaderPtr page;
   xmlDocPtr doc;

   doc = xmlRecoverFile (daisy[misc.playing].smil_file);
   if (! (page = xmlReaderWalker (doc)))
   {
      endwin ();
      beep ();
      printf (gettext ("\nCannot read %s\n"), daisy[misc.playing].smil_file);
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
   } while (strcasecmp (misc.tag, "text") != 0);
   id2 = strdup (my_attribute.id);
   xmlTextReaderClose (page);
   xmlFreeDoc (doc);
   doc = xmlRecoverFile (misc.NCC_HTML);
   if (! (page = xmlReaderWalker (doc)))
   {
      endwin ();
      beep ();
      printf (gettext ("\nCannot read %s\n"), misc.NCC_HTML);
      fflush (stdout);
      _exit (1);
   } // if
   while (1)
   {
      if (! get_tag_or_label (page))
         return;
      if (strcasecmp (misc.tag, "a") == 0)
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
   } while (! *misc.label);
   xmlTextReaderClose (page);
   xmlFreeDoc (doc);
   misc.current_page_number = atoi (misc.label);
} // get_page_number_2

int get_next_clips ()
{                              
   while (1)
   {
      if (! get_tag_or_label (misc.reader))
         return 0;
      if (strcasecmp (my_attribute.id, daisy[misc.playing + 1].anchor) == 0 &&
          misc.playing + 1 != misc.total_items)
         return 0;
      if (strcasecmp (my_attribute.class, "pagenum") == 0)
         if (strcasestr (misc.daisy_version, "3"))
            get_page_number_3 (misc);
      if (strcasecmp (misc.tag, "audio") == 0)
      {
         strncpy (misc.current_audio_file, my_attribute.src, MAX_STR - 1);
         get_clips (my_attribute.clip_begin, my_attribute.clip_end);
         return 1;
      } // if (strcasecmp (misc.tag, "audio") == 0)
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

   misc.total_time = 0;
   for (x = 0; x < misc.total_items; x++)
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
         if (strcasecmp (misc.tag, "audio") == 0)
         {
            strncpy (misc.current_audio_file, my_attribute.src, MAX_STR - 1);
            get_clips (my_attribute.clip_begin, my_attribute.clip_end);
            daisy[x].begin = misc.clip_begin;
            daisy[x].duration += misc.clip_end - misc.clip_begin;
            while (1)
            {
               if (! get_tag_or_label (parse))
                  break;
               if (*daisy[x + 1].anchor)
                  if (strcasecmp (my_attribute.id, daisy[x + 1].anchor) == 0)
                     break;
               if (strcasecmp (misc.tag, "audio") == 0)
               {
                  get_clips (my_attribute.clip_begin, my_attribute.clip_end);
                  daisy[x].duration += misc.clip_end - misc.clip_begin;
               } // if (strcasecmp (misc.tag, "audio") == 0)
            } // while
            if (*daisy[x + 1].anchor)
               if (strcasecmp (my_attribute.id, daisy[x + 1].anchor) == 0)
                  break;
         } // if (strcasecmp (misc.tag, "audio") == 0)
      } // while
      xmlTextReaderClose (parse);
      xmlFreeDoc (doc);
      misc.total_time += daisy[x].duration;
   } // for
} // parse_smil_2

void view_page (char *id)
{
   if (misc.playing == -1)
      return;
   if (daisy[misc.playing].screen != daisy[misc.current].screen)
      return;
   if (misc.total_pages == 0)
      return;
   wattron (misc.screenwin, A_BOLD);
   if (strcasestr (misc.daisy_version, "2.02"))
      get_page_number_2 (id);
   if (misc.current_page_number)
      mvwprintw (misc.screenwin, daisy[misc.playing].y, 62, "(%3d)",
                 misc.current_page_number);
   else
      mvwprintw (misc.screenwin, daisy[misc.playing].y, 62, "     ");
   wattroff (misc.screenwin, A_BOLD);
   wmove (misc.screenwin, daisy[misc.current].y, daisy[misc.current].x);
   wrefresh (misc.screenwin);
} // view_page

void view_time ()
{
   float remain_time = 0, curr_time = 0;

   if (misc.playing == -1 ||
       daisy[misc.current].screen != daisy[misc.playing].screen)
      return;
   wattron (misc.screenwin, A_BOLD);
   if (misc.audiocd == 0)
      curr_time = misc.start_time + (float) (time (NULL) - misc.seconds);
   if (misc.audiocd == 1)
      curr_time = (misc.lsn_cursor - daisy[misc.playing].first_lsn) / 75;
   curr_time /= misc.speed;
   mvwprintw (misc.screenwin, daisy[misc.playing].y, 68, "%02d:%02d",
              (int) curr_time / 60, (int) curr_time % 60);
   if (misc.audiocd == 0)
      remain_time = daisy[misc.playing].duration - curr_time;
   if (misc.audiocd == 1)
      remain_time = (daisy[misc.playing].last_lsn -
                    daisy[misc.playing].first_lsn) / 75 - curr_time;
   remain_time /= misc.speed;
   mvwprintw (misc.screenwin, daisy[misc.playing].y, 74, "%02d:%02d",
              (int) remain_time / 60, (int) remain_time % 60);
   wattroff (misc.screenwin, A_BOLD);
   wmove (misc.screenwin, daisy[misc.current].y, daisy[misc.current].x);
   wrefresh (misc.screenwin);
} // view_time

void view_screen ()
{
   int i, x, x2;
   float time;

   mvwaddstr (misc.titlewin, 1, 0,
              "----------------------------------------");
   waddstr (misc.titlewin, "----------------------------------------");
   mvwprintw (misc.titlewin, 1, 0, gettext ("'h' for help -"));
   if (misc.total_pages)
      wprintw (misc.titlewin, gettext (" %d pages "), misc.total_pages);
   mvwprintw (misc.titlewin, 1, 28, gettext (" level: %d of %d "),
              misc.level, misc.depth);
   time = misc.total_time / misc.speed;
   int hours   = time / 3600;
   int minutes = (time - hours * 3600) / 60;
   int seconds = time - (hours * 3600 + minutes * 60);
   mvwprintw (misc.titlewin, 1, 47, gettext (" total length: %02d:%02d:%02d "),
              hours, minutes,seconds);
   mvwprintw (misc.titlewin, 1, 74, " %d/%d ",
              daisy[misc.current].screen + 1,
              daisy[misc.total_items - 1].screen + 1);
   wrefresh (misc.titlewin);
   wclear (misc.screenwin);
   for (i = 0; daisy[i].screen != daisy[misc.current].screen; i++);
   do
   {
      mvwprintw (misc.screenwin, daisy[i].y, daisy[i].x + 1, daisy[i].label);
      x = strlen (daisy[i].label) + daisy[i].x;
      if (x / 2 * 2 != x)
         waddstr (misc.screenwin, " ");
      for (x2 = x; x2 < 59; x2 = x2 + 2)
         waddstr (misc.screenwin, " .");
      if (daisy[i].page_number)
         mvwprintw (misc.screenwin, daisy[i].y, 61, " (%3d)", daisy[i].page_number);
      if (daisy[i].level <= misc.level)
      {
         int x = i, dur = 0;

         do
         {
            dur += daisy[x].duration;
         } while (daisy[++x].level > misc.level);
         dur /= misc.speed;
         mvwprintw (misc.screenwin, daisy[i].y, 74, "%02d:%02d",
                    (int) (dur + .5) / 60, (int) (dur + .5) % 60);
      } // if
      if (i >= misc.total_items - 1)
         break;
   } while (daisy[++i].screen == daisy[misc.current].screen);
   if (misc.just_this_item != -1 &&
       daisy[misc.current].screen == daisy[misc.playing].screen)
      mvwprintw (misc.screenwin, daisy[misc.displaying].y, 0, "J");
   wmove (misc.screenwin, daisy[misc.current].y, daisy[misc.current].x);
   wrefresh (misc.screenwin);
} // view_screen

void read_daisy_2 ()
{
// function for daisy 2.02
   int indent = 0, found_page_normal = 0;
   xmlTextReaderPtr ncc;

   xmlDocPtr doc = xmlRecoverFile (misc.NCC_HTML);
   if (! (ncc = xmlReaderWalker (doc)))
   {
      endwin ();
      beep ();
      printf (gettext ("\nCannot read %s\n"), misc.NCC_HTML);
      fflush (stdout);
      _exit (1);
   } // if
   misc.current = misc.displaying = 0;
   while (1)
   {
      if (! get_tag_or_label (ncc))
         break;
      if (strcasecmp (misc.tag, "h1") == 0 ||
          strcasecmp (misc.tag, "h2") == 0 ||
          strcasecmp (misc.tag, "h3") == 0 ||
          strcasecmp (misc.tag, "h4") == 0 ||
          strcasecmp (misc.tag, "h5") == 0 ||
          strcasecmp (misc.tag, "h6") == 0)
      {
         int l;

         found_page_normal = 0;
         daisy[misc.current].page_number = 0;
         l = misc.tag[1] - '0';
         daisy[misc.current].level = l;
         daisy[misc.current].x = l + 3 - 1;
         indent = daisy[misc.current].x = (l - 1) * 3 + 1;
         do
         {
            if (! get_tag_or_label (ncc))
               break;
         } while (strcasecmp (misc.tag, "a") != 0);
         strncpy (daisy[misc.current].smil_file, my_attribute.href,
                  MAX_STR - 1);

         if (strchr (daisy[misc.current].smil_file, '#'))
         {
            strncpy (daisy[misc.current].anchor,
                     strchr (daisy[misc.current].smil_file, '#') + 1, MAX_STR - 1);
            *strchr (daisy[misc.current].smil_file, '#') = 0;
         } // if
         do
         {
            if (! get_tag_or_label (ncc))
               break;
         } while (*misc.label == 0);
         get_label (indent, 0);
         misc.current++;
         misc.displaying++;
      } // if (strcasecmp (misc.tag, "h1") == 0 || ...
      if (strcasecmp (my_attribute.class, "page-normal") == 0 &&
          found_page_normal == 0)
      {
         found_page_normal = 1;
         do
         {
            if (! get_tag_or_label (ncc))
               break;
         } while (*misc.label == 0);
         daisy[misc.current - 1].page_number = atoi (misc.label);
      } // if (strcasecmp (my_attribute.class, "page-normal")
   } // while
   xmlTextReaderClose (ncc);
   xmlFreeDoc (doc);
   misc.total_items = misc.current;
   parse_smil_2 ();
} // read_daisy_2

void player_ended ()
{
   wait (NULL);
} // player_ended

void play_now ()
{                          
   if (misc.playing == -1)
      return;

   misc.seconds = time (NULL);
   misc.start_time = misc.clip_begin;
   switch (misc.player_pid = fork ())
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
   lseek (misc.tmp_wav_fd, SEEK_SET, 0);
   snprintf (cmd, MAX_CMD - 1, "madplay -Q %s -s %f -t %f -o wav:\"%s\"",
                  misc.current_audio_file, misc.clip_begin,
                  misc.clip_end - misc.clip_begin, misc.tmp_wav);
   if (system (cmd) != 0)
      _exit (0);
   snprintf (tempo_str, 10, "%lf", misc.speed);
   playfile (misc.tmp_wav, "wav", misc.sound_dev, "alsa", tempo_str);
   _exit (0);
} // play_now

void open_smil_file (char *smil_file, char *anchor)
{
   if (misc.audiocd == 1)
      return;
   xmlTextReaderClose (misc.reader);
   xmlDocPtr doc = xmlRecoverFile (smil_file);
   if (! (misc.reader = xmlReaderWalker (doc)))
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
      if (! get_tag_or_label (misc.reader))
         break;;
      if (strcasecmp (my_attribute.id, anchor) == 0)
         break;
   } // while
} // open_smil_file

void pause_resume ()
{                              
   if (misc.playing > -1)
   {
      misc.playing = -1;
      if (misc.audiocd == 0)
      {
         kill (misc.player_pid, SIGKILL);
         misc.player_pid = -2;
      }
      else
      {
         kill (misc.player_pid, SIGKILL);
      } // if
   }
   else
   {
      misc.playing = misc.displaying;
      if (misc.audiocd == 0)
      {
         skip_left (misc);
         skip_right (misc);
      }
      else
      {
         lsn_t start;

         start = (lsn_t) (misc.start_time + (float) (time (NULL) - misc.seconds) * 75) +
                 daisy[misc.playing].first_lsn;
         misc.player_pid = play_track (misc.sound_dev, "alsa",
                            start);
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
   if (misc.audiocd == 1)
   {
      pid_t pid;
      int sp;
      int16_t *p_readbuf;

      sp = misc.speed;
      misc.speed = 1;
      pid = play_track (str, "wav",
                        daisy[misc.current].first_lsn);
      misc.speed = sp;
      do
      {
         if (! (p_readbuf = paranoia_read (misc.par, NULL)))
            break;
         switch (write (misc.pipefd[1], p_readbuf, CDIO_CD_FRAMESIZE_RAW))
         {
         default:
            break;
         } // switch
      } while (++misc.lsn_cursor <= daisy[misc.current].last_lsn);
      kill (pid, SIGQUIT);
      set_drive_speed (4);
      return;
   } // if

   char cmd[MAX_CMD];

   lseek (misc.tmp_wav_fd, SEEK_SET, 0);
   snprintf (cmd, MAX_CMD, "madplay -Q \"%s\" -s %f -t %f -o wav:\"%s\"",
             misc.current_audio_file,
             daisy[misc.current].begin, daisy[misc.current].duration, str);
   if (system (cmd) != 0)
      _exit (0);
   set_drive_speed (4);
} // write_wav

void store_to_disk ()
{                               
   char str[MAX_STR];
   int i, pause_flag;

   pause_flag = misc.playing;
   if (pause_flag > -1)
      pause_resume (misc);
   wclear (misc.screenwin);
   snprintf (str, MAX_STR - 1, "%s.wav", daisy[misc.current].label);
   wprintw (misc.screenwin,
            "\nStoring \"%s\" as \"%s\" into your home-folder...",
            daisy[misc.current].label, str);
   wrefresh (misc.screenwin);
   for (i = 0; str[i] != 0; i++)
      if (str[i] == '/')
         str[i] = '-';
   write_wav (str);
   if (pause_flag > -1)
      pause_resume (misc);
   view_screen (misc);
} // store_to_disk

void help ()
{                      
   int y, x;

   getyx (misc.screenwin, y, x);
   wclear (misc.screenwin);
   waddstr (misc.screenwin, gettext ("\nThese commands are available in this version:\n"));
   waddstr (misc.screenwin, "========================================");
   waddstr (misc.screenwin, "========================================\n\n");
   waddstr (misc.screenwin, gettext ("cursor down     - move cursor to the next item\n"));
   waddstr (misc.screenwin, gettext ("cursor up       - move cursor to the previous item\n"));
   waddstr (misc.screenwin, gettext ("cursor right    - skip to next phrase\n"));
   waddstr (misc.screenwin, gettext ("cursor left     - skip to previous phrase\n"));
   waddstr (misc.screenwin, gettext ("page-down       - view next page\n"));
   waddstr (misc.screenwin, gettext ("page-up         - view previous page\n"));
   waddstr (misc.screenwin, gettext ("enter           - start playing\n"));
   waddstr (misc.screenwin, gettext ("space           - pause/resume playing\n"));
   waddstr (misc.screenwin, gettext ("home            - play on normal speed\n"));
   waddstr (misc.screenwin, "\n");
   waddstr (misc.screenwin, gettext ("Press any key for next page..."));
   nodelay (misc.screenwin, FALSE);
   wgetch (misc.screenwin);
   nodelay (misc.screenwin, TRUE);
   wclear (misc.screenwin);
   waddstr (misc.screenwin, gettext ("\n/               - search for a label\n"));
   waddstr (misc.screenwin, gettext ("d               - store current item to disk\n"));
   waddstr (misc.screenwin, gettext ("D               - decrease playing speed\n"));
   waddstr (misc.screenwin, gettext (
"f               - find the currently playing item and place the cursor there\n"));
   if (misc.audiocd == 0)
      waddstr (misc.screenwin,
            gettext ("g               - go to page number (if any)\n"));
   else
      waddstr (misc.screenwin,
            gettext ("g               - go to time in this song (MM:SS)\n"));
   waddstr (misc.screenwin, gettext ("h or ?          - give this help\n"));
   waddstr (misc.screenwin, gettext ("j               - just play current item\n"));
   waddstr (misc.screenwin,
          gettext ("l               - switch to next level\n"));
   waddstr (misc.screenwin, gettext ("L               - switch to previous level\n"));
   waddstr (misc.screenwin, gettext ("n               - search forwards\n"));
   waddstr (misc.screenwin, gettext ("N               - search backwards\n"));
   waddstr (misc.screenwin, gettext ("o               - select next output sound device\n"));
   waddstr (misc.screenwin, gettext ("p               - place a bookmark\n"));
   waddstr (misc.screenwin, gettext ("q               - quit daisy-player and place a bookmark\n"));
   waddstr (misc.screenwin, gettext ("s               - stop playing\n"));
   waddstr (misc.screenwin, gettext ("U               - increase playing speed\n"));
   waddstr (misc.screenwin, gettext ("\nPress any key to leave help..."));
   nodelay (misc.screenwin, FALSE);
   wgetch (misc.screenwin);
   nodelay (misc.screenwin, TRUE);
   view_screen (misc);
   wmove (misc.screenwin, y, x);
} // help

void previous_item ()
{                               
   if (misc.current == 0)
      return;
   while (daisy[misc.current].level > misc.level)
      misc.current--;
   if (misc.playing == -1)
      misc.displaying = misc.current;
   view_screen (misc);
   wmove (misc.screenwin, daisy[misc.current].y, daisy[misc.current].x);
} // previous_item

void next_item ()
{                           
   if (misc.current >= misc.total_items - 1)
   {
      beep ();
      return;
   } // if
   while (daisy[++misc.current].level > misc.level)
   {
      if (misc.current >= misc.total_items - 1)
      {
         beep ();
         previous_item (misc);
         return;
      } // if
   } // while
   view_screen (misc);
   wmove (misc.screenwin, daisy[misc.current].y, daisy[misc.current].x);
} // next_item

void play_clip ()
{                           
   char str[MAX_STR];

   if (kill (misc.player_pid, 0) == 0)
// if still playing
      return;
   misc.player_pid = -2;
   if (get_next_clips (misc))
   {
      play_now (misc);
      return;
   } // if

// go to next item
   strncpy (my_attribute.clip_begin, "0", 2);
   misc.clip_begin = 0;
   if (++misc.playing >= misc.total_items)
   {
      struct passwd *pw;

      pw = getpwuid (geteuid ());
      quit_daisy_player (misc);
      snprintf (str, MAX_STR - 1, "%s/.daisy-player/%s%s", pw->pw_dir,
                misc.bookmark_title, get_mcn (misc));
      unlink (str);
      _exit (0);
   } // if
   if (daisy[misc.playing].level <= misc.level)
      misc.displaying = misc.current = misc.playing;
   if (misc.just_this_item != -1)
      if (daisy[misc.playing].level <= misc.level)
         misc.playing = misc.just_this_item = -1;
   if (misc.playing > -1)
      open_smil_file (daisy[misc.playing].smil_file,
                      daisy[misc.playing].anchor);
   view_screen (misc);
} // play_clip

void skip_left ()
{
   struct
   {
      char src[MAX_STR], clip_begin[MAX_STR], clip_end[MAX_STR];
   } curr, prev, orig;

   if (misc.audiocd == 1)
      return;
   if (misc.playing < 0)
   {
      beep ();
      return;
   } // if
   if (misc.player_pid > 0)
      while (kill (misc.player_pid, SIGKILL) != 0);
   if (misc.audiocd == 1)
   {
      beep ();
      return;
   } // if
   get_clips (my_attribute.clip_begin, my_attribute.clip_end);
   if (daisy[misc.playing].begin != misc.clip_begin)
   {
// save current values in orig and curr arrays
      strncpy (orig.clip_begin, my_attribute.clip_begin, MAX_STR - 1);
      strncpy (orig.clip_end, my_attribute.clip_end, MAX_STR - 1);
      strncpy (orig.src, my_attribute.src, MAX_STR - 1);
      strncpy (curr.clip_begin, my_attribute.clip_begin, MAX_STR - 1);
      strncpy (curr.clip_end, my_attribute.clip_end, MAX_STR - 1);
      strncpy (curr.src, my_attribute.src, MAX_STR - 1);
      open_smil_file (daisy[misc.playing].smil_file,
                      daisy[misc.playing].anchor);
      while (1)
      {
         if (! get_tag_or_label (misc.reader))
            return;
         if (strcasecmp (my_attribute.class, "pagenum") == 0)
         {
            do
            {
               if (! get_tag_or_label (misc.reader))
                  return;
            } while (strcasecmp (misc.tag, "/par") != 0);
         } // if (strcasecmp (my_attribute.class, "pagenum") == 0)
         if (strcasecmp (misc.tag, "audio") == 0)
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
               open_smil_file (daisy[misc.playing].smil_file,
                               daisy[misc.playing].anchor);
               while (1)
               {
                  if (! get_tag_or_label (misc.reader))
                     return;
                  if (strcasecmp (misc.tag , "audio") == 0)
                  {
                     if (strcasecmp (my_attribute.clip_begin,
                         prev.clip_begin) == 0 &&
                         strcasecmp (my_attribute.src, prev.src) == 0)
                     {
                        get_clips (my_attribute.clip_begin,
                                   my_attribute.clip_end);
                        play_now (misc);
                        return;
                     } // if (strcasecmp (my_attribute.clip_begin, prev.cli ...
                  } // if (strcasecmp (misc.tag , "audio") == 0)
               } // while
            } // if (strcasecmp (orig.clip_begin, curr.clip_begin) == 0 &&
         } // if (strcasecmp (misc.tag, "audio") == 0)
      } // while
   } // if (clip_begin != daisy[misc.playing].begin)

// go to end of previous item
   misc.current = misc.displaying = --misc.playing;
   if (misc.current < 0)
   {
      beep ();
      misc.current = misc.displaying = misc.playing = 0;
      return;
   } // if
   open_smil_file (daisy[misc.current].smil_file, daisy[misc.current].anchor);
   while (1)
   {
// search last clip
      if (! get_tag_or_label (misc.reader))
         break;
      if (strcasecmp (my_attribute.class, "pagenum") == 0)
      {
         do
         {
            if (! get_tag_or_label (misc.reader))
               return;
         } while (strcasecmp (misc.tag, "/par") != 0);
      } // if (strcasecmp (my_attribute.class, "pagenum") == 0)
      if (strcasecmp (misc.tag, "audio") == 0)
      {
         strncpy (curr.clip_begin, my_attribute.clip_begin, MAX_STR - 1);
         strncpy (curr.clip_end, my_attribute.clip_end, MAX_STR - 1);
         strncpy (curr.src, my_attribute.src, MAX_STR - 1);
      } // if (strcasecmp (misc.tag, "audio") == 0)
      if (strcasecmp (my_attribute.id, daisy[misc.current + 1].anchor) == 0)
         break;
   } // while
   misc.just_this_item = -1;
   open_smil_file (daisy[misc.current].smil_file, daisy[misc.current].anchor);
   while (1)
   {
      if (! get_tag_or_label (misc.reader))
         return;
      if (strcasecmp (misc.tag , "audio") == 0)
      {
         if (strcasecmp (my_attribute.clip_begin, curr.clip_begin) == 0 &&
             strcasecmp (my_attribute.src, curr.src) == 0)
         {
            strncpy (misc.current_audio_file, my_attribute.src, MAX_STR - 1);
            get_clips (my_attribute.clip_begin, my_attribute.clip_end);
            view_screen (misc);
            play_now (misc);
            return;
         } // if (strcasecmp (my_attribute.clip_begin, curr.clip_begin) == 0 &&
      } // if (strcasecmp (misc.tag , "audio") == 0)
   } // while
} // skip_left

void skip_right ()
{                            
   if (misc.audiocd == 1)
      return;
   if (misc.playing == -1)
   {
      beep ();
      return;
   } // if
   if (misc.audiocd == 0)
   {
      kill (misc.player_pid, SIGKILL);
      misc.player_pid = -2;
   } // if
} // skip_right

void change_level (char key)
{
   int c, l;

   if (misc.depth == 1)
      return;
   if (key == 'l')
      if (++misc.level > misc.depth)
         misc.level = 1;
   if (key == 'L')
      if (--misc.level < 1)
         misc.level = misc.depth;
   mvwprintw (misc.titlewin, 1, 0, gettext ("Please wait... -------------------------"));
   wprintw (misc.titlewin, "----------------------------------------");
   wrefresh (misc.titlewin);
   c = misc.current;
   l = misc.level;
   if (strcasestr (misc.daisy_version, "2.02"))
      read_daisy_2 (misc);
   if (strcasestr (misc.daisy_version, "3"))
   {
      read_daisy_3 (misc);
      parse_smil_3 (misc);
   } // if
   misc.current = c;
   misc.level = l;
   if (daisy[misc.current].level > misc.level)
      previous_item (misc);
   view_screen (misc);
   wmove (misc.screenwin, daisy[misc.current].y, daisy[misc.current].x);
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
      strncpy (misc.sound_dev, "hw:0", MAX_STR - 1);
      return;
   } // if
   do
   {
      if (! get_tag_or_label (reader))
         break;
   } while (strcasecmp (misc.tag, "prefs") != 0);
   xmlTextReaderClose (reader);
   xmlFreeDoc (doc);
   if (misc.cddb_flag != 'n' && misc.cddb_flag != 'y')
      misc.cddb_flag = 'y';
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
      (writer, BAD_CAST "sound_dev", "%s", misc.sound_dev);
   xmlTextWriterWriteFormatAttribute (writer, BAD_CAST "speed", "%lf",
                                      misc.speed);
   xmlTextWriterWriteFormatAttribute
      (writer, BAD_CAST "cd_dev", "%s", misc.cd_dev);
   xmlTextWriterWriteFormatAttribute
      (writer, BAD_CAST "cddb_flag", "%c", misc.cddb_flag);
   xmlTextWriterEndElement (writer);
   xmlTextWriterEndDocument (writer);
   xmlFreeTextWriter (writer);
} // save_rc

void quit_daisy_player ()
{
   endwin ();
   kill (misc.player_pid, SIGKILL);
   misc.player_pid = -2;
   put_bookmark (misc);
   save_rc (misc);

// reset the CD speed
   set_drive_speed (100);
   close (misc.tmp_wav_fd);
   puts ("");
} // quit_daisy_player

void search (int start, char mode)
{
   int c, found = 0, flag = 0;

   if (misc.playing != -1)
   {
      flag = 1;
      pause_resume (misc);
   } // if
   if (mode == '/')
   {
      mvwaddstr (misc.titlewin, 1, 0, "----------------------------------------");
      waddstr (misc.titlewin, "----------------------------------------");
      mvwprintw (misc.titlewin, 1, 0, gettext ("What do you search? "));
      echo ();
      wgetnstr (misc.titlewin, misc.search_str, 25);
      noecho ();
   } // if
   if (mode == '/' || mode == 'n')
   {
      for (c = start; c <= misc.total_items + 1; c++)
      {
         if (strcasestr (daisy[c].label, misc.search_str))
         {
            found = 1;
            break;
         } // if
      } // for
      if (! found)
      {
         for (c = 0; c < start; c++)
         {
            if (strcasestr (daisy[c].label, misc.search_str))
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
         if (strcasestr (daisy[c].label, misc.search_str))
         {
            found = 1;
            break;
         } // if
      } // for
      if (! found)
      {
         for (c = misc.total_items + 1; c > start; c--)
         {
            if (strcasestr (daisy[c].label, misc.search_str))
            {
               found = 1;
               break;
            } // if
         } // for
      } // if
   } // if
   if (found)
   {
      misc.playing = misc.displaying= misc.current = c;
      misc.clip_begin = daisy[misc.current].begin;
      misc.just_this_item = -1;
      view_screen (misc);
      if (misc.audiocd == 0)
         open_smil_file (daisy[misc.current].smil_file,
                         daisy[misc.current].anchor);
      else
      {
         misc.seconds = time (NULL);
         misc.player_pid = play_track (misc.sound_dev, "alsa",
                daisy[misc.current].first_lsn);
      } // if
   }
   else
   {
      beep ();
      view_screen (misc);
      if (! flag)
         return;
      misc.playing = misc.displaying;
      if (misc.audiocd == 0)
      {
         skip_left (misc);
         skip_right (misc);
      }
      else
      {
         misc.playing = -1;
         pause_resume (misc);
      } // if
   } // if
} // search

void go_to_page_number ()
{                                   
   char filename[MAX_STR], *anchor = 0;
   char pn[15], href[MAX_STR], prev_href[MAX_STR];

   kill (misc.player_pid, SIGKILL);
   misc.player_pid = -2;
   mvwaddstr (misc.titlewin, 1, 0, "----------------------------------------");
   waddstr (misc.titlewin, "----------------------------------------");
   mvwprintw (misc.titlewin, 1, 0, gettext ("Go to page number: "));
   echo ();
   wgetnstr (misc.titlewin, pn, 5);
   noecho ();
   view_screen (misc);
   if (! *pn || atoi (pn) > misc.total_pages || ! atoi (pn))
   {
      beep ();
      skip_left (misc);
      skip_right (misc);
      return;
   } // if
   if (strcasestr (misc.daisy_version, "2.02"))
   {
      xmlTextReaderClose (misc.reader);
      xmlDocPtr doc = xmlRecoverFile (misc.NCC_HTML);
      if (! (misc.reader = xmlReaderWalker (doc)))
      {
         endwin ();
         printf (gettext ("\nCannot read %s\n"), misc.NCC_HTML);
         beep ();
         fflush (stdout);
         _exit (1);
      } // if
      *href = 0;
      while (1)
      {
         if (! get_tag_or_label (misc.reader))
            return;
         if (strcasecmp (misc.tag, "a") == 0)
         {
            strncpy (prev_href, href, MAX_STR - 1);
            strncpy (href, my_attribute.href, MAX_STR - 1);
            do
            {
               if (! get_tag_or_label (misc.reader))
                  return;
            } while (! *misc.label);
            if (strcasecmp (misc.label, pn) == 0)
               break;
         } // if (strcasecmp (misc.tag, "a") == 0)
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
      for (misc.current = misc.total_items - 1; misc.current >= 0; misc.current--)
      {
         if (! daisy[misc.current].page_number)
            continue;
         if (atoi (pn) >= daisy[misc.current].page_number)
            break;
      } // for
      misc.playing = misc.displaying = misc.current;
      view_screen (misc);
      misc.current_page_number = atoi (pn);
      wmove (misc.screenwin, daisy[misc.current].y, daisy[misc.current].x);
      misc.just_this_item = -1;
      open_smil_file (daisy[misc.current].smil_file, anchor);
      free (anchor);
      return;
   } // if (strcasestr (misc.daisy_version, "2.02"))

   if (strcasestr (misc.daisy_version, "3"))
   {
      char *anchor = 0;

      xmlTextReaderClose (misc.reader);
      xmlDocPtr doc = xmlRecoverFile (misc.NCC_HTML);
      if (! (misc.reader = xmlReaderWalker (doc)))
      {
         endwin ();
         printf (gettext ("\nCannot read %s\n"), misc.NCC_HTML);
         beep ();
         fflush (stdout);
         _exit (1);
      } // if
      do
      {
         if (! get_tag_or_label (misc.reader))
         {
            xmlFreeDoc (doc);
            return;
         } // if
      } while (strcasecmp (misc.tag, "pageTarget") != 0 ||
               strcasecmp (my_attribute.value, pn) != 0);
      do
      {
         if (! get_tag_or_label (misc.reader))
         {
            xmlTextReaderClose (misc.reader);
            xmlFreeDoc (doc);
            return;
         } // if
      } while (strcasecmp (misc.tag, "content") != 0);
      xmlFreeDoc (doc);
      if (strchr (my_attribute.src, '#'))
      {
         anchor = strdup (strchr (my_attribute.src, '#') + 1);
         *strchr (my_attribute.src, '#') = 0;
      } // if
      // search current item
      for (misc.current = misc.total_items - 1; misc.current >= 0; misc.current--)
      {
         if (! daisy[misc.current].page_number)
            continue;
         if (atoi (pn) >= daisy[misc.current].page_number)
            break;
      } // for
      misc.playing = misc.displaying = misc.current;
      view_screen (misc);
      misc.current_page_number = atoi (pn);
      wmove (misc.screenwin, daisy[misc.current].y, daisy[misc.current].x);
      misc.just_this_item = -1;
      open_smil_file (my_attribute.src, anchor);
   } // if (strcasestr (misc.daisy_version, "3"))
} // go_to_page_number

void go_to_time ()
{
   char time_str[10];
   int secs;

   kill (misc.player_pid, SIGKILL);
   misc.player_pid = -2;
   mvwaddstr (misc.titlewin, 1, 0, "----------------------------------------");
   waddstr (misc.titlewin, "----------------------------------------");
   mvwprintw (misc.titlewin, 1, 0, gettext ("Go to time (MM:SS): "));
   echo ();
   wgetnstr (misc.titlewin, time_str, 5);
   noecho ();
   view_screen (misc);
   secs = (time_str[0] - 48) * 600 + (time_str[1] - 48)* 60 +
             (time_str[3] - 48) * 10 + (time_str[4] - 48);
   misc.player_pid = play_track (misc.sound_dev, "alsa",
                      daisy[misc.current].first_lsn + (secs * 75));
   misc.seconds = time (NULL) - secs;
   misc.playing = misc.displaying = misc.current;
} // go_to_time

void select_next_output_device ()
{
   FILE *r;
   int n, y;
   size_t bytes;
   char *list[10], *trash;

   wclear (misc.screenwin);
   wprintw (misc.screenwin, "\nSelect a soundcard:\n\n");
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
      wprintw (misc.screenwin, "   %s", list[n]);
      free (list[n]);
   } // for
   fclose (r);
   y = 3;
   nodelay (misc.screenwin, FALSE);
   for (;;)
   {
      wmove (misc.screenwin, y, 2);
      switch (wgetch (misc.screenwin))
      {
      case 13:
         snprintf (misc.sound_dev, 15, "hw:%i", y - 3);
         view_screen (misc);
         nodelay (misc.screenwin, TRUE);
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
         view_screen (misc);
         nodelay (misc.screenwin, TRUE);
         return;
      } // switch
   } // for
} // select_next_output_device

void browse (char *wd)
{
   int old_screen;
   char str[MAX_STR];

   misc.current = 0;
   misc.just_this_item = -1;
   get_bookmark (misc);
   view_screen (misc);
   nodelay (misc.screenwin, TRUE);
   wmove (misc.screenwin, daisy[misc.current].y, daisy[misc.current].x);
   if (misc.audiocd == 1)
   {
      misc.depth = 1;
      view_screen (misc);
   } // if

   for (;;)
   {
      switch (wgetch (misc.screenwin))
      {
      case 13: // ENTER
         misc.just_this_item = -1;
         view_screen (misc);
         misc.playing = misc.displaying = misc.current;
         misc.current_page_number = daisy[misc.playing].page_number;
         if (misc.player_pid > -1)
            kill (misc.player_pid, SIGKILL);
         misc.player_pid = -2;
         if (misc.discinfo)
         {
            snprintf (str, MAX_STR - 1,
                      "cd \"%s\"; \"%s\" \"%s\"/\"%s\" -d %s",
                      wd, PACKAGE, misc.daisy_mp,
                      daisy[misc.current].daisy_mp, misc.sound_dev);
            switch (system (str))
            {
            default:
               break;
            } // switch

            snprintf (str, MAX_STR - 1,
                      "cd \"%s\"; \"%s\" \"%s\" -d %s\n", wd, PACKAGE,
                      misc.daisy_mp, misc.sound_dev);
            switch (system (str))
            {
            default:
               break;
            } // switch
            quit_daisy_player (misc);
            _exit (0);
         } // if
         if (misc.audiocd == 1)
         {
            misc.player_pid = play_track (misc.sound_dev, "alsa",
                daisy[misc.current].first_lsn);
            misc.seconds = time (NULL);
         } // if
         if (misc.audiocd == 0)
         {
            open_smil_file (daisy[misc.current].smil_file,
                            daisy[misc.current].anchor);
         } // if
         misc.start_time = 0;
         break;
      case '/':
         if (misc.discinfo)
         {
            beep ();
            break;
         } // if
         search (misc.current + 1, '/');
         break;
      case ' ':
      case KEY_IC:
      case '0':
         if (misc.discinfo)
         {
            beep ();
            break;
         } // if
         pause_resume (misc);
         break;
      case 'd':
         if (misc.discinfo)
         {
            beep ();
            break;
         } // if
         store_to_disk (misc);
         break;
      case 'e':
      case '.':
      case KEY_DC:
         if (misc.discinfo)
         {
            beep ();
            break;
         } // if
         xmlTextReaderClose (misc.reader);
         quit_daisy_player ();
         while (kill (misc.player_pid, 0) == 0);
         if (chdir ("/") == 0)
         {
            char cmd[MAX_CMD + 1];

            snprintf (cmd, MAX_CMD, "eject %s", misc.cd_dev);
            switch (system (cmd))
            {
            default:
               _exit (0);
            } // switch
         } // if
         _exit (0);
      case 'f':
         if (misc.playing == -1)
         {
            beep ();
            break;
         } // if
         misc.current = misc.playing;
         previous_item (misc);
         view_screen (misc);
         break;
      case 'g':
         if (misc.audiocd == 1)
         {
            go_to_time (misc);
            break;
         } // if
         if (misc.total_pages)
            go_to_page_number (misc);
         else
            beep ();
         break;
      case 'h':
      case '?':
      {
         int flag = 0;

         if (misc.playing < 0)
            flag = 1;
         if (! misc.discinfo)
            pause_resume (misc);
         misc.player_pid = -2;
         help (misc);
         if (flag)
            break;
         misc.playing = misc.displaying;
         if (misc.audiocd == 0)
         {
            skip_left (misc);
            skip_right (misc);
         } // if
         if (misc.audiocd == 1)
         {
            misc.playing = -1;
            pause_resume (misc);
         } // if
         break;
      }
      case 'j':
      case '5':
      case KEY_B2:
         if (misc.discinfo)
         {
            beep ();
            break;
         } // if
         if (misc.just_this_item != -1)
            misc.just_this_item = -1;
         else
            misc.just_this_item = misc.current;
         mvwprintw (misc.screenwin, daisy[misc.displaying].y, 0, " ");
         if (misc.playing == -1)
         {
            misc.just_this_item = misc.displaying = misc.playing = misc.current;
            kill (misc.player_pid, SIGKILL);
            misc.player_pid = -2;
            if (misc.audiocd == 1)
            {
               misc.player_pid = play_track (misc.sound_dev, "alsa",
                   daisy[misc.current].first_lsn);
            }
            else
               open_smil_file (daisy[misc.current].smil_file,
                               daisy[misc.current].anchor);
         } // if
         wattron (misc.screenwin, A_BOLD);
         if (misc.just_this_item != -1 &&
             daisy[misc.displaying].screen == daisy[misc.playing].screen)
            mvwprintw (misc.screenwin, daisy[misc.displaying].y, 0, "J");
         else
            mvwprintw (misc.screenwin, daisy[misc.displaying].y, 0, " ");
         wrefresh (misc.screenwin);
         wattroff (misc.screenwin, A_BOLD);
         misc.current_page_number = daisy[misc.playing].page_number;
         break;
      case 'l':
         if (misc.discinfo)
         {
            beep ();
            break;
         } // if
         change_level ('l');
         break;
      case 'L':
         if (misc.discinfo)
         {
            beep ();
            break;
         } // if
         change_level ('L');
         break;
      case 'n':
         if (misc.discinfo)
         {
            beep ();
            break;
         } // if
         search (misc.current + 1, 'n');
         break;
      case 'N':
         if (misc.discinfo)
         {
            beep ();
            break;
         } // if
         search (misc.current - 1, 'N');
         break;
      case 'o':
         if (misc.playing == -1)
         {
            beep ();
            break;
         } // if
         pause_resume (misc);
         select_next_output_device (misc);
         misc.playing = misc.displaying;
         if (misc.audiocd == 0)
         {
            skip_left (misc);
            skip_right (misc);
         } // if
         if (misc.audiocd == 1)
         {
            misc.playing = -1;
            pause_resume (misc);
         } // if
         break;
      case 'p':
         put_bookmark (misc);
         break;
      case 'q':
         quit_daisy_player (misc);
         _exit (0);
      case 's':
         if (misc.audiocd == 0)
         {
            kill (misc.player_pid, SIGKILL);
            misc.player_pid = -2;
         } // if
         if (misc.audiocd == 1)
         {
            kill (misc.player_pid, SIGKILL);
         } // if
         misc.playing = misc.just_this_item = -1;
         view_screen (misc);
         wmove (misc.screenwin, daisy[misc.current].y, daisy[misc.current].x);
         break;
      case KEY_DOWN:
      case '2':
         next_item (misc);
         break;
      case KEY_UP:
      case '8':
         if (misc.current == 0)
         {
            beep ();
            break;
         } // if
         misc.current--;
         wmove (misc.screenwin, daisy[misc.current].y, daisy[misc.current].x);
         previous_item (misc);
         break;
      case KEY_RIGHT:
      case '6':
         if (misc.audiocd == 0)
            skip_right (misc);
         if (misc.audiocd == 1)
         {
            switch (ftruncate (misc.pipefd[0], 0))
            {
            default:
               break;
            } // switch
            switch (ftruncate (misc.pipefd[1], 0))
            {
            default:
               break;
            } // switch
            paranoia_seek (misc.par, 225, SEEK_CUR);
            misc.lsn_cursor += 225;
            switch (ftruncate (misc.pipefd[0], 0))
            {
            default:
               break;
            } // switch
            switch (ftruncate (misc.pipefd[1], 0))
            {
            default:
               break;
            } // switch
         } // if
         break;
      case KEY_LEFT:
      case '4':
         if (misc.audiocd == 0)
            skip_left (misc);
         if (misc.audiocd == 1)
         {
            switch (ftruncate (misc.pipefd[0], 0))
            {
            default:
               break;
            } // switch
            switch (ftruncate (misc.pipefd[1], 0))
            {
            default:
               break;
            } // switch
            paranoia_seek (misc.par, -225, SEEK_CUR);
            misc.lsn_cursor -= 225;
            switch (ftruncate (misc.pipefd[0], 0))
            {
            default:
               break;
            } // switch
            switch (ftruncate (misc.pipefd[1], 0))
            {
            default:
               break;
            } // switch
         } // if
         break;
      case KEY_NPAGE:
      case '3':
         if (daisy[misc.current].screen == daisy[misc.total_items - 1].screen)
         {
            beep ();
            break;
         } // if
         old_screen = daisy[misc.current].screen;
         while (daisy[++misc.current].screen == old_screen);
         view_screen (misc);
         wmove (misc.screenwin, daisy[misc.current].y, daisy[misc.current].x);
         break;
      case KEY_PPAGE:
      case '9':
         if (daisy[misc.current].screen == 0)
         {
            beep ();
            break;
         } // if
         old_screen = daisy[misc.current].screen;
         while (daisy[--misc.current].screen == old_screen);
         misc.current -= misc.max_y - 1;
         view_screen (misc);
         wmove (misc.screenwin, daisy[misc.current].y, daisy[misc.current].x);
         break;
      case ERR:
         break;
      case 'U':
      case '+':
         if (misc.speed >= 2)
         {
            beep ();
            break;
         } // if
         misc.speed += 0.1;
         if (misc.audiocd == 1)
         {
            kill (misc.player_pid, SIGKILL);
            lsn_t start;

            start = (lsn_t) (misc.start_time + (float) (time (NULL) - misc.seconds) * 75) +
                    daisy[misc.playing].first_lsn;
            misc.player_pid = play_track (misc.sound_dev, "alsa",
                            start);
            view_screen ();
            break;
         } // if
         skip_left (misc);
         kill (misc.player_pid, SIGKILL);
         view_screen ();
         break;
      case 'D':
      case '-':
         if (misc.speed <= 0.3)
         {
            beep ();
            break;
         } // if
         misc.speed -= 0.1;
         if (misc.audiocd == 1)
         {
            kill (misc.player_pid, SIGKILL);
            lsn_t start;

            start = (lsn_t) (misc.start_time + (float) (time (NULL) - misc.seconds) * 75) +
                 daisy[misc.playing].first_lsn;
            misc.player_pid = play_track (misc.sound_dev, "alsa",
                            start);
            view_screen ();
            break;
         } // if
         skip_left (misc);
         kill (misc.player_pid, SIGKILL);
         view_screen ();
         break;
      case KEY_HOME:
      case '*':
      case '7':
         misc.speed = 1;
         if (misc.audiocd == 1)
         {
            kill (misc.player_pid, SIGKILL);
            lsn_t start;

            start = (lsn_t) (misc.start_time + (float) (time (NULL) - misc.seconds) * 75) +
                 daisy[misc.playing].first_lsn;
            misc.player_pid = play_track (misc.sound_dev, "alsa",
                            start);
            view_screen ();
            break;
         } // if
         kill (misc.player_pid, SIGKILL);
         skip_left (misc);
         skip_right (misc);
         view_screen ();
         break;
      default:
         beep ();
         break;
      } // switch

      if (misc.playing > -1 && misc.audiocd == 0)
      {
         play_clip (misc);
         view_time (misc);
      } // if

      if (misc.playing == -1 || misc.audiocd == 0)
      {
         fd_set rfds;
         struct timeval tv;

         FD_ZERO (&rfds);
         FD_SET (0, &rfds);
         tv.tv_sec = 0;
         tv.tv_usec = 100000;
// pause till a key has been pressed or 0.1 misc.seconds are elapsed
         select (1, &rfds, NULL, NULL, &tv);
      } // if

      if (misc.playing > -1 && misc.audiocd == 1)
      {
         int16_t *p_readbuf;

         if (! (p_readbuf = paranoia_read (misc.par, NULL)))
            break;
         switch (write (misc.pipefd[1], p_readbuf, CDIO_CD_FRAMESIZE_RAW))
         {
         default:
            break;
         } // switch
         misc.lsn_cursor++;
         if (misc.lsn_cursor > daisy[misc.playing].last_lsn)
         {
            misc.start_time = 0;
            misc.current = misc.displaying = ++misc.playing;
            misc.lsn_cursor = daisy[misc.playing].first_lsn;
            if (misc.current >= misc.total_items)
            {
               struct passwd *pw;

               pw = getpwuid (geteuid ());
               quit_daisy_player (misc);
               snprintf (str, MAX_STR - 1, "%s/.daisy-player/%s%s",
                         pw->pw_dir, misc.bookmark_title, get_mcn (misc));
               unlink (str);
               _exit (0);
            } // if
            if (misc.just_this_item > -1)
            {
               kill (misc.player_pid, SIGKILL);
               misc.playing = misc.just_this_item = -1;
            } // if
            view_screen (misc);
            misc.seconds = time (NULL);
         } // if
         view_time (misc);
      } // if
   } // for
} // browse

void usage (char *argv)
{
   printf (gettext ("Daisy-player - Version %s\n"), PACKAGE_VERSION);
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
      strncpy (misc.daisy_mp, strchr (str, ' ') + 1, MAX_STR - 1);
      *strchr (misc.daisy_mp, ' ') = 0;
      free (str);
      return misc.daisy_mp;
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
      if (strcasecmp (misc.tag, "title") == 0)
      {
         do
         {
            if (! get_tag_or_label (di))
               break;
         } while ( !*misc.label);
         strncpy (misc.daisy_title, misc.label, MAX_STR - 1);
      } // if (strcasecmp (misc.tag, "title") == 0)
      if (strcasecmp (misc.tag, "a") == 0)
      {
         strncpy (daisy[misc.current].filename, my_attribute.href, MAX_STR - 1);
         xmlDocPtr doc = xmlRecoverFile (daisy[misc.current].filename);
         if (! (ncc = xmlReaderWalker (doc)))
         {
            endwin ();
            beep ();
            printf (gettext ("\nCannot read %s\n"), daisy[misc.current].filename);
            fflush (stdout);
            _exit (1);
         } // if
         do
         {
            *misc.ncc_totalTime = 0;
            if (! get_tag_or_label (ncc))
               break;
         } while (! *misc.ncc_totalTime);
         daisy[misc.current].duration = read_time (misc.ncc_totalTime);
         t += daisy[misc.current].duration;
         xmlTextReaderClose (ncc);
         xmlFreeDoc (doc);
         do
         {
            if (! get_tag_or_label (di))
               break;
         } while (! *misc.label);
         strncpy (daisy[misc.current].label, misc.label, MAX_STR - 1);
         strncpy (daisy[misc.current].daisy_mp, dirname (daisy[misc.current].filename),
                  MAX_STR - 1);
         daisy[misc.current].level = 1;
         daisy[misc.current].x = 0;
         daisy[misc.current].y = misc.displaying;
         daisy[misc.current].screen = misc.current / misc.max_y;
         misc.current++;
         misc.displaying++;
      } // if (strcasecmp (misc.tag, "a") == 0)
   } // while
   xmlTextReaderClose (di);
   xmlFreeDoc (doc);
   misc.total_items = misc.current;
   misc.total_time = t;
   h = t / 3600;
   t -= h * 3600;
   m = t / 60;
   t -= m * 60;
   s = t;
   snprintf (misc.ncc_totalTime, MAX_STR - 1, "%02d:%02d:%02d", h, m, s);
   misc.depth = 1;
   view_screen (misc);
} // handle_discinfo

int main (int argc, char *argv[])
{
   int opt;
   char str[MAX_STR], DISCINFO_HTML[MAX_STR], start_wd[MAX_STR];

// jos   LIBXML_TEST_VERSION // libxml2
   misc.daisy_player_pid = getpid ();
   misc.speed = 1;
   misc.playing = misc.just_this_item = -1;
   misc.discinfo = 0;
   strncpy (misc.cd_dev, "/dev/sr0", 15);
   atexit (quit_daisy_player);
   signal (SIGCHLD, player_ended);
   signal (SIGTERM, quit_daisy_player);
   read_rc (misc);
   setlocale (LC_ALL, "");
   setlocale (LC_NUMERIC, "C");
   textdomain (PACKAGE);
   strncpy (start_wd, get_current_dir_name (), MAX_STR - 1);
   opterr = 0;
   while ((opt = getopt (argc, argv, "c:d:ny")) != -1)
   {
      switch (opt)   
      {
      case 'c':
         strncpy (misc.cd_dev, optarg, 15);
         break;
      case 'd':
         strncpy (misc.sound_dev, optarg, 15);
         break;
      case 'n':
         misc.cddb_flag = 'n';
         break;
      case 'y':
      case 'j':
         misc.cddb_flag = 'y';
         switch (system ("cddbget -c null > /dev/null 2>&1"))
         // if cddbget is installed
         {
         case 0:
            break;
         default:
            misc.cddb_flag = 'n';
         } // switch
         break;
      default:
         usage (PACKAGE);
      } // switch
   } // while
   initscr ();
   if (! (misc.titlewin = newwin (2, 80,  0, 0)) ||
       ! (misc.screenwin = newwin (23, 80, 2, 0)))
   {
      endwin ();
      beep ();
      printf ("No curses\n");
      fflush (stdout);
      _exit (1);
   } // if
   fclose (stderr);
   getmaxyx (misc.screenwin, misc.max_y, misc.max_x);
   printw ("(C)2003-2014 J. Lemmens\n");
   printw (gettext ("Daisy-player - Version %s\n"), PACKAGE_VERSION);
   printw (gettext ("A parser to play Daisy CD's with Linux\n"));
   if (system ("eject -h 2> /dev/null") != 256)
   {
      endwin ();
      beep ();
      printf (gettext ("\nDaisy-player needs the \"eject\" programme.\n"));
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
   misc.audiocd = 0;
   if (argv[optind])
// if there is an argument
   {
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
         usage (PACKAGE);
      } // if
      if (strcasestr (magic_file (myt, argv[optind]), "directory"))
      {
         strncpy (misc.daisy_mp, argv[optind], MAX_STR - 1);
      }
      else
      if (strcasestr (magic_file (myt, argv[optind]), "Zip archive") ||
          strcasestr (magic_file (myt, argv[optind]), "RAR archive data") ||
          strcasestr (magic_file (myt, argv[optind]),
                      "Microsoft Cabinet archive data") ||
          strcasestr (magic_file (myt, argv[optind]),
                      "gzip compressed data") ||
          strcasestr (magic_file (myt, argv[optind]),
                      "bzip2 compressed data") ||
          strcasestr (magic_file (myt, argv[optind]), "ISO 9660"))
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
         wrefresh (misc.screenwin);

         DIR *dir;
         struct dirent *dirent;
         int entries = 0;

         if (! (dir = opendir (str)))
         {
            int e;

            e = errno;
            endwin ();
            beep ();
            printf ("\n%s: %s\n", str, strerror (e));
            fflush (stdout);
            _exit (1);
         } // if
         while ((dirent = readdir (dir)) != NULL)
         {
            if (strcasecmp (dirent->d_name, ".") == 0 ||
                strcasecmp (dirent->d_name, "..") == 0)
               continue;
            snprintf (misc.daisy_mp, MAX_STR - 1, "%s/%s", str, dirent->d_name);
            entries++;
         } // while
         if (entries > 1)
            snprintf (misc.daisy_mp, MAX_STR - 1, "%s", str);
         closedir (dir);
      } // if unar
      else
      {
         endwin ();
         printf (gettext ("\nNo DAISY-CD or Audio-cd found\n"));
         beep ();
         fflush (stdout);
         usage (PACKAGE);
      } // if
      magic_close (myt);
   } // if there is an argument
   else
// try the default block device (/dev/sr0)
   {
      char cmd[MAX_CMD];
      time_t start;
      FILE *r;

      snprintf (cmd, MAX_CMD, "eject -t %s", misc.cd_dev);
      switch (system (cmd))
      {
      default:
         break;
      } // switch
      start = time (NULL);
      if ((r = fopen ("/run/udev/data/b11:0", "r")) == NULL)
// block 11 = cdrom
      {
         int e;

         e = errno;
         endwin ();
         beep ();
         printf ("/run/udev/data/b11:0: %s\n\n", gettext (strerror (e)));
         fflush (stdout);
         usage (PACKAGE);
      } // if
      while (1)
      {
         char *str;
         size_t s;

         str = malloc (MAX_STR);
         s = MAX_STR - 1;
         switch (getline (&str, &s, r))
         {
         default:
            break;
         } // switch
         if (strcasestr (str, "TRACK_COUNT_DATA=1"))
         {
            if (! get_mount_point ())
// if not found a mounted cd, try to mount one
            {
               char cmd[MAX_CMD + 1], *tmp;

               tmp = strdup ("/tmp/daisy-player.XXXXXX");
               strncpy (misc.daisy_mp, mkdtemp (tmp), MAX_STR);
               snprintf (cmd, MAX_CMD,
               "udisksctl mount -b %s > /dev/null", misc.cd_dev);
               if (system (cmd))
               {
                  endwin ();
                  beep ();
                  printf (gettext
                     ("\nDaisy-player needs the \"udisksctl\" programme.\n"));      printf (gettext ("Please install it and try again.\n"));
                  fflush (stdout);
                  _exit (1);
               } // if
               get_mount_point ();
            } // if
            break;
         } // if TRACK_COUNT_DATA=1

         if (strcasestr (str, "TRACK_COUNT_AUDIO"))
         {
// probably an Audio-CD
            misc.audiocd = 1;
            printw (gettext ("\nFound an Audio-CD. "));
            if (misc.cddb_flag == 'y')
               printw (gettext ("Get titles from freedb.freedb.org..."));
            refresh ();
            strncpy (misc.bookmark_title, "Audio-CD", MAX_STR - 1);
            strncpy (misc.daisy_title, "Audio-CD", MAX_STR - 1);
            init_paranoia (misc.cd_dev);
            get_toc_audiocd (misc.cd_dev);
            strncpy (misc.daisy_mp, "/tmp", MAX_STR - 1);
            break;
         } // if TRACK_COUNT_AUDIO
         if (time (NULL) - start >= 30)
         {
            endwin ();
            puts (gettext ("No Daisy CD in drive."));
            fflush (stdout);
            beep ();
            _exit (0);
         } // if
         if (feof (r))
            r = freopen ("/run/udev/data/b11:0", "r", r);
      } // while
      fclose (r);
   } // if use misc.cd_dev
   keypad (misc.screenwin, TRUE);
   meta (misc.screenwin,       TRUE);
   nonl ();
   noecho ();
   misc.player_pid = -2;
   if (chdir (misc.daisy_mp) == -1)
   {
      int e;

      e = errno;
      endwin ();
      beep ();
      printf ("\ndaisy_mp %s: %s\n", misc.daisy_mp, strerror (e));
      fflush (stdout);
      _exit (1);
   } // if
   if (misc.audiocd == 0)
   {
      snprintf (DISCINFO_HTML, MAX_STR - 1, "discinfo.html");
      if (access (DISCINFO_HTML, R_OK) == 0)
         handle_discinfo (DISCINFO_HTML);
      if (! misc.discinfo)
      {
         snprintf (misc.NCC_HTML, MAX_STR - 1, "ncc.html");
         if (access (misc.NCC_HTML, R_OK) == 0)
         {
            xmlDocPtr doc = xmlRecoverFile (misc.NCC_HTML);
            if (! (misc.reader = xmlReaderWalker (doc)))
            {
               endwin ();
               beep ();
               printf (gettext ("\nCannot read %s\n"), misc.NCC_HTML);
               fflush (stdout);
               _exit (1);
            } // if
            while (1)
            {
               if (! get_tag_or_label (misc.reader))
                  break;
               if (strcasecmp (misc.tag, "title") == 0)
               {
                  if (! get_tag_or_label (misc.reader))
                     break;
                  if (*misc.label)
                  {
                     int x;

                     for (x = strlen (misc.label) - 1; x >= 0; x--)
                     {
                        if (isspace (misc.label[x]))
                           misc.label[x] = 0;
                        else
                           break;
                     } // for
                     strncpy (misc.bookmark_title, misc.label, MAX_STR - 1);
                     break;
                  } // if
               } // if
            } // while
            read_daisy_2 (misc);
         }
         else
         {
            read_daisy_3 (misc);
            strncpy (misc.daisy_version, "3", 2);
         } // if
         if (misc.total_items == 0)
            misc.total_items = 1;
      } // if (! misc.discinfo);
   } // if misc.audiocd == 0
   wattron (misc.titlewin, A_BOLD);
   snprintf (str, MAX_STR - 1, gettext
             ("Daisy-player - Version %s - (C)2014 J. Lemmens"),
             PACKAGE_VERSION);
   mvwprintw (misc.titlewin, 0, 0, str);
   wrefresh (misc.titlewin);

   if (strlen (misc.daisy_title) + strlen (str) >= 80)
      mvwprintw (misc.titlewin, 0,
                 80 - strlen (misc.daisy_title) - 4, "... ");
   mvwprintw (misc.titlewin, 0, 80 - strlen (misc.daisy_title),
              "%s", misc.daisy_title);
   wrefresh (misc.titlewin);
   mvwaddstr (misc.titlewin, 1, 0,
              "----------------------------------------");
   waddstr (misc.titlewin, "----------------------------------------");
   mvwprintw (misc.titlewin, 1, 0, gettext ("Press 'h' for help "));
   misc.level = 1;
   *misc.search_str = 0;
   snprintf (misc.tmp_wav, MAX_STR, "/tmp/daisy-player_XXXXXX");
   if ((misc.tmp_wav_fd = mkstemp (misc.tmp_wav)) == 01)
   {
      int e;

      e = errno;
      endwin ();
      beep ();
      printf ("%s\n", strerror (e));
      fflush (stdout);
      _exit (0);
   } // if
   browse (start_wd);
   return 0;
} // main
