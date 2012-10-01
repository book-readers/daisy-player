/* daisy-player - A parser to play Daisy cd's.
 *  Copyright (C) 2003-2012 J. Lemmens
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

#define VERSION "7.2.2"

int discinfo = 0, displaying = 0, phrase_nr, tts_no;
int playing, just_this_item, current_playorder = 1;
int current_page_number;
float speed;
WINDOW *screenwin, *titlewin;
xmlTextReaderPtr reader;
char label[max_phrase_len], bookmark_title[100], search_str[30];
char tag[255], element[1024], tmp_wav[255];
char daisy_version[25];
char ncx_name[255], opf_name[255];
pid_t player_pid, daisy_player_pid;
float clip_begin, clip_end;
char daisy_title[255], prog_name[255], daisy_language[255];
int current, max_y, max_x, total_items, level, displaying;
char NCC_HTML[255], ncc_totalTime[100];
char sound_dev[16], daisy_mp[255];
char current_audio_file[100];
time_t seconds;
float played_time, start_time;
float read_time (char *);
daisy_t daisy[2000];
my_attribute_t my_attribute;
int depth, total_pages;

void realname (char *);
int get_tag_or_label (xmlTextReaderPtr);
void get_clips (char *, char *);
void save_rc ();
void quit_daisy_player ();
void get_page_number_3 (xmlTextReaderPtr);
void parse_smil_3 ();
void open_smil_file (char *, char *);
void parse_ncx (char *);
void parse_opf (char *, char *);

void playfile (char *filename, char *tempo)
{
  sox_format_t *sox_in, *sox_out; /* input and output files */
  sox_effects_chain_t *chain;
  sox_effect_t *e;
  char *args[100], str[100];

  sox_globals.verbosity = 0;
  if (sox_init ())
     return;
  if (! (sox_in = sox_open_read (filename, NULL, NULL, NULL)))
    return;
  while (! (sox_out =
         sox_open_write (sound_dev, &sox_in->signal,
                         NULL, "alsa", NULL, NULL)))
  {
    strncpy (sound_dev, "hw:0", 8);
    save_rc ();
    if (sox_out)
      sox_close (sox_out);
  } // while

  chain = sox_create_effects_chain (&sox_in->encoding, &sox_out->encoding);

  e = sox_create_effect (sox_find_effect ("input"));
  args[0] = (char *) sox_in, sox_effect_options (e, 1, args);
  sox_add_effect (chain, e, &sox_in->signal, &sox_in->signal);

/* Don't use the sox trim effect. It works nice, but is far too slow
  char str2[100];
  snprintf (str,  90, "%f", clip_begin);
  snprintf (str2, 90, "%f", clip_end - clip_begin);
  e = sox_create_effect (sox_find_effect ("trim"));
  args[0] = str;
  args[1] = str2;
  sox_effect_options (e, 2, args);
  sox_add_effect (chain, e, &sox_in->signal, &sox_in->signal);
*/

  e = sox_create_effect (sox_find_effect ("tempo"));
  args[0] = tempo, sox_effect_options (e, 1, args);
  sox_add_effect (chain, e, &sox_in->signal, &sox_in->signal);

  snprintf (str, 90, "%lf", sox_out->signal.rate);
  e = sox_create_effect (sox_find_effect ("rate"));
  args[0] = str, sox_effect_options (e, 1, args);
  sox_add_effect (chain, e, &sox_in->signal, &sox_in->signal);

  snprintf (str, 90, "%i", sox_out->signal.channels);
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

void read_daisy_3 (int flag, char *daisy_mp)
{
   DIR *dir;
   struct dirent *dirent;

   if (! (dir = opendir (daisy_mp)))
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf (gettext ("\nCannot read %s\n"), daisy_mp);
      fflush (stdout);
      _exit (1);
   } // if
   while ((dirent = readdir (dir)) != NULL)
   {
      if (strcasecmp (dirent->d_name +
                      strlen (dirent->d_name) - 4, ".ncx") == 0)
      {
         strncpy (ncx_name, dirent->d_name, 250);
         strncpy (NCC_HTML, dirent->d_name, 250);
      } // if   
      if (strcasecmp (dirent->d_name +
                      strlen (dirent->d_name) - 4, ".opf") == 0)
         strncpy (opf_name, dirent->d_name, 250);
   } // while
   closedir (dir);
   parse_ncx (ncx_name);
   total_items = current;
} // read_daisy_3

void put_bookmark ()
{
   char str[255];
   xmlTextWriterPtr writer;
   struct passwd *pw;;

   pw = getpwuid (geteuid ());
   snprintf (str, 250, "%s/.daisy-player", pw->pw_dir);
   mkdir (str, 0755);
   snprintf (str, 250, "%s/.daisy-player/%s", pw->pw_dir, bookmark_title);
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
   xmlTextWriterWriteFormatAttribute
      (writer, BAD_CAST "clip-begin", "%f", clip_begin);
   xmlTextWriterWriteFormatAttribute (writer, BAD_CAST "level", "%d", level);
   xmlTextWriterEndElement (writer);
   xmlTextWriterEndDocument (writer);
   xmlFreeTextWriter (writer);
} // put_bookmark

void get_bookmark ()
{
   char str[255];
   float begin;
   xmlTextReaderPtr local;
   xmlDocPtr doc;
   struct passwd *pw;;

   pw = getpwuid (geteuid ());
   if (! *bookmark_title)
      return;
   snprintf (str, 250, "%s/.daisy-player/%s", pw->pw_dir, bookmark_title);
   realname (str);
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
   get_clips (my_attribute.clip_begin, my_attribute.clip_end);
   begin = clip_begin;
   if (current <= 0)
      current = 0;
   if (current >= total_items)
      current = 0;
   open_smil_file (daisy[current].smil_file, daisy[current].anchor);
   while (1)
   {
      if (! get_tag_or_label (reader))
         return;
      if (strcasecmp (tag, "audio") == 0)
      {
         strncpy (current_audio_file, my_attribute.src, 90);
         get_clips (my_attribute.clip_begin, my_attribute.clip_end);
         if (clip_begin == begin)
            break;
      } // if
   } // while
   if (level < 1)
      level = 1;
   displaying = playing = current;
   current_page_number = daisy[playing].page_number;
   just_this_item = -1;
   play_now ();
} // get_bookmark

void get_page_number_2 (char *p)
{
// function for daisy 2.02
   char *anchor = 0, *id1, *id2;
   xmlTextReaderPtr page;

   xmlDocPtr doc = xmlRecoverFile (daisy[playing].smil_file);
   if (! (page = xmlReaderWalker (doc)))
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf (gettext ("\nCannot read %s\n"), daisy[playing].smil_file);
      fflush (stdout);
      _exit (1);
   } // if
   id1 = strdup (p);
   do
   {
      if (! get_tag_or_label (page))
         return;
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
      playfile (PREFIX"share/daisy-player/error.wav", "1");
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
         strncpy (current_audio_file, my_attribute.src, 90);
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

   for (x = 0; x < total_items; x++)
   {
      if (*daisy[x].smil_file == 0)
         continue;
      realname (daisy[x].smil_file);
      xmlDocPtr doc = xmlRecoverFile (daisy[x].smil_file);
      if (! (parse = xmlReaderWalker (doc)))
      {
         endwin ();
         playfile (PREFIX"share/daisy-player/error.wav", "1");
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
            strncpy (current_audio_file, my_attribute.src, 90);
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
   float remain_time = 0, curr_time;

   wattron (screenwin, A_BOLD);
   curr_time = start_time + (float) (time (NULL) - seconds);
   mvwprintw (screenwin, daisy[playing].y, 68, "%02d:%02d",
              (int) curr_time / 60, (int) curr_time % 60);
   remain_time =  daisy[playing].duration - curr_time;
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
   mvwprintw (titlewin, 1, 47, gettext (" total length: %s "), ncc_totalTime);
   mvwprintw (titlewin, 1, 74, " %d/%d ", \
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

void read_daisy_2 (int flag)
{
// function for daisy 2.02
   int indent = 0, found_page_normal = 0;
   xmlTextReaderPtr ncc;

   realname (NCC_HTML);
   xmlDocPtr doc = xmlRecoverFile (NCC_HTML);
   if (! (ncc = xmlReaderWalker (doc)))
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
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
         strncpy (daisy[current].smil_file, my_attribute.href, 250);
         if (strchr (daisy[current].smil_file, '#'))
         {
            strncpy (daisy[current].anchor,
                     strchr (daisy[current].smil_file, '#') + 1, 250);
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
   played_time += clip_end - clip_begin;
   start_time = clip_begin;
   switch (player_pid = fork ())
   {
   case -1:
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      puts ("fork()");
      fflush (stdout);
      _exit (1);
   case 0:
      break;
   default:
      return;
   } // switch

   char tempo_str[15], cmd[255];

   view_page (my_attribute.id);
   snprintf (cmd, 250, "madplay -Q %s -s %f -t %f -o \"%s\"",
                  current_audio_file, clip_begin,
                  clip_end - clip_begin, tmp_wav);
   if (system (cmd) != 0)
      _exit (0);
   snprintf (tempo_str, 10, "%lf", speed);
   playfile (tmp_wav, tempo_str);
/* When the sox trim effedct is used, use this
   playfile (current_audio_file, tempo_str);
*/
   _exit (0);
} // play_now

void open_smil_file (char *smil_file, char *anchor)
{
   realname (smil_file);
   xmlTextReaderClose (reader);
   xmlDocPtr doc = xmlRecoverFile (smil_file);
   if (! (reader = xmlReaderWalker (doc)))
   {
      endwin ();
      printf ("open_smil_file(): %s: %s\n", smil_file, strerror (errno));
      playfile (PREFIX"share/daisy-player/error.wav", "1");
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
      kill (player_pid, SIGKILL);
      player_pid = -2;
   }
   else
   {
      playing = displaying;
      skip_left ();
      skip_right ();
   } // if
} // pause_resume

void write_wav (char *outfile)
{
  sox_format_t *out = NULL;
  sox_format_t *in;
  sox_sample_t samples[2048];
  size_t number_read;
  char str[255];
  struct passwd *pw;

  pw = getpwuid (geteuid ());
  snprintf (str, 250, "%s/%s", pw->pw_dir, outfile);
  in = sox_open_read (my_attribute.src, NULL, NULL, NULL);
  out = sox_open_write (str, &in->signal, NULL, NULL, NULL, NULL);
  while ((number_read = sox_read (in, samples, 2048)))
    sox_write (out, samples, number_read);
  sox_close (in);
  sox_close (out);
} // write_wav

void store_to_disk ()
{
   char str[100];

   wclear (screenwin);
   snprintf (str, 90, "%s.wav", daisy[current].label);
   wprintw (screenwin,
            "\nStoring \"%s\" as \"%s\" into your home-folder...",
            daisy[current].label, str);
   wrefresh (screenwin);
   pause_resume ();
   write_wav (str);
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
   waddstr (screenwin, gettext ("f               - find the currently playing item and place the cursor there\n"));
   waddstr (screenwin, gettext ("g               - go to page number (if any)\n"));
   waddstr (screenwin, gettext ("h or ?          - give this help\n"));
   waddstr (screenwin, gettext ("j               - just play current item\n"));
   waddstr (screenwin, gettext ("l               - switch to next level\n"));
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
   char str[255];

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
      snprintf (str, 250, "%s/.daisy-player/%s", pw->pw_dir, bookmark_title);
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
      char src[100], clip_begin[100], clip_end[100];
   } curr, prev, orig;

   if (playing < 0)
   {
      beep ();
      return;
   } // if
   if (player_pid > 0)
      while (kill (player_pid, SIGKILL) != 0);
   get_clips (my_attribute.clip_begin, my_attribute.clip_end);
   if (daisy[playing].begin != clip_begin)
   {
// save current values in orig and curr arrays
      strncpy (orig.clip_begin, my_attribute.clip_begin, 90);
      strncpy (orig.clip_end, my_attribute.clip_end, 90);
      strncpy (orig.src, my_attribute.src, 90);
      strncpy (curr.clip_begin, my_attribute.clip_begin, 90);
      strncpy (curr.clip_end, my_attribute.clip_end, 90);
      strncpy (curr.src, my_attribute.src, 90);
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
            strncpy (prev.clip_begin, curr.clip_begin, 90);
            strncpy (prev.clip_end, curr.clip_end, 90);
            strncpy (prev.src, curr.src, 90);
            strncpy (curr.clip_begin, my_attribute.clip_begin, 90);
            strncpy (curr.clip_end, my_attribute.clip_end, 90);
            strncpy (curr.src, my_attribute.src, 90);
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
         strncpy (curr.clip_begin, my_attribute.clip_begin, 90);
         strncpy (curr.clip_end, my_attribute.clip_end, 90);
         strncpy (curr.src, my_attribute.src, 90);
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
            strncpy (current_audio_file, my_attribute.src, 90);
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
   if (playing == -1)
   {
      beep ();
      return;
   } // if
   kill (player_pid, SIGKILL);
   player_pid = -2;
} // skip_right

void change_level (char key)
{
   int c;

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
   if (strcasestr (daisy_version, "2.02"))
      read_daisy_2 (0);
   if (strcasestr (daisy_version, "3"))
   {
      read_daisy_3 (0, daisy_mp);
      parse_smil_3 ();
   } // if
   current = c;
   if (daisy[current].level > level)
      previous_item ();
   view_screen ();
   wmove (screenwin, daisy[current].y, daisy[current].x);
} // change_level

void read_rc ()
{
   char str[255];
   struct passwd *pw;;
   xmlTextReaderPtr reader;
   xmlDocPtr doc;

   pw = getpwuid (geteuid ());
   snprintf (str, 250, "%s/.daisy-player.rc", pw->pw_dir);
   doc = xmlRecoverFile (str);
   if (! (reader = xmlReaderWalker (doc)))
      return;
   do
   {
      if (! get_tag_or_label (reader))
         break;
   } while (strcasecmp (tag, "prefs") != 0);
   xmlTextReaderClose (reader);
   xmlFreeDoc (doc);
} // read_rc

void save_rc ()
{
   struct passwd *pw;
   char str[255];
   xmlTextWriterPtr writer;

   pw = getpwuid (geteuid ());
   snprintf (str, 250, "%s/.daisy-player.rc", pw->pw_dir);
   if (! (writer = xmlNewTextWriterFilename (str, 0)))
      return;
   xmlTextWriterStartDocument (writer, NULL, NULL, NULL);
   xmlTextWriterStartElement (writer, BAD_CAST "prefs");
   xmlTextWriterWriteFormatAttribute
      (writer, BAD_CAST "sound_dev", "%s", sound_dev);
   xmlTextWriterWriteFormatAttribute (writer, BAD_CAST "speed", "%lf", speed);
   xmlTextWriterEndElement (writer);
   xmlTextWriterEndDocument (writer);
   xmlFreeTextWriter (writer);
} // save_rc

void quit_daisy_player ()
{
   endwin ();
   kill (player_pid, SIGKILL);
   player_pid = -2;
   wait (NULL);
   put_bookmark ();
   save_rc ();
   unlink (tmp_wav);
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
      open_smil_file (daisy[current].smil_file, daisy[current].anchor);
   }
   else
   {
      beep ();
      view_screen ();
      if (! flag)
         return;
      playing = displaying;
      skip_left ();
      skip_right ();
   } // if
} // search

void go_to_page_number ()
{
   char filename[255], *anchor = 0;
   char pn[15], href[100], prev_href[100];

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
      realname (NCC_HTML);
      xmlTextReaderClose (reader);
      xmlDocPtr doc = xmlRecoverFile (NCC_HTML);
      if (! (reader = xmlReaderWalker (doc)))
      {
         endwin ();
         printf (gettext ("\nCannot read %s\n"), NCC_HTML);
         playfile (PREFIX"share/daisy-player/error.wav", "1");
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
            strncpy (prev_href, href, 90);
            strncpy (href, my_attribute.href, 90);
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
         strncpy (filename, prev_href, 250);
         if (strchr (filename, '#'))
         {
            anchor = strdup (strchr (filename, '#') + 1);
            *strchr (filename, '#') = 0;
         } // if
      } // if (*prev_href)
      realname (filename);
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
      return;
   } // if (strcasestr (daisy_version, "2.02"))

   if (strcasestr (daisy_version, "3"))
   {
      char *anchor = 0;

      xmlTextReaderClose (reader);
      realname (NCC_HTML);
      xmlDocPtr doc = xmlRecoverFile (NCC_HTML);
      if (! (reader = xmlReaderWalker (doc)))
      {
         endwin ();
         printf (gettext ("\nCannot read %s\n"), NCC_HTML);
         playfile (PREFIX"share/daisy-player/error.wav", "1");
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
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      puts (gettext ("Cannot read /proc/asound/cards"));
      fflush (stdout);
      _exit (1);
   } // if
   for (n = 0; n < 10; n++)
   {
      list[n] = (char *) malloc (1000);
      bytes = getline (&list[n], &bytes, r);
      if (bytes == -1)
         break;
      trash = (char *) malloc (1000);
      bytes = getline (&trash, &bytes, r);
      free (trash);
      wprintw (screenwin, "   %s", list[n]);
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

void browse ()
{
   int old_screen;
   char str[255];

   current = 0;
   just_this_item = playing = -1;
   get_bookmark ();
   view_screen ();
   nodelay (screenwin, TRUE);
   wmove (screenwin, daisy[current].y, daisy[current].x);

   for (;;)
   {
      signal (SIGCHLD, player_ended);
      switch (wgetch (screenwin))
      {
      case 13:
         just_this_item = -1;
         view_screen ();
         playing = displaying = current;
         current_page_number = daisy[playing].page_number;
         kill (player_pid, SIGKILL);
         player_pid = -2;
         if (discinfo)
         {
            snprintf (str, 250, "%s %s -d %s", prog_name,
                      daisy[current].label, sound_dev);
            switch (system (str))
            {
            default:
               break;
            } // switch
            snprintf (str, 250, "%s . -d %s\n", prog_name, sound_dev);
            switch (system (str))
            {
            default:
               break;
            } // switch
            _exit (0);
         }
         else
            open_smil_file (daisy[current].smil_file, daisy[current].anchor);
         played_time = start_time = 0;
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
            if (system ("udisks --unmount /dev/sr0 > /dev/null") == -1)
               _exit (0);
            if (system ("udisks --eject /dev/sr0 > /dev/null") == -1)
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
         kill (player_pid, SIGKILL);
         player_pid = -2;
         help ();
         if (flag)
            break;
         playing = displaying;
         skip_left ();
         skip_right ();
         break;
      }
      case 'j':
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
         skip_left ();
         skip_right ();
         break;
      case 'p':
         put_bookmark ();
         break;
      case 'q':
         quit_daisy_player ();
         _exit (0);
      case 's':
         kill (player_pid, SIGKILL);
         player_pid = -2;
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
         skip_right ();
         break;
      case KEY_LEFT:
      case '4':
         skip_left ();
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
         skip_left ();
         kill (player_pid, SIGKILL);
         break;
      case KEY_HOME:
      case '*':
         speed = 1;
         kill (player_pid, SIGKILL);
         skip_left ();
         skip_right ();
         break;
      default:
         beep ();
         break;
      } // switch

      if (playing != -1)
      {
         play_clip ();
         view_time ();
      } // if

      fd_set rfds;
      struct timeval tv;

      FD_ZERO (&rfds);
      FD_SET (0, &rfds);
      tv.tv_sec = 0;
      tv.tv_usec = 100000;
// pause till a key has been pressed or 0.1 seconds are elapsed
      select (1, &rfds, NULL, NULL, &tv);
   } // for
} // browse

void usage (char *argv)
{
   printf (gettext ("Daisy-player - Version %s\n"), VERSION);
   puts ("(C)2003-2012 J. Lemmens");
   playfile (PREFIX"share/daisy-player/error.wav", "1");
   printf (gettext ("\nUsage: %s [directory with a Daisy-structure] [-d ALSA sound device]\n"), basename (argv));
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
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      puts (gettext ("\nCannot read /proc/mounts."));
      fflush (stdout);
      _exit (1);
   } // if
   do
   {
      str = malloc (len + 1 );
      if (getline (&str, &len, proc) == -1)
         break;
   } while (! strcasestr (str, "iso9660"));
   fclose (proc);      
   if (strcasestr (str, "iso9660"))
   {
      strncpy (daisy_mp, strchr (str, ' ') + 1, 90);
      *strchr (daisy_mp, ' ') = 0;
      return daisy_mp;
   } // if
   return NULL;
} // get_mount_point

void handle_discinfo (char *discinfo_html)
{
   int h, m, s;
   float t = 0;
   xmlTextReaderPtr di, ncc;

   realname (discinfo_html);
   xmlDocPtr doc = xmlRecoverFile (discinfo_html);
   if (! (di = xmlReaderWalker (doc)))
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
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
         strncpy (daisy_title, label, 90);
      } // if (strcasecmp (tag, "title") == 0)
      if (strcasecmp (tag, "a") == 0)
      {
         strncpy (daisy[current].daisy_mp, my_attribute.href, 90);
         realname (daisy[current].daisy_mp);
         xmlDocPtr doc = xmlRecoverFile (daisy[current].daisy_mp);
         if (! (ncc = xmlReaderWalker (doc)))
         {
            endwin ();
            playfile (PREFIX"share/daisy-player/error.wav", "1");
            printf (gettext ("\nCannot read %s\n"), daisy[current].daisy_mp);
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
         strncpy (daisy[current].label, label, 250);
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
   h = t / 3600;
   t -= h * 3600;
   m = t / 60;
   t -= m * 60;
   s = t;
   snprintf (ncc_totalTime, 90, "%02d:%02d:%02d", h, m, s);
   depth = 1;
   view_screen ();
} // handle_discinfo

int main (int argc, char *argv[])
{
   int opt;
   char str[255], DISCINFO_HTML[100];

   LIBXML_TEST_VERSION // libxml2
   fclose (stderr); // discard SoX messages
   strncpy (prog_name, basename (argv[0]), 90);
   daisy_player_pid = getpid ();
   speed = 1;
   atexit (quit_daisy_player);
   read_rc ();
   setlocale (LC_ALL, getenv ("LANG"));
   setlocale (LC_NUMERIC, "C");
   textdomain (prog_name);
   bindtextdomain (prog_name, PREFIX"share/locale");
   textdomain (prog_name);
   opterr = 0;
   while ((opt = getopt (argc, argv, "md:")) != -1)
   {
      switch (opt)
      {
      case 'd':
         strncpy (sound_dev, optarg, 15);
         break;
      default:
         usage (prog_name);
      } // switch
   } // while
   puts ("(C)2003-2012 J. Lemmens");
   printf (gettext ("Daisy-player - Version %s\n"), VERSION);
   puts (gettext ("A parser to play Daisy CD's with Linux"));
   if (system ("madplay -h > /dev/null") > 0)
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf (gettext ("\nDaisy-player needs the \"madplay\" programme.\n"));
      printf (gettext ("Please install it and try again.\n"));
      fflush (stdout);
      _exit (1);
   } // if
   if (! argv[optind])
// when no mount-point is given try to mount the cd
   {
      if (! get_mount_point ())
      // if not found a mounted cd, try to mount one
      {
         if (system ("udisks --mount /dev/sr0 > /dev/null") == -1)
         {
            endwin ();
            playfile (PREFIX"share/daisy-player/error.wav", "1");
            puts (gettext ("\nCannot use udisks command."));
            fflush (stdout);
            _exit (1);
         } // if

         // try again to mount one
         if (! get_mount_point ())
         {
            endwin ();
            playfile (PREFIX"share/daisy-player/error.wav", "1");
            puts (gettext ("\nNo daisy-cd in drive."));
            fflush (stdout);
            _exit (1);
         } // if
      } // if
   }
   else
   {
      struct stat st_buf;

      if (*argv[optind] == '/')
         strncpy (daisy_mp, argv[optind], 250);
      else
         snprintf (daisy_mp, 250, "%s/%s", getenv ("PWD"), argv[optind]);
      if (stat (daisy_mp, &st_buf) == -1)
      {
         endwin ();
         printf ("stat: %s: %s\n", strerror (errno), daisy_mp);
         playfile (PREFIX"share/daisy-player/error.wav", "1");
         fflush (stdout);
         _exit (1);
      } // if
   } // if
   initscr ();
   if (! (titlewin = newwin (2, 80,  0, 0)) ||
       ! (screenwin = newwin (23, 80, 2, 0)))
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf ("No curses\n");
      fflush (stdout);
      _exit (1);
   } // if
   getmaxyx (screenwin, max_y, max_x);
   max_y--;
   keypad (screenwin, TRUE);
   meta (screenwin,       TRUE);
   nonl ();
   noecho ();
   player_pid = -2;
   if (chdir (daisy_mp) == -1)
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf (gettext ("No such directory: %s\n"), daisy_mp);
      fflush (stdout);
      _exit (1);
   } // if
   snprintf (DISCINFO_HTML, 90, "discinfo.html");
   realname (DISCINFO_HTML);
   if (access (DISCINFO_HTML, R_OK) == 0)
      handle_discinfo (DISCINFO_HTML);
   if (! discinfo)
   {
      snprintf (NCC_HTML, 250, "ncc.html");
      realname (NCC_HTML);
      if (access (NCC_HTML, R_OK) == 0)
      {
         xmlDocPtr doc = xmlRecoverFile (NCC_HTML);
         if (! (reader = xmlReaderWalker (doc)))
         {
            endwin ();
            playfile (PREFIX"share/daisy-player/error.wav", "1");
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
                  strncpy (bookmark_title, label, 90);
                  break;
               } // if
            } // if
         } // while
         read_daisy_2 (1);
      }
      else
      {
         read_daisy_3 (1, daisy_mp);
         parse_smil_3 ();
         strncpy (daisy_version, "3", 2);
      } // if
   } // if (! discinfo);
   wattron (titlewin, A_BOLD);
   snprintf (str, 250, gettext
             ("Daisy-player - Version %s - (C)2012 J. Lemmens"), VERSION);
   mvwprintw (titlewin, 0, 0, str);
   wrefresh (titlewin);

   snprintf (tmp_wav, 200, "/tmp/daisy_player.XXXXXX");
   if (! mkstemp (tmp_wav))
   {
      endwin ();
      printf ("mkstemp ()\n");
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      fflush (stdout);
      _exit (1);
   } // if
   unlink (tmp_wav);
   strcat (tmp_wav, ".wav");
   if (strlen (daisy_title) + strlen (str) >= 80)
      mvwprintw (titlewin, 0, 80 - strlen (daisy_title) - 4, "... ");
   mvwprintw (titlewin, 0, 80 - strlen (daisy_title), "%s", daisy_title);
   wrefresh (titlewin);
   mvwaddstr (titlewin, 1, 0, "----------------------------------------");
   waddstr (titlewin, "----------------------------------------");
   mvwprintw (titlewin, 1, 0, gettext ("Press 'h' for help "));
   level = 1;
   *search_str = 0;
   browse ();
   return 0;
} // main
