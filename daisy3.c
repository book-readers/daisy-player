/* daisy3.c - functions to insert daisy3 info into a struct.
 *  Copyright (C) 2012 J. Lemmens
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

#include "daisy-player.h"

extern int get_tag_or_label (xmlTextReaderPtr), current_page_number;
extern daisy_t daisy[];
extern struct my_attribute my_attribute;
extern int current, displaying, max_y, total_items;
extern char tag[], label[], daisy_title[], bookmark_title[], NCC_HTML[100];
extern float clip_begin, clip_end;
extern void realname (char *);

char daisy_language[255];

void parse_text_file (char *text_file)
{
   xmlTextReaderPtr textptr;
   char *anchor = 0;

   if (strchr (text_file, '#'))
   {
      anchor = strdup (strchr (text_file, '#') + 1);
      *strchr (text_file, '#') = 0;
   } // if
   realname (text_file);
   xmlDocPtr doc = xmlRecoverFile (text_file);
   if (! (textptr = xmlReaderWalker (doc)))
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf (gettext ("\nCannot read %s\n"), text_file);
      _exit (1);
   } // if
   while (1)
   {
      if (! get_tag_or_label (textptr))
      {
         xmlTextReaderClose (textptr);
         xmlFreeDoc (doc);
         return;
      } // if
      if (strcasecmp (my_attribute.id, anchor) == 0)
         break;
   } // while
   do
   {
      if (! get_tag_or_label (textptr))
      {
         xmlTextReaderClose (textptr);
         xmlFreeDoc (doc);
         return;
      } // if
   } while (! *label);
   daisy[current].page_number = atoi (label);
   xmlTextReaderClose (textptr);
   xmlFreeDoc (doc);
} // parse_text_file

void get_page_number_3 (xmlTextReaderPtr reader)
{
   xmlDocPtr doc;
   xmlTextReaderPtr page;
   char *anchor = 0;

   do
   {
      if (! get_tag_or_label (reader))
         return;
   } while (strcasecmp (tag, "text") != 0);
   if (strchr (my_attribute.src, '#'))
   {
      anchor = strdup (strchr (my_attribute.src, '#') + 1);
      *strchr (my_attribute.src, '#') = 0;
   } // if
   realname (my_attribute.src);
   doc = xmlRecoverFile (my_attribute.src);
   if (! (page = xmlReaderWalker (doc)))
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf (gettext ("\nCannot read %s\n"), my_attribute.src);
      fflush (stdout);
      _exit (1);
   } // if
   do
   {
      if (! get_tag_or_label (page))
         return;
   } while (strcasecmp (my_attribute.id, anchor) != 0);
   do
   {
      if (! get_tag_or_label (page))
         return;
   } while (! *label);
   xmlTextReaderClose (page);
   xmlFreeDoc (doc);
   current_page_number = atoi (label);
} // get_page_number_3

void parse_smil_3 ()
{
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
      while (1)
      {
         if (! get_tag_or_label (parse))
            break;
         if (strcasecmp (daisy[x].anchor, my_attribute.id) == 0)
            break;
      } // while                                        
      daisy[x].duration = 0;
      while (1)
      {
         if (! get_tag_or_label (parse))
            break;
// get clip_begin
         if (strcasecmp (tag, "audio") == 0)
         {
            get_clips (my_attribute.clip_begin, my_attribute.clip_end);
            daisy[x].begin = clip_begin;
            daisy[x].duration += clip_end - clip_begin;
// get clip_end
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
               } // IF
            } // while
            if (*daisy[x + 1].anchor)
               if (strcasecmp (my_attribute.id, daisy[x + 1].anchor) == 0)
                  break;
         } // if (strcasecmp (tag, "audio") == 0)
      } // while
      xmlTextReaderClose (parse);
      xmlFreeDoc (doc);
   } // for
} // parse_smil_3

void parse_content (char *src)
{
   int found_anchor, found_text;
   char text_file[100];
   xmlTextReaderPtr content;

   strncpy (daisy[current].smil_file, src, 250);
   if (strchr (daisy[current].smil_file, '#'))
   {
      strncpy (daisy[current].anchor,
                           strchr (daisy[current].smil_file, '#') + 1, 250);
      *strchr (daisy[current].smil_file, '#') = 0;
   } // if
   realname (daisy[current].smil_file);
   xmlDocPtr doc = xmlRecoverFile (daisy[current].smil_file);
   if (! (content = xmlReaderWalker (doc)))
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf (gettext ("\nCannot read %s\n"), daisy[current].smil_file);
      _exit (1);
   } // if
   found_anchor = found_text = 0;
   while (1)
   {
      if (! get_tag_or_label (content))
      {
         xmlTextReaderClose (content);
         xmlFreeDoc (doc);
         return;
      } // if
      if (strcasecmp (tag, "text") == 0)
      {
         found_text = 1;
         strncpy (text_file, my_attribute.src, 90);
      } // if
      if (strcasecmp (my_attribute.id, daisy[current].anchor) == 0)
         found_anchor = 1;
      if (found_anchor && found_text)
         break;
   } // while
   parse_text_file (text_file);
   xmlTextReaderClose (content);
   xmlFreeDoc (doc);
} // parse_content

void parse_opf_ncx (char *name)
{
   int indent, flag = 0;
   xmlTextReaderPtr reader;

   realname (name);
   xmlDocPtr doc = xmlRecoverFile (name);
   if (! (reader = xmlReaderWalker (doc)))
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf (gettext ("\nCannot read %s\n"), name);
      _exit (1);
   } // if
   current = displaying = 0;
   while (1)
   {
      if (! get_tag_or_label (reader))
         break;
      if (strcasecmp (tag, "item") == 0 &&
          strcasecmp (my_attribute.id, "ncx") == 0)
      {
         realname (my_attribute.href);
         strncpy (NCC_HTML, my_attribute.href, 90);
         parse_opf_ncx (my_attribute.href);
         return;
      } // if (strcasecmp (my_attribute.id, "ncx") == 0)
      if (strcasecmp (tag, "dc:language") == 0)
      {
         do
         {
            if (! get_tag_or_label (reader))
               break;
         } while (! *label);
         strncpy (daisy_language, label, 90);
      } // if dc:language
      if (strcasecmp (tag, "dc:title") == 0)
      {
         do
         {
            if (! get_tag_or_label (reader))
               break;
         } while (! *label);
         strncpy (daisy_title, label, 90);
      } // if dc:title
      if (strcasecmp (tag, "spine") == 0)
      {
         while (1)
         {
            if (! get_tag_or_label (reader))
               break;
            if (strcasecmp (tag, "itemref") == 0)
            {
               char idref[100];

               realname (name);
               xmlDocPtr doc = xmlRecoverFile (name);
               if (! (reader = xmlReaderWalker (doc)))
               {
                  endwin ();
                  playfile (PREFIX"share/daisy-player/error.wav", "1");
                  printf (gettext ("\nCannot read %s\n"), name);
                  _exit (1);
               } // if
               strncpy (idref, my_attribute.idref, 90);
               while (1)
               {
                  if (! get_tag_or_label (reader))
                     break;
                  if (strcasecmp (my_attribute.id, idref) == 0)
                     break;
               } // while
               strncpy (daisy[current].smil_file, my_attribute.href, 90);
               daisy[current].x = 1;
               daisy[current].screen = current / max_y;
               daisy[current].y = current - daisy[current].screen * max_y;
               sprintf (daisy[current].label, "%d", current);
               current++;
               break;
            } // if (strcasecmp (tag, "itemref") == 0)
         } // while
      } // if spine
      if (strcasecmp (tag, "docTitle") == 0)
      {
         do
         {
            if (! get_tag_or_label (reader))
               break;
         } while (! *label);
         strncpy (daisy_title, label, 90);
         strncpy (bookmark_title, label, 90);
      } // if
      if (strcasecmp (tag, "navPoint") == 0)
      {
         int l;

         daisy[current].page_number = 0;
         daisy[current].playorder = atoi (my_attribute.playorder);
         l = my_attribute.class[1] - '0';
         daisy[current].level = l;
         indent = daisy[current].x = (l - 1) * 3 + 1;
         strncpy (daisy[current].class, my_attribute.class, 90);
         while (1)
         {
            if (! get_tag_or_label (reader))
               break;
            if (strcasecmp (tag, "text") == 0)
            {
               while (1)
               {
                  if (! get_tag_or_label (reader))
                     break;
                  if (*label);
                  {
                     get_label (flag, indent);
                     daisy[current].screen = current / max_y;
                     daisy[current].y = current - daisy[current].screen * max_y;
                     strncpy (daisy[current].label, label, 90);
                     break;
                  } // if
               } // while
            } // if (strcasecmp (tag, "text") == 0)
            if (strcasecmp (tag, "content") == 0)
            {
               parse_content (my_attribute.src);
               current++;
               break;
            } // if (strcasecmp (tag, "content") == 0)
         } // while
      } // if (strcasecmp (tag, "navPoint") == 0)
   } // while
} // parse_opf_ncx

void read_daisy_3 (int flag, char *daisy_mp)
{
   DIR *dir;
   struct dirent *dirent;

   if (! (dir = opendir (daisy_mp)))
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf (gettext ("\nCannot read %s\n"), daisy_mp);
      _exit (1);
   } // if

// parse opf
   while ((dirent = readdir (dir)) != NULL)
   {
      if (strcasecmp (dirent->d_name +
                      strlen (dirent->d_name) - 4, ".opf") == 0)
      {
         strncpy (NCC_HTML, dirent->d_name, 90);
         parse_opf_ncx (dirent->d_name);
         break;
      } // if
   } // while
   closedir (dir);
   total_items = current;
   parse_smil_3 ();
} // read_daisy_3
