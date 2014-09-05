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

void playfile (misc_t *misc, char *in_file, char *in_type,
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
      failure (in_file, errno);
   if ((sox_out = sox_open_write (out_file, &sox_in->signal,
           NULL, out_type, NULL, NULL)) == NULL)
   {
      int e;

      e = errno;
      strncpy (misc->sound_dev, "hw:0", MAX_STR - 1);
      save_rc (misc);
      failure (out_file, e);
   } // if
   if (strcmp (in_type, "cdda") == 0)
   {
      sox_in->encoding.encoding = SOX_ENCODING_SIGN2;
      sox_in->encoding.bits_per_sample = 16;
      sox_in->encoding.reverse_bytes = sox_option_no;
   } // if

   chain = sox_create_effects_chain (&sox_in->encoding, &sox_out->encoding);

   e = sox_create_effect (sox_find_effect ("input"));
   args[0] = (char *) sox_in, sox_effect_options (e, 1, args);
   sox_add_effect (chain, e, &sox_in->signal, &sox_in->signal);

/* Don't use the sox trim effect. It works nice, but is far too slow
   char str2[MAX_STR];
   snprintf (str,  MAX_STR - 1, "%f", misc->clip_begin);
   snprintf (str2, MAX_STR - 1, "%f", misc->clip_end - misc->clip_begin);
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

void put_bookmark (misc_t *misc)
{
   char str[MAX_STR];
   xmlTextWriterPtr writer;
   struct passwd *pw;;

   pw = getpwuid (geteuid ());
   snprintf (str, MAX_STR - 1, "%s/.daisy-player", pw->pw_dir);
   mkdir (str, 0755);
   snprintf (str, MAX_STR - 1, "%s/.daisy-player/%s%s",
             pw->pw_dir, misc->bookmark_title, get_mcn (misc));
   if (! (writer = xmlNewTextWriterFilename (str, 0)))
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
      xmlTextWriterWriteFormatAttribute
         (writer, BAD_CAST "clip-begin", "%f", misc->clip_begin);
   else
      xmlTextWriterWriteFormatAttribute
         (writer, BAD_CAST "seconds", "%d", (int) (time (NULL) - misc->seconds));
   xmlTextWriterWriteFormatAttribute (writer, BAD_CAST "level", "%d", misc->level);
   xmlTextWriterEndElement (writer);
   xmlTextWriterEndDocument (writer);
   xmlFreeTextWriter (writer);
} // put_bookmark

void get_bookmark (misc_t *misc, my_attribute_t *my_attribute,
                   daisy_t *daisy)
{
   char str[MAX_STR];
   float begin;
   xmlTextReaderPtr local_reader;
   xmlDocPtr local_doc;
   struct passwd *pw;

   if (misc->ignore_bookmark == 1)
      return;
   pw = getpwuid (geteuid ());
   if (! *misc->bookmark_title)
      return;
   snprintf (str, MAX_STR - 1, "%s/.daisy-player/%s%s",
             pw->pw_dir, misc->bookmark_title, get_mcn (misc));
   local_doc = xmlRecoverFile (str);
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
   if (misc->current <= 0)
      misc->current = 0;
   if (misc->current >= misc->total_items)
      misc->current = 0;
   misc->displaying = misc->playing = misc->current;
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
   {
      misc->player_pid = play_track (misc, misc->sound_dev, "alsa",
            daisy[misc->current].first_lsn + (misc->seconds * 75));
      misc->seconds = time (NULL) - misc->seconds;
      return;
   } // if
   get_clips (misc, my_attribute->clip_begin, my_attribute->clip_end);
   begin = misc->clip_begin;
   open_smil_file (misc, my_attribute, daisy[misc->current].smil_file,
                   daisy[misc->current].anchor);
   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, misc->reader))
         break;
      if (strcasecmp (misc->tag, "audio") == 0)
      {
         strncpy (misc->current_audio_file, my_attribute->src, MAX_STR - 1);
         get_clips (misc, my_attribute->clip_begin, my_attribute->clip_end);
         if (misc->clip_begin == begin)
            break;
      } // if
   } // while
   if (misc->level < 1)
      misc->level = 1;
   misc->current_page_number = daisy[misc->playing].page_number;
   misc->just_this_item = -1;
   skip_left (misc, my_attribute, daisy);
   skip_right (misc);
   view_screen (misc, daisy);
   return;
} // get_bookmark

void get_page_number_2 (misc_t *misc, my_attribute_t *my_attribute,
                        daisy_t *daisy, char *p)
{
// function for daisy 2.02
   char *anchor = 0, *id1, *id2;
   xmlTextReaderPtr page;
   xmlDocPtr doc;

   doc = xmlRecoverFile (daisy[misc->playing].smil_file);
   if (! (page = xmlReaderWalker (doc)))
   {
      int e;
      char str[MAX_STR];

      e = errno;
      snprintf (str, MAX_STR,
            gettext ("\nCannot read %s\n"), daisy[misc->playing].smil_file);
      failure (str, e);
   } // if
   id1 = strdup (p);
   do
   {
      if (! get_tag_or_label (misc, my_attribute, page))
      {
         free (id1);
         return;
      } // if
   } while (strcasecmp (my_attribute->id, id1) != 0);
   do
   {
      if (! get_tag_or_label (misc, my_attribute, page))
         return;
   } while (strcasecmp (misc->tag, "text") != 0);
   id2 = strdup (my_attribute->id);
   xmlTextReaderClose (page);
   xmlFreeDoc (doc);
   doc = xmlRecoverFile (misc->NCC_HTML);
   if (! (page = xmlReaderWalker (doc)))
   {
      int e;
      char str[MAX_STR];

      e = errno;
      snprintf (str, MAX_STR, gettext ("\nCannot read %s\n"), misc->NCC_HTML);
      failure (str, e);
   } // if
   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, page))
         return;
      if (strcasecmp (misc->tag, "a") == 0)
      {
         if (strchr (my_attribute->href, '#'))
            anchor = strdup (strchr (my_attribute->href, '#') + 1);
         if (strcasecmp (anchor, id2) == 0)
            break;
      } // if
   } // while
   do
   {
      if (! get_tag_or_label (misc, my_attribute, page))
         return;
   } while (! *misc->label);
   xmlTextReaderClose (page);
   xmlFreeDoc (doc);
   misc->current_page_number = atoi (misc->label);
} // get_page_number_2

int get_next_clips (misc_t *misc, my_attribute_t *my_attribute,
                    daisy_t *daisy)
{
   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, misc->reader))
         return 0;
      if (strcasecmp (my_attribute->id, daisy[misc->playing + 1].anchor) == 0 &&
          misc->playing + 1 != misc->total_items)
         return 0;
      if (strcasecmp (my_attribute->class, "pagenum") == 0)
         if (strcasestr (misc->daisy_version, "3"))
            get_page_number_3 (misc, my_attribute);
      if (strcasecmp (misc->tag, "audio") == 0)
      {
         strncpy (misc->current_audio_file, my_attribute->src, MAX_STR - 1);
         get_clips (misc, my_attribute->clip_begin, my_attribute->clip_end);
         return 1;
      } // if (strcasecmp (misc->tag, "audio") == 0)
   } // while
} // get_next_clips

int compare_playorder (const void *p1, const void *p2)
{
   return (*(int *)p1 - *(int*)p2);                   
} // compare_playorder

void parse_smil_2 (misc_t *misc, my_attribute_t *my_attribute, daisy_t *daisy)
{
// function for daisy 2.02
   int x;
   xmlTextReaderPtr parse;

   misc->total_time = 0;
   for (x = 0; x < misc->total_items; x++)
   {
      if (*daisy[x].smil_file == 0)
         continue;
      xmlDocPtr doc = xmlRecoverFile (daisy[x].smil_file);
      if (! (parse = xmlReaderWalker (doc)))
      {
         int e;
         char str[MAX_STR];

         e = errno;
         snprintf (str, MAX_STR, 
                     gettext ("\nCannot read %s\n"), daisy[x].smil_file);
         failure (str, e);
      } // if

// parse this smil
      do
      {
         if (! get_tag_or_label (misc, my_attribute, parse))
            break;
      } while (strcasecmp (my_attribute->id, daisy[x].anchor) != 0);
      daisy[x].duration = 0;
      while (1)
      {
         if (! get_tag_or_label (misc, my_attribute, parse))
            break;
         if (strcasecmp (misc->tag, "audio") == 0)
         {
            strncpy (misc->current_audio_file, my_attribute->src, MAX_STR - 1);
            get_clips (misc, my_attribute->clip_begin, my_attribute->clip_end);
            daisy[x].begin = misc->clip_begin;
            daisy[x].duration += misc->clip_end - misc->clip_begin;
            while (1)
            {
               if (! get_tag_or_label (misc, my_attribute, parse))
                  break;
               if (*daisy[x + 1].anchor)
                  if (strcasecmp (my_attribute->id, daisy[x + 1].anchor) == 0)
                     break;
               if (strcasecmp (misc->tag, "audio") == 0)
               {
                  get_clips (misc, my_attribute->clip_begin,
                             my_attribute->clip_end);
                  daisy[x].duration += misc->clip_end - misc->clip_begin;
               } // if (strcasecmp (misc->tag, "audio") == 0)
            } // while
            if (*daisy[x + 1].anchor)
               if (strcasecmp (my_attribute->id, daisy[x + 1].anchor) == 0)
                  break;
         } // if (strcasecmp (misc->tag, "audio") == 0)
      } // while
      xmlTextReaderClose (parse);
      xmlFreeDoc (doc);
      misc->total_time += daisy[x].duration;
   } // for
} // parse_smil_2

void view_page (misc_t *misc, my_attribute_t *my_attribute, daisy_t *daisy)
{
   if (misc->playing == -1)
      return;
   if (daisy[misc->playing].screen != daisy[misc->current].screen)
      return;
   if (misc->total_pages == 0)
      return;
   wattron (misc->screenwin, A_BOLD);
   if (strcasestr (misc->daisy_version, "2.02"))
      get_page_number_2 (misc, my_attribute, daisy, my_attribute->id);
   if (misc->current_page_number)
      mvwprintw (misc->screenwin, daisy[misc->playing].y, 62, "(%3d)",
                 misc->current_page_number);
   else
      mvwprintw (misc->screenwin, daisy[misc->playing].y, 62, "     ");
   wattroff (misc->screenwin, A_BOLD);
   wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
   wrefresh (misc->screenwin);
} // view_page

void view_time (misc_t *misc, daisy_t *daisy)
{
   float remain_time = 0, curr_time = 0;

   if (misc->playing == -1 ||
       daisy[misc->current].screen != daisy[misc->playing].screen)
      return;
   wattron (misc->screenwin, A_BOLD);
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      curr_time = (misc->lsn_cursor - daisy[misc->playing].first_lsn) / 75;
   else
      curr_time = misc->start_time + (float) (time (NULL) - misc->seconds);
   curr_time /= misc->speed;
   mvwprintw (misc->screenwin, daisy[misc->playing].y, 68, "%02d:%02d",
              (int) curr_time / 60, (int) curr_time % 60);
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      remain_time = (daisy[misc->playing].last_lsn -
                     daisy[misc->playing].first_lsn) / 75 - curr_time;
   else
      remain_time = daisy[misc->playing].duration - curr_time;
   remain_time /= misc->speed;
   mvwprintw (misc->screenwin, daisy[misc->playing].y, 74, "%02d:%02d",
              (int) remain_time / 60, (int) remain_time % 60);
   wattroff (misc->screenwin, A_BOLD);
   wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
   wrefresh (misc->screenwin);
} // view_time

void view_screen (misc_t *misc, daisy_t *daisy)
{
   int i, x, x2;
   float time;

   mvwaddstr (misc->titlewin, 1, 0,
              "----------------------------------------");
   waddstr (misc->titlewin, "----------------------------------------");
   mvwprintw (misc->titlewin, 1, 0, gettext ("'h' for help -"));
   if (misc->total_pages)
      wprintw (misc->titlewin, gettext (" %d pages "), misc->total_pages);
   mvwprintw (misc->titlewin, 1, 28, gettext (" level: %d of %d "),
              misc->level, misc->depth);
   time = misc->total_time / misc->speed;
   int hours   = time / 3600;
   int minutes = (time - hours * 3600) / 60;
   int seconds = time - (hours * 3600 + minutes * 60);
   mvwprintw (misc->titlewin, 1, 47, gettext (" total length: %02d:%02d:%02d "),
              hours, minutes,seconds);
   mvwprintw (misc->titlewin, 1, 74, " %d/%d ",
              daisy[misc->current].screen + 1,
              daisy[misc->total_items - 1].screen + 1);
   wrefresh (misc->titlewin);
   wclear (misc->screenwin);
   for (i = 0; daisy[i].screen != daisy[misc->current].screen; i++);
   do
   {
      mvwprintw (misc->screenwin, daisy[i].y, daisy[i].x + 1, daisy[i].label);
      x = strlen (daisy[i].label) + daisy[i].x;
      if (x / 2 * 2 != x)
         waddstr (misc->screenwin, " ");
      for (x2 = x; x2 < 59; x2 = x2 + 2)
         waddstr (misc->screenwin, " .");
      if (daisy[i].page_number)
         mvwprintw (misc->screenwin, daisy[i].y, 61, " (%3d)", daisy[i].page_number);
      if (daisy[i].level <= misc->level)
      {
         int x = i, dur = 0;

         do
         {
            dur += daisy[x].duration;
         } while (daisy[++x].level > misc->level);
         dur /= misc->speed;
         mvwprintw (misc->screenwin, daisy[i].y, 74, "%02d:%02d",
                    (int) (dur + .5) / 60, (int) (dur + .5) % 60);
      } // if
      if (i >= misc->total_items - 1)
         break;
   } while (daisy[++i].screen == daisy[misc->current].screen);
   if (misc->just_this_item != -1 &&
       daisy[misc->current].screen == daisy[misc->playing].screen)
      mvwprintw (misc->screenwin, daisy[misc->displaying].y, 0, "J");
   wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
   wrefresh (misc->screenwin);
} // view_screen

void read_daisy_2 (misc_t *misc, my_attribute_t *my_attribute,
                   daisy_t *daisy)
{
// function for daisy 2.02
   int indent = 0, found_page_normal = 0;
   xmlTextReaderPtr ncc;
   xmlDocPtr doc;

   if ((doc = xmlRecoverFile (misc->NCC_HTML)) == NULL)
      failure ("read_daisy_2 ()", errno);
   if (! (ncc = xmlReaderWalker (doc)))
   {
      char str[MAX_STR];
      int e;

      e = errno;
      snprintf (str, MAX_STR, gettext ("\nCannot read %s\n"), misc->NCC_HTML);
      failure (str, e);
   } // if
   misc->current = misc->displaying = 0;
   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, ncc))
         break;
      if (strcasecmp (misc->tag, "h1") == 0 ||
          strcasecmp (misc->tag, "h2") == 0 ||
          strcasecmp (misc->tag, "h3") == 0 ||
          strcasecmp (misc->tag, "h4") == 0 ||
          strcasecmp (misc->tag, "h5") == 0 ||
          strcasecmp (misc->tag, "h6") == 0)
      {
         int l;

         found_page_normal = 0;
         daisy[misc->current].page_number = 0;
         l = misc->tag[1] - '0';
         daisy[misc->current].level = l;
         daisy[misc->current].x = l + 3 - 1;
         indent = daisy[misc->current].x = (l - 1) * 3 + 1;
         do
         {
            if (! get_tag_or_label (misc, my_attribute, ncc))
               break;
         } while (strcasecmp (misc->tag, "a") != 0);
         strncpy (daisy[misc->current].smil_file, my_attribute->href,
                  MAX_STR - 1);

         if (strchr (daisy[misc->current].smil_file, '#'))
         {
            strncpy (daisy[misc->current].anchor,
                     strchr (daisy[misc->current].smil_file, '#') + 1, MAX_STR - 1);
            *strchr (daisy[misc->current].smil_file, '#') = 0;
         } // if
         do
         {
            if (! get_tag_or_label (misc, my_attribute, ncc))
               break;
         } while (*misc->label == 0);
         get_label (misc, daisy, indent);
         misc->current++;
         misc->displaying++;
      } // if (strcasecmp (misc->tag, "h1") == 0 || ...
      if (strcasecmp (my_attribute->class, "page-normal") == 0 &&
          found_page_normal == 0)
      {
         found_page_normal = 1;
         do
         {
            if (! get_tag_or_label (misc, my_attribute, ncc))
               break;
         } while (*misc->label == 0);
         daisy[misc->current - 1].page_number = atoi (misc->label);
      } // if (strcasecmp (my_attribute->class, "page-normal")
   } // while
   xmlTextReaderClose (ncc);
   xmlFreeDoc (doc);
   misc->total_items = misc->current;
   parse_smil_2 (misc, my_attribute, daisy);
} // read_daisy_2

void player_ended ()
{
   wait (NULL);
} // player_ended

void play_now (misc_t *misc, my_attribute_t *my_attribute,
               daisy_t *daisy)
{
   if (misc->playing == -1)
      return;

   misc->seconds = time (NULL);
   misc->start_time = misc->clip_begin;
   switch (misc->player_pid = fork ())
   {
   case -1:
      failure ("fork ()", errno);
   case 0: // child
      break;
   default: // parent
      return;
   } // switch

   char tempo_str[15], cmd[MAX_CMD];
   setsid ();

   view_page (misc, my_attribute, daisy);
   lseek (misc->tmp_wav_fd, SEEK_SET, 0);
   snprintf (cmd, MAX_CMD - 1, "madplay -Q %s -s %f -t %f -o wav:\"%s\"",
                  misc->current_audio_file, misc->clip_begin,
                  misc->clip_end - misc->clip_begin, misc->tmp_wav);
   if (system (cmd) != 0)
      _exit (0);
   snprintf (tempo_str, 10, "%lf", misc->speed);
   playfile (misc, misc->tmp_wav, "wav", misc->sound_dev, "alsa", tempo_str);
   _exit (0);
} // play_now

void open_smil_file (misc_t *misc, my_attribute_t *my_attribute,
                     char *smil_file, char *anchor)
{
   xmlDocPtr local_doc;

   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      return;
   if (*smil_file == 0)
      return;
   local_doc = xmlRecoverFile (smil_file);
   if (! (misc->reader = xmlReaderWalker (local_doc)))
   {
      int e;
      char str[MAX_STR];

      e = errno;
      snprintf (str, MAX_STR, "open_smil_file(): %s\n", smil_file);
      failure (str, e);
   } // if

// skip to anchor
   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, misc->reader))
         break;;
      if (strcasecmp (my_attribute->id, anchor) == 0)
         break;
   } // while
   return;
} // open_smil_file

void pause_resume (misc_t *misc, my_attribute_t *my_attribute, daisy_t *daisy)
{
   if (misc->playing > -1)
   {
      misc->playing = -1;
      if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
         kill (misc->player_pid, SIGKILL);
      else
      {
         kill (misc->player_pid, SIGKILL);
         misc->player_pid = -2;
      } // if
   }
   else
   {
      misc->playing = misc->displaying;
      if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      {
         lsn_t start;

         start = (lsn_t) (misc->start_time + (float) (time (NULL) - misc->seconds) * 75) +
                 daisy[misc->playing].first_lsn;
         misc->player_pid = play_track (misc, misc->sound_dev, "alsa",
                            start);
      }
      else
      {
         skip_left (misc, my_attribute, daisy);
         skip_right (misc);
      } // if
   } // if
} // pause_resume

void write_wav (misc_t *misc, daisy_t *daisy, char *outfile)
{
   char str[MAX_STR];
   struct passwd *pw;

   pw = getpwuid (geteuid ());
   snprintf (str, MAX_STR - 1, "%s/%s", pw->pw_dir, outfile);
   while (access (str, F_OK) == 0)
      strncat (str, ".wav", MAX_STR - 1);

   if (misc->cd_type == CDIO_DISC_MODE_CD_DA || \
       misc->cd_type == CDIO_DISC_MODE_CD_DATA)
      set_drive_speed (misc, 100); // fastest
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
   {
      pid_t pid;                              
      int sp;
      int16_t *p_readbuf;

      sp = misc->speed;
      misc->speed = 1;
      pid = play_track (misc, str, "wav",
                        daisy[misc->current].first_lsn);
      misc->speed = sp;
      do
      {
         if (! (p_readbuf = paranoia_read (misc->par, NULL)))
            break;
         switch (write (misc->pipefd[1], p_readbuf, CDIO_CD_FRAMESIZE_RAW))
         {
         default:
            break;
         } // switch
      } while (++misc->lsn_cursor <= daisy[misc->current].last_lsn);
      kill (pid, SIGQUIT);
      if (misc->cd_type == CDIO_DISC_MODE_CD_DA || \
          misc->cd_type == CDIO_DISC_MODE_CD_DATA)
         set_drive_speed (misc, 4);
      return;
   } // if

   char cmd[MAX_CMD];

   lseek (misc->tmp_wav_fd, SEEK_SET, 0);
   snprintf (cmd, MAX_CMD, "madplay -Q \"%s\" -s %f -t %f -o wav:\"%s\"",
             misc->current_audio_file, 
             daisy[misc->current].begin, daisy[misc->current].duration, str);
   if (system (cmd) != 0)
      failure (cmd, errno);
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA || \
       misc->cd_type == CDIO_DISC_MODE_CD_DATA)
      set_drive_speed (misc, 4);
} // write_wav

void store_to_disk (misc_t *misc, my_attribute_t *my_attribute, daisy_t *daisy)
{
   char str[MAX_STR];
   int i, pause_flag;

   pause_flag = misc->playing;
   if (pause_flag > -1)
      pause_resume (misc, my_attribute, daisy);
   wclear (misc->screenwin);
   snprintf (str, MAX_STR - 1, "%s.wav", daisy[misc->current].label);
   wprintw (misc->screenwin,
            "\nStoring \"%s\" as \"%s\" into your home-folder...",
            daisy[misc->current].label, str);
   wrefresh (misc->screenwin);
   for (i = 0; str[i] != 0; i++)
      if (str[i] == '/')
         str[i] = '-';
   write_wav (misc, daisy, str);
   if (pause_flag > -1)
      pause_resume (misc, my_attribute, daisy);
   view_screen (misc, daisy);
} // store_to_disk

void help (misc_t *misc, daisy_t *daisy)
{
   int y, x;

   getyx (misc->screenwin, y, x);
   wclear (misc->screenwin);
   waddstr (misc->screenwin, gettext ("\nThese commands are available in this version:\n"));
   waddstr (misc->screenwin, "========================================");
   waddstr (misc->screenwin, "========================================\n\n");
   waddstr (misc->screenwin,
            gettext ("cursor down,2   - move cursor to the next item\n"));
   waddstr (misc->screenwin,
            gettext ("cursor up,8     - move cursor to the previous item\n"));
   waddstr (misc->screenwin,
            gettext ("cursor right,6  - skip to next phrase\n"));
   waddstr (misc->screenwin,
            gettext ("cursor left,4   - skip to previous phrase\n"));
   waddstr (misc->screenwin,
            gettext ("page-down,3     - view next page\n"));
   waddstr (misc->screenwin,
            gettext ("page-up,9       - view previous page\n"));
   waddstr (misc->screenwin,
            gettext ("enter           - start playing\n"));
   waddstr (misc->screenwin,
            gettext ("space,0         - pause/resume playing\n"));
   waddstr (misc->screenwin,
            gettext ("home,*          - play on normal speed\n"));
   waddstr (misc->screenwin, "\n");
   waddstr (misc->screenwin, gettext ("Press any key for next page..."));
   nodelay (misc->screenwin, FALSE);
   wgetch (misc->screenwin);
   nodelay (misc->screenwin, TRUE);
   wclear (misc->screenwin);
   waddstr (misc->screenwin,
            gettext ("\n/               - search for a label\n"));
   waddstr (misc->screenwin,
            gettext ("d               - store current item to disk\n"));
   waddstr (misc->screenwin,
            gettext ("D,-             - decrease playing speed\n"));
   waddstr (misc->screenwin, gettext (
            "e,.             - quit daisy-player, place a bookmark and eject\n"));
   waddstr (misc->screenwin, gettext (
"f               - find the currently playing item and place the cursor there\n"));
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      waddstr (misc->screenwin,
            gettext ("g               - go to time in this song (MM:SS)\n"));
   else
      waddstr (misc->screenwin,
            gettext ("g               - go to time in this item (MM:SS)\n"));
   waddstr (misc->screenwin,
            gettext ("G               - go to page number (if any)\n"));
   waddstr (misc->screenwin,
            gettext ("h,?             - give this help\n"));
   waddstr (misc->screenwin, 
            gettext ("j,5             - just play current item\n"));
   waddstr (misc->screenwin,
          gettext ("l               - switch to next level\n"));
   waddstr (misc->screenwin, gettext ("L               - switch to previous level\n"));
   waddstr (misc->screenwin, gettext ("n               - search forwards\n"));
   waddstr (misc->screenwin, gettext ("N               - search backwards\n"));
   waddstr (misc->screenwin, gettext ("o               - select next output sound device\n"));
   waddstr (misc->screenwin, gettext ("p               - place a bookmark\n"));
   waddstr (misc->screenwin,
   gettext ("q               - quit daisy-player and place a bookmark\n"));
   waddstr (misc->screenwin, gettext ("s               - stop playing\n"));
   waddstr (misc->screenwin,
            gettext ("U,+             - increase playing speed\n"));
   waddstr (misc->screenwin, gettext ("\nPress any key to leave help..."));
   nodelay (misc->screenwin, FALSE);
   wgetch (misc->screenwin);
   nodelay (misc->screenwin, TRUE);
   view_screen (misc, daisy);
   wmove (misc->screenwin, y, x);
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

void play_clip (misc_t *misc, my_attribute_t *my_attribute,
                daisy_t *daisy)
{
   char str[MAX_STR];

   if (kill (misc->player_pid, 0) == 0)
// if still playing
      return;
   misc->player_pid = -2;
   if (get_next_clips (misc, my_attribute, daisy))
   {
      play_now (misc, my_attribute, daisy);
      return;
   } // if

// go to next item
   strncpy (my_attribute->clip_begin, "0", 2);
   misc->clip_begin = 0;
   if (++misc->playing >= misc->total_items)
   {
      struct passwd *pw;

      pw = getpwuid (geteuid ());
      quit_daisy_player (misc);
      snprintf (str, MAX_STR - 1, "%s/.daisy-player/%s%s", pw->pw_dir,
                misc->bookmark_title, get_mcn (misc));
      unlink (str);
      _exit (0);
   } // if
   if (daisy[misc->playing].level <= misc->level)
      misc->displaying = misc->current = misc->playing;
   if (misc->just_this_item != -1)
      if (daisy[misc->playing].level <= misc->level)
         misc->playing = misc->just_this_item = -1;
   if (misc->playing > -1)
      open_smil_file (misc, my_attribute, daisy[misc->playing].smil_file,
                      daisy[misc->playing].anchor);
   view_screen (misc, daisy);
} // play_clip

void skip_left (misc_t *misc, my_attribute_t *my_attribute,
                daisy_t *daisy)
{
   struct
   {
      char src[MAX_STR], clip_begin[MAX_STR], clip_end[MAX_STR];
   } curr, prev, orig;

   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      return;
   if (misc->playing < 0)
   {
      beep ();
      return;
   } // if
   if (misc->player_pid > 0)
      while (kill (misc->player_pid, SIGKILL) != 0);
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
   {
      beep ();
      return;
   } // if
   get_clips (misc, my_attribute->clip_begin, my_attribute->clip_end);
   if (daisy[misc->playing].begin != misc->clip_begin)
   {
// save current values in orig and curr arrays
      strncpy (orig.clip_begin, my_attribute->clip_begin, MAX_STR - 1);
      strncpy (orig.clip_end, my_attribute->clip_end, MAX_STR - 1);
      strncpy (orig.src, my_attribute->src, MAX_STR - 1);
      strncpy (curr.clip_begin, my_attribute->clip_begin, MAX_STR - 1);
      strncpy (curr.clip_end, my_attribute->clip_end, MAX_STR - 1);
      strncpy (curr.src, my_attribute->src, MAX_STR - 1);
      open_smil_file (misc, my_attribute, daisy[misc->playing].smil_file,
                      daisy[misc->playing].anchor);
      while (1)
      {
         if (! get_tag_or_label (misc, my_attribute, misc->reader))
            return;
         if (strcasecmp (my_attribute->class, "pagenum") == 0)
         {
            do
            {
               if (! get_tag_or_label (misc, my_attribute, misc->reader))
                  return;
            } while (strcasecmp (misc->tag, "/par") != 0);
         } // if (strcasecmp (my_attribute->class, "pagenum") == 0)
         if (strcasecmp (misc->tag, "audio") == 0)
         {
            strncpy (prev.clip_begin, curr.clip_begin, MAX_STR - 1);
            strncpy (prev.clip_end, curr.clip_end, MAX_STR - 1);
            strncpy (prev.src, curr.src, MAX_STR - 1);
            strncpy (curr.clip_begin, my_attribute->clip_begin, MAX_STR - 1);
            strncpy (curr.clip_end, my_attribute->clip_end, MAX_STR - 1);
            strncpy (curr.src, my_attribute->src, MAX_STR - 1);
            if (strcasecmp (orig.clip_begin, curr.clip_begin) == 0 &&
                strcasecmp (orig.src, curr.src) == 0)
            {
               open_smil_file (misc, my_attribute, daisy[misc->playing].smil_file,
                               daisy[misc->playing].anchor);
               while (1)
               {
                  if (! get_tag_or_label (misc, my_attribute, misc->reader))
                     return;
                  if (strcasecmp (misc->tag , "audio") == 0)
                  {
                     if (strcasecmp (my_attribute->clip_begin,
                         prev.clip_begin) == 0 &&
                         strcasecmp (my_attribute->src, prev.src) == 0)
                     {
                        get_clips (misc, my_attribute->clip_begin,
                                   my_attribute->clip_end);
                        play_now (misc, my_attribute, daisy);
                        return;
                     } // if (strcasecmp (my_attribute->clip_begin, prev.cli ...
                  } // if (strcasecmp (misc->tag , "audio") == 0)
               } // while
            } // if (strcasecmp (orig.clip_begin, curr.clip_begin) == 0 &&
         } // if (strcasecmp (misc->tag, "audio") == 0)
      } // while
   } // if (clip_begin != daisy[misc->playing].begin)

// go to end of previous item
   misc->current = misc->displaying = --misc->playing;
   if (misc->current < 0)
   {
      beep ();
      misc->current = misc->displaying = misc->playing = 0;
      return;
   } // if
   open_smil_file (misc, my_attribute, daisy[misc->current].smil_file, daisy[misc->current].anchor);
   while (1)
   {
// search last clip
      if (! get_tag_or_label (misc, my_attribute, misc->reader))
         break;
      if (strcasecmp (my_attribute->class, "pagenum") == 0)
      {
         do
         {
            if (! get_tag_or_label (misc, my_attribute, misc->reader))
               return;
         } while (strcasecmp (misc->tag, "/par") != 0);
      } // if (strcasecmp (my_attribute->class, "pagenum") == 0)
      if (strcasecmp (misc->tag, "audio") == 0)
      {
         strncpy (curr.clip_begin, my_attribute->clip_begin, MAX_STR - 1);
         strncpy (curr.clip_end, my_attribute->clip_end, MAX_STR - 1);
         strncpy (curr.src, my_attribute->src, MAX_STR - 1);
      } // if (strcasecmp (misc->tag, "audio") == 0)
      if (strcasecmp (my_attribute->id, daisy[misc->current + 1].anchor) == 0)
         break;
   } // while
   misc->just_this_item = -1;
   open_smil_file (misc, my_attribute, daisy[misc->current].smil_file,
                   daisy[misc->current].anchor);
   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, misc->reader))
         return;
      if (strcasecmp (misc->tag , "audio") == 0)
      {
         if (strcasecmp (my_attribute->clip_begin, curr.clip_begin) == 0 &&
             strcasecmp (my_attribute->src, curr.src) == 0)
         {
            strncpy (misc->current_audio_file, my_attribute->src, MAX_STR - 1);
            get_clips (misc, my_attribute->clip_begin, my_attribute->clip_end);
            view_screen (misc, daisy);
            play_now (misc, my_attribute, daisy);
            return;
         } // if (strcasecmp (my_attribute->clip_begin, curr.clip_begin) == 0 &&
      } // if (strcasecmp (misc->tag , "audio") == 0)
   } // while
} // skip_left

void skip_right (misc_t *misc)
{
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      return;
   if (misc->playing == -1)
   {
      beep ();
      return;
   } // if
   if (misc->cd_type != CDIO_DISC_MODE_CD_DA)
   {
      kill (misc->player_pid, SIGKILL);
      misc->player_pid = -2;
   } // if
} // skip_right

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
   mvwprintw (misc->titlewin, 1, 0, gettext ("Please wait... -------------------------"));
   wprintw (misc->titlewin, "----------------------------------------");
   wrefresh (misc->titlewin);
   c = misc->current;
   l = misc->level;
   if (strcasestr (misc->daisy_version, "2.02"))
      read_daisy_2 (misc, my_attribute, daisy);
   if (strcasestr (misc->daisy_version, "3"))
   {
      read_daisy_3 (misc, my_attribute, daisy);
      parse_smil_3 (misc, my_attribute, daisy);
   } // if
   misc->current = c;
   misc->level = l;
   if (daisy[misc->current].level > misc->level)
      previous_item (misc, daisy);
   view_screen (misc, daisy);
   wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
} // change_level

void read_rc (misc_t *misc, my_attribute_t *my_attribute)
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
      strncpy (misc->sound_dev, "hw:0", MAX_STR - 1);
      return;
   } // if
   do
   {
      if (! get_tag_or_label (misc, my_attribute, reader))
         break;
   } while (strcasecmp (misc->tag, "prefs") != 0);
   xmlTextReaderClose (reader);
   xmlFreeDoc (doc);
   if (misc->cddb_flag != 'n' && misc->cddb_flag != 'y')
      misc->cddb_flag = 'y';
} // read_rc

void save_rc (misc_t *misc)
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
} // save_rc

void quit_daisy_player (misc_t *misc)
{
   endwin ();
   kill (misc->player_pid, SIGKILL);
   misc->player_pid = -2;
   put_bookmark (misc);
   save_rc (misc);

   if (misc->cd_type == CDIO_DISC_MODE_CD_DA || \
       misc->cd_type == CDIO_DISC_MODE_CD_DATA)
      set_drive_speed (misc, 0);
   close (misc->tmp_wav_fd);
   puts ("");
} // quit_daisy_player

void search (misc_t *misc, my_attribute_t *my_attribute, daisy_t *daisy,
             int start, char mode)
{
   int c, found = 0, flag = 0;

   if (misc->playing != -1)
   {
      flag = 1;
      pause_resume (misc, my_attribute, daisy);
   } // if
   if (mode == '/')
   {
      mvwaddstr (misc->titlewin, 1, 0, "----------------------------------------");
      waddstr (misc->titlewin, "----------------------------------------");
      mvwprintw (misc->titlewin, 1, 0, gettext ("What do you search? "));
      echo ();
      wgetnstr (misc->titlewin, misc->search_str, 25);
      noecho ();
   } // if
   if (mode == '/' || mode == 'n')
   {
      for (c = start; c <= misc->total_items + 1; c++)
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
         for (c = misc->total_items + 1; c > start; c--)
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
      misc->playing = misc->displaying= misc->current = c;
      misc->clip_begin = daisy[misc->current].begin;
      misc->just_this_item = -1;
      view_screen (misc, daisy);
      if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      {
         misc->seconds = time (NULL);
         misc->player_pid = play_track (misc, misc->sound_dev, "alsa",
                daisy[misc->current].first_lsn);
      }
      else
         open_smil_file (misc, my_attribute, daisy[misc->current].smil_file,
                         daisy[misc->current].anchor);
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
         skip_left (misc, my_attribute, daisy);
         skip_right (misc);
      } // if
   } // if
} // search

void go_to_page_number (misc_t *misc, my_attribute_t *my_attribute,
                        daisy_t *daisy)
{
   char filename[MAX_STR], *anchor = 0;
   char pn[15], href[MAX_STR], prev_href[MAX_STR];

   kill (misc->player_pid, SIGKILL);
   misc->player_pid = -2;
   mvwaddstr (misc->titlewin, 1, 0, "----------------------------------------");
   waddstr (misc->titlewin, "----------------------------------------");
   mvwprintw (misc->titlewin, 1, 0, gettext ("Go to page number: "));
   echo ();
   wgetnstr (misc->titlewin, pn, 5);
   noecho ();
   view_screen (misc, daisy);
   if (! *pn || atoi (pn) > misc->total_pages || ! atoi (pn))
   {
      beep ();
      skip_left (misc, my_attribute, daisy);
      skip_right (misc);
      return;
   } // if
   if (strcasestr (misc->daisy_version, "2.02"))
   {
      xmlTextReaderClose (misc->reader);
      xmlDocPtr doc = xmlRecoverFile (misc->NCC_HTML);
      if (! (misc->reader = xmlReaderWalker (doc)))
      {
         int e;
         char str[MAX_STR];
         
         e = errno;
         snprintf (str, MAX_STR,
                gettext ("\nCannot read %s\n"), misc->NCC_HTML);
         failure (str, e);
      } // if
      *href = 0;
      while (1)
      {
         if (! get_tag_or_label (misc, my_attribute, misc->reader))
            return;
         if (strcasecmp (misc->tag, "a") == 0)
         {
            strncpy (prev_href, href, MAX_STR - 1);
            strncpy (href, my_attribute->href, MAX_STR - 1);
            do
            {
               if (! get_tag_or_label (misc, my_attribute, misc->reader))
                  return;
            } while (! *misc->label);
            if (strcasecmp (misc->label, pn) == 0)
               break;
         } // if (strcasecmp (misc->tag, "a") == 0)
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
      for (misc->current = misc->total_items - 1; misc->current >= 0; misc->current--)
      {
         if (! daisy[misc->current].page_number)
            continue;
         if (atoi (pn) >= daisy[misc->current].page_number)
            break;
      } // for
      misc->playing = misc->displaying = misc->current;
      view_screen (misc, daisy);
      misc->current_page_number = atoi (pn);
      wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
      misc->just_this_item = -1;
      open_smil_file (misc, my_attribute, daisy[misc->current].smil_file, anchor);
      free (anchor);
      return;
   } // if (strcasestr (misc->daisy_version, "2.02"))

   if (strcasestr (misc->daisy_version, "3"))
   {
      char *anchor = 0;

      xmlTextReaderClose (misc->reader);
      xmlDocPtr doc = xmlRecoverFile (misc->NCC_HTML);
      if (! (misc->reader = xmlReaderWalker (doc)))
      {
         int e;
         char str[MAX_STR];
         
         e = errno;
         snprintf (str, MAX_STR, 
                  gettext ("\nCannot read %s\n"), misc->NCC_HTML);
         failure (str, e);
      } // if
      do
      {
         if (! get_tag_or_label (misc, my_attribute, misc->reader))
         {
            xmlFreeDoc (doc);
            return;
         } // if
      } while (strcasecmp (misc->tag, "pageTarget") != 0 ||
               strcasecmp (my_attribute->value, pn) != 0);
      do
      {
         if (! get_tag_or_label (misc, my_attribute, misc->reader))
         {
            xmlTextReaderClose (misc->reader);
            xmlFreeDoc (doc);
            return;
         } // if
      } while (strcasecmp (misc->tag, "content") != 0);
      xmlFreeDoc (doc);
      if (strchr (my_attribute->src, '#'))
      {
         anchor = strdup (strchr (my_attribute->src, '#') + 1);
         *strchr (my_attribute->src, '#') = 0;
      } // if
      // search current item
      for (misc->current = misc->total_items - 1; misc->current >= 0; misc->current--)
      {
         if (! daisy[misc->current].page_number)
            continue;
         if (atoi (pn) >= daisy[misc->current].page_number)
            break;
      } // for
      misc->playing = misc->displaying = misc->current;
      view_screen (misc, daisy);
      misc->current_page_number = atoi (pn);
      wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
      misc->just_this_item = -1;
      open_smil_file (misc, my_attribute, my_attribute->src, anchor);
   } // if (strcasestr (misc->daisy_version, "3"))
} // go_to_page_number

void go_to_time (misc_t *misc, daisy_t *daisy, my_attribute_t *my_attribute)
{
   char time_str[10];
   int secs;

   kill (misc->player_pid, SIGKILL);
   misc->player_pid = -2;
   mvwaddstr (misc->titlewin, 1, 0, "----------------------------------------");
   waddstr (misc->titlewin, "----------------------------------------");
   while (1)
   {
      mvwprintw (misc->titlewin, 1, 0, gettext ("Go to time (MM:SS): "));
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
      secs = (time_str[0] - 48) * 600 + (time_str[1] - 48)* 60 +
             (time_str[3] - 48) * 10 + (time_str[4] - 48);
   if (secs >= daisy[misc->current].duration)
   {
      beep ();
      secs = 0;
   } // if
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
   {
      misc->player_pid = play_track (misc, misc->sound_dev, "alsa",
                         daisy[misc->current].first_lsn + (secs * 75));
      misc->seconds = time (NULL) - secs;
   }
   else
   {
      misc->clip_begin = 0;
      open_smil_file (misc, my_attribute, daisy[misc->current].smil_file,
                      daisy[misc->current].anchor);
      do
      {
         if (! get_tag_or_label (misc, my_attribute, misc->reader))
            break;
         if (strcasecmp (misc->tag, "audio") == 0)
         {
            get_clips (misc, my_attribute->clip_begin,
                       my_attribute->clip_end);
         } // if
      } while (misc->clip_begin < secs);
      skip_left (misc, my_attribute, daisy);
   } // if
   misc->playing = misc->displaying = misc->current;
} // go_to_time

void select_next_output_device (misc_t *misc, daisy_t *daisy)
{
   FILE *r;
   int n, y;
   size_t bytes;
   char *list[10], *trash;

   wclear (misc->screenwin);
   wprintw (misc->screenwin, "\nSelect a soundcard:\n\n");
   if (! (r = fopen ("/proc/asound/cards", "r")))
      failure (gettext ("Cannot read /proc/asound/cards"), errno);
   for (n = 0; n < 10; n++)
   {
      list[n] = (char *) malloc (1000);
      bytes = getline (&list[n], &bytes, r);
      if ((int) bytes == -1)
         break;
      trash = (char *) malloc (1000);
      bytes = getline (&trash, &bytes, r);
      free (trash);
      wprintw (misc->screenwin, "   %s", list[n]);
      free (list[n]);
   } // for
   fclose (r);
   y = 3;
   nodelay (misc->screenwin, FALSE);
   for (;;)
   {
      wmove (misc->screenwin, y, 2);
      switch (wgetch (misc->screenwin))
      {
      case 13:
         snprintf (misc->sound_dev, 15, "hw:%i", y - 3);
         view_screen (misc, daisy);
         nodelay (misc->screenwin, TRUE);
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
         view_screen (misc, daisy);
         nodelay (misc->screenwin, TRUE);
         return;
      } // switch
   } // for
} // select_next_output_device

void browse (misc_t *misc, my_attribute_t *my_attribute,
             daisy_t *daisy, char *wd)
{
   int old_screen;
   char str[MAX_STR];

   misc->current = 0;
   misc->just_this_item = -1;
   get_bookmark (misc, my_attribute, daisy);
   view_screen (misc, daisy);
   nodelay (misc->screenwin, TRUE);
   wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
   {
      misc->depth = 1;
      view_screen (misc, daisy);
   } // if

   for (;;)
   {
      switch (wgetch (misc->screenwin))
      {
      case 13: // ENTER
         misc->just_this_item = -1;
         view_screen (misc, daisy);
         misc->playing = misc->displaying = misc->current;
         misc->current_page_number = daisy[misc->playing].page_number;
         if (misc->player_pid > -1)
            kill (misc->player_pid, SIGKILL);
         misc->player_pid = -2;
         if (misc->discinfo)
         {
            snprintf (str, MAX_STR - 1,
                      "cd \"%s\"; \"%s\" \"%s\"/\"%s\" -d %s",
                      wd, PACKAGE, misc->daisy_mp,
                      daisy[misc->current].daisy_mp, misc->sound_dev);
            switch (system (str))
            {
            default:
               break;
            } // switch

            snprintf (str, MAX_STR - 1,
                      "cd \"%s\"; \"%s\" \"%s\" -d %s\n", wd, PACKAGE,
                      misc->daisy_mp, misc->sound_dev);
            switch (system (str))
            {
            default:
               break;
            } // switch
            quit_daisy_player (misc);
            _exit (0);
         } // if
         if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
         {
            misc->player_pid = play_track (misc, misc->sound_dev, "alsa",
                daisy[misc->current].first_lsn);
            misc->seconds = time (NULL);
         }
         else
         {
            open_smil_file (misc, my_attribute, daisy[misc->current].smil_file,
                            daisy[misc->current].anchor);
         } // if
         misc->start_time = 0;
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
         if (misc->current == 0)
         {
            misc->just_this_item = -1;
            view_screen (misc, daisy);
            misc->playing = misc->displaying = misc->current;
            misc->current_page_number = daisy[misc->playing].page_number;
            if (misc->player_pid > -1)
               kill (misc->player_pid, SIGKILL);
            misc->player_pid = -2;
            if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
            {
               misc->player_pid = play_track (misc, misc->sound_dev, "alsa",
                   daisy[misc->current].first_lsn);
               misc->seconds = time (NULL);
            }
            else
            {
               open_smil_file (misc, my_attribute,
                               daisy[misc->current].smil_file,
                               daisy[misc->current].anchor);
            } // if
            misc->start_time = 0;
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
         break;
      case 'e':
      case '.':
      case KEY_DC:
         if (misc->discinfo)
         {
            beep ();
            break;
         } // if
         xmlTextReaderClose (misc->reader);
         quit_daisy_player (misc);
         while (kill (misc->player_pid, 0) == 0);
         if (chdir ("/") == 0)
         {
            char cmd[MAX_CMD + 1];

            snprintf (cmd, MAX_CMD, "eject %s", misc->cd_dev);
            switch (system (cmd))
            {
            default:
               _exit (0);
            } // switch
         } // if
         _exit (0);
      case 'f':
         if (misc->playing == -1)
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
         if (misc->total_pages)
            go_to_page_number (misc, my_attribute, daisy);
         else
            beep ();
         break;
      case 'h':
      case '?':
      {
         int flag = 0;

         if (misc->playing < 0)
            flag = 1;
         if (! misc->discinfo)
            pause_resume (misc, my_attribute, daisy);
         misc->player_pid = -2;
         help (misc, daisy);
         if (flag)
            break;
         misc->playing = misc->displaying;
         if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
         {
            misc->playing = -1;
            pause_resume (misc, my_attribute, daisy);
         }
         else
         {
            skip_left (misc, my_attribute, daisy);
            skip_right (misc);
         } // if
         break;
      }
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
            misc->just_this_item = misc->displaying = misc->playing = misc->current;
            kill (misc->player_pid, SIGKILL);
            misc->player_pid = -2;
            if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
            {
               misc->player_pid = play_track (misc, misc->sound_dev, "alsa",
                   daisy[misc->current].first_lsn);
            }
            else
               open_smil_file (misc, my_attribute,
                 daisy[misc->current].smil_file, daisy[misc->current].anchor);
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
         if (misc->playing == -1)
         {
            beep ();
            break;
         } // if
         pause_resume (misc, my_attribute, daisy);
         select_next_output_device (misc, daisy);
         misc->playing = misc->displaying;
         if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
         {
            misc->playing = -1;
            pause_resume (misc, my_attribute, daisy);
         }
         else
         {
            skip_left (misc, my_attribute, daisy);
            skip_right (misc);
         } // if
         break;
      case 'p':
         put_bookmark (misc);
         break;
      case 'q':
         quit_daisy_player (misc);
         _exit (0);
      case 's':
         if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
         {
            kill (misc->player_pid, SIGKILL);
         }
         else
         {
            kill (misc->player_pid, SIGKILL);
            misc->player_pid = -2;
         } // if
         misc->playing = misc->just_this_item = -1;
         misc->phrase_nr = 0;
         view_screen (misc, daisy);
         wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
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
            kill (misc->player_pid, SIGKILL);
            misc->lsn_cursor += 600;
            misc->displaying = misc->playing = misc->current;
            misc->player_pid = play_track (misc, misc->sound_dev, "alsa",
                        misc->lsn_cursor);
//          misc->seconds = time (NULL) - misc->seconds;
         }
         else
            skip_right (misc);
         break;
      case KEY_LEFT:
      case '4':
         if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
         {
            kill (misc->player_pid, SIGKILL);
            misc->lsn_cursor -= 600;
            misc->displaying = misc->playing = misc->current;
            misc->player_pid = play_track (misc, misc->sound_dev, "alsa",
                        misc->lsn_cursor);
//          misc->seconds = time (NULL) - misc->seconds;
         }
         else
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
         wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
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
         misc->speed += 0.1;
         if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
         {
            kill (misc->player_pid, SIGKILL);
            lsn_t start;

            start = (lsn_t) (misc->start_time +
                         (float) (time (NULL) - misc->seconds) * 75) +
                    daisy[misc->playing].first_lsn;
            misc->player_pid = play_track (misc, misc->sound_dev, "alsa",
                            start);
            view_screen (misc, daisy);
            break;
         } // if
         skip_left (misc, my_attribute, daisy);
         kill (misc->player_pid, SIGKILL);
         view_screen (misc, daisy);
         break;
      case 'D':
      case '-':
         if (misc->speed <= 0.3)
         {
            beep ();
            break;
         } // if
         misc->speed -= 0.1;
         if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
         {
            kill (misc->player_pid, SIGKILL);
            lsn_t start;

            start = (lsn_t) (misc->start_time +
                       (float) (time (NULL) - misc->seconds) * 75) +
                 daisy[misc->playing].first_lsn;
            misc->player_pid = play_track (misc, misc->sound_dev, "alsa",
                            start);
            view_screen (misc, daisy);
            break;
         } // if
         skip_left (misc, my_attribute, daisy);
         kill (misc->player_pid, SIGKILL);
         view_screen (misc, daisy);
         break;
      case KEY_HOME:
      case '*':
      case '7':   
         misc->speed = 1;
         if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
         {
            kill (misc->player_pid, SIGKILL);
            lsn_t start;

            start = (lsn_t) (misc->start_time +
                               (float) (time (NULL) - misc->seconds) * 75) +
                 daisy[misc->playing].first_lsn;
            misc->player_pid = play_track (misc, misc->sound_dev, "alsa",
                            start);
            view_screen (misc, daisy);
            break;
         } // if
         kill (misc->player_pid, SIGKILL);
         skip_left (misc, my_attribute, daisy);
         skip_right (misc);
         view_screen (misc, daisy);
         break;
      default:
         beep ();
         break;
      } // switch

      if (misc->playing > -1 && misc->cd_type != CDIO_DISC_MODE_CD_DA)
      {
         play_clip (misc, my_attribute, daisy);
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
// pause till a key has been pressed or 0.1 misc->seconds are elapsed
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
            misc->start_time = 0;
            misc->current = misc->displaying = ++misc->playing;
            misc->lsn_cursor = daisy[misc->playing].first_lsn;
            if (misc->current >= misc->total_items)
            {
               struct passwd *pw;

               pw = getpwuid (geteuid ());
               quit_daisy_player (misc);
               snprintf (str, MAX_STR - 1, "%s/.daisy-player/%s%s",
                         pw->pw_dir, misc->bookmark_title, get_mcn (misc));
               unlink (str);
               _exit (0);
            } // if
            if (misc->just_this_item > -1)
            {
               kill (misc->player_pid, SIGKILL);
               misc->playing = misc->just_this_item = -1;
            } // if
            view_screen (misc, daisy);
            misc->seconds = time (NULL);
         } // if
         view_time (misc, daisy);
      } // if
   } // for
} // browse

void usage ()
{
   printf (gettext ("Daisy-player - Version %s %s"), PACKAGE_VERSION, "\n");
   puts ("(C)2003-2014 J. Lemmens");
   printf (gettext ("\nUsage: %s [directory_with_a_Daisy-structure] "),
           PACKAGE);
   printf (gettext ("[-c cdrom_device] [-d ALSA_sound_device]\n"));
   printf ("[-i] [-n | -y]\n");
   fflush (stdout);
   _exit (1);
} // usage

char *get_mount_point (misc_t *misc)
{
   FILE *proc;
   size_t len = 0;
   char *str = NULL;

   if (! (proc = fopen ("/proc/mounts", "r")))
      failure (gettext ("\nCannot read /proc/mounts."), errno);
   do
   {
      str = malloc (len + 1);
      if (getline (&str, &len, proc) == -1)
         break;
   } while (! strcasestr (str, "iso9660"));
   fclose (proc);
   if (strcasestr (str, "iso9660"))
   {
      strncpy (misc->daisy_mp, strchr (str, ' ') + 1, MAX_STR - 1);
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
   int h, m, s;
   float t = 0;
   xmlTextReaderPtr di, ncc;
   xmlDocPtr doc;

   doc = xmlRecoverFile (discinfo_html);
   if (! (di = xmlReaderWalker (doc)))
   {
      int e;
      char str[MAX_STR];

      e = errno;
      snprintf (str, MAX_STR, gettext ("\nCannot read %s\n"), discinfo_html);
      failure (str, e);
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
         strncpy (daisy[misc->current].filename, my_attribute->href, MAX_STR - 1);
         xmlDocPtr doc = xmlRecoverFile (daisy[misc->current].filename);
         if (! (ncc = xmlReaderWalker (doc)))
         {
            int e;
            char str[MAX_STR];

            e = errno;
            snprintf (str, MAX_STR, 
               gettext ("\nCannot read %s\n"), daisy[misc->current].filename);
            failure (str, e);
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
         strncpy (daisy[misc->current].label, misc->label, MAX_STR - 1);
         strncpy (daisy[misc->current].daisy_mp, dirname (daisy[misc->current].filename),
                  MAX_STR - 1);
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
   char str[MAX_STR], DISCINFO_HTML[MAX_STR], start_wd[MAX_STR];
   misc_t misc;
   my_attribute_t my_attribute;
   daisy_t *daisy = NULL;

   misc.daisy_player_pid = getpid ();
   misc.speed = 1;
   misc.playing = misc.just_this_item = -1;
   misc.discinfo = 0;
   misc.cd_type = -1;
   misc.ignore_bookmark = 0;
   strncpy (misc.cd_dev, "/dev/sr0", 15);
   signal (SIGCHLD, player_ended);
   read_rc (&misc, &my_attribute);
   setlocale (LC_ALL, "");
   setlocale (LC_NUMERIC, "C");
   textdomain (PACKAGE);
   strncpy (start_wd, get_current_dir_name (), MAX_STR - 1);
   opterr = 0;
   while ((opt = getopt (argc, argv, "c:d:iny")) != -1)
   {
      switch (opt)
      {
      case 'c':
         strncpy (misc.cd_dev, optarg, 15);
         break;
      case 'd':
         strncpy (misc.sound_dev, optarg, 15);
         break;
      case 'i':
         misc.ignore_bookmark = 1;
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
         usage ();
      } // switch
   } // while
   initscr ();
   if (! (misc.titlewin = newwin (2, 80,  0, 0)) ||
       ! (misc.screenwin = newwin (23, 80, 2, 0)))
      failure ("No curses", errno);
   fclose (stderr);
   getmaxyx (misc.screenwin, misc.max_y, misc.max_x);
   printw ("(C)2003-2014 J. Lemmens\n");
   printw (gettext ("Daisy-player - Version %s %s"), PACKAGE_VERSION, "\n");
   printw (gettext ("A parser to play Daisy CD's with Linux\n"));

   printw (gettext ("Scanning for a Daisy CD..."));
   refresh ();
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
         usage ();
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
            failure ("mkdtemp ()", errno);
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
            failure (str, errno);
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
         usage ();
      } // if
      magic_close (myt);
   } // if there is an argument
   else
// try misc.cd_dev
   {
      if (access (misc.cd_dev, R_OK) != 0)
         failure (misc.cd_dev, errno);

      char cmd[MAX_CMD], *tmp;
      time_t start;
      CdIo_t *cd;

      snprintf (cmd, MAX_CMD, "eject -t %s", misc.cd_dev);
      switch (system (cmd))
      {
      default:
         break;
      } // switch
      start = time (NULL);
      tmp = strdup ("/tmp/daisy-player.XXXXXX");
      strncpy (misc.daisy_mp, mkdtemp (tmp), MAX_STR);
      cd = NULL;
      cdio_init ();
      do
      {
         if (time (NULL) - start >= 30)
            failure (gettext ("No Daisy CD in drive."), errno);
         cd = cdio_open (misc.cd_dev, DRIVER_UNKNOWN);
      } while (cd == NULL);
      start = time (NULL);
      do
      {
         if (time (NULL) - start >= 30)
            failure (gettext ("No Daisy CD in drive."), errno);
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
// set the CD speed so it makes less noise
            set_drive_speed (&misc, 4);

            do
// if not found a mounted cd, try to mount one
            {
               if (time (NULL) - start >= 30)
                  failure (gettext ("No Daisy CD in drive."), errno);
               snprintf (cmd, MAX_CMD,
                         "udisksctl mount -b %s > /dev/null", misc.cd_dev);
               switch (system (cmd))
               {
                  break;
               } // switch
               get_mount_point (&misc);
            } while (! get_mount_point (&misc));
            break;
         } // TRACK_COUNT_DATA"
         case CDIO_DISC_MODE_CD_DA: /**< CD-DA */
         {
// probably an Audio-CD
            printw (gettext ("\nFound an Audio-CD. "));
            if (misc.cddb_flag == 'y')
               printw (gettext ("Get titles from freedb.freedb.org..."));
            refresh ();
            strncpy (misc.bookmark_title, "Audio-CD", MAX_STR - 1);
            strncpy (misc.daisy_title, "Audio-CD", MAX_STR - 1);
// set the CD speed so it makes less noise
            set_drive_speed (&misc, 4);
            init_paranoia (&misc);
            daisy = get_number_of_tracks (&misc);
            get_toc_audiocd (&misc, daisy);
            strncpy (misc.daisy_mp, "/tmp", MAX_STR - 1);
            break;
         } //  TRACK_COUNT_AUDIO
         case CDIO_DISC_MODE_CD_I:
         case CDIO_DISC_MODE_DVD_OTHER:
         case CDIO_DISC_MODE_NO_INFO:
         case CDIO_DISC_MODE_ERROR:
            failure (gettext ("\nNo DAISY-CD or Audio-cd found\n"), errno);
         } // switch       
      } while (misc.cd_type == -1);
   } // if use misc.cd_dev
   keypad (misc.screenwin, TRUE);
   meta (misc.screenwin,       TRUE);
   nonl ();
   noecho ();
   misc.player_pid = -2;
   if (chdir (misc.daisy_mp) == -1)
   {
      int e;
      char str[MAX_STR];

      e = errno;
      snprintf (str, MAX_STR, "\ndaisy_mp %s", misc.daisy_mp);
      failure (str, e);
   } // if
   if (misc.cd_type != CDIO_DISC_MODE_CD_DA)
   {
      daisy = create_daisy_struct (&misc, &my_attribute);
      snprintf (DISCINFO_HTML, MAX_STR - 1, "discinfo.html");
      if (access (DISCINFO_HTML, R_OK) == 0)
         handle_discinfo (&misc, &my_attribute, daisy, DISCINFO_HTML);
      if (! misc.discinfo)
      {
         snprintf (misc.NCC_HTML, MAX_STR - 1, "ncc.html");
         if (access (misc.NCC_HTML, R_OK) == 0)
         {
            xmlDocPtr doc;

            doc = xmlRecoverFile (misc.NCC_HTML);
            if (! (misc.reader = xmlReaderWalker (doc)))
            {
               int e;
               char str[MAX_STR];

               e = errno;
               snprintf (str, MAX_STR,
                           gettext ("\nCannot read %s\n"), misc.NCC_HTML);
               failure (str, e);
            } // if
            while (1)
            {
               if (! get_tag_or_label (&misc, &my_attribute, misc.reader))
                  break;
               if (strcasecmp (misc.tag, "title") == 0)
               {
                  if (! get_tag_or_label (&misc, &my_attribute, misc.reader))
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
            read_daisy_2 (&misc, &my_attribute, daisy);
         }
         else
         {
            read_daisy_3 (&misc, &my_attribute, daisy);
            strncpy (misc.daisy_version, "3", 2);
         } // if
         if (misc.total_items == 0)
            misc.total_items = 1;
      } // if (! misc.discinfo);
   } // if misc.audiocd == 0

   wattron (misc.titlewin, A_BOLD);
   snprintf (str, MAX_STR - 1, gettext ("Daisy-player - Version %s %s"),
             PACKAGE_VERSION, " - (C)2014 J. Lemmens");
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
      failure ("mkstemp ()", errno);
   browse (&misc, &my_attribute, daisy, start_wd);
   return 0;
} // main
