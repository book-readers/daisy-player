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

 #include "daisy.h"

extern struct my_attribute my_attribute;
extern int current, displaying, max_y, total_items, current_page_number;
extern int phrase_nr, tts_no;
extern float speed;
extern char sound_dev[];
extern char tag[], label[], daisy_title[], bookmark_title[];
float clip_begin, clip_end;
extern char daisy_version[], daisy_mp[];

int depth = 0, total_pages, level = 0;
char ncc_totalTime[100];
char daisy_language[255];
daisy_t daisy[2000];

void parse_ncx (char *);

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

void get_clips (char *orig_begin, char *end)
{
   char begin_str[100], *begin;

   strncpy (begin_str, orig_begin, 90);
   begin = begin_str;
   while (! isdigit (*begin))
      begin++;
   if (strchr (begin, 's'))
      *strchr (begin, 's') = 0;
   if (! strchr (begin, ':'))
      clip_begin = (float) atof (begin);
   else
      clip_begin = read_time (begin);

// fill end
   while (! isdigit (*end))
      end++;
   if (strchr (end, 's'))
      *strchr (end, 's') = 0;
   if (! strchr (end, ':'))
      clip_end = (float) atof (end);
   else
      clip_end = read_time (end);
} // get_clips

void realname (char *name)
{
   DIR *dir;
   struct dirent *dirent;
   char *names[100], *dname, *bname, path[1000];
   int i, j, found;

   strncpy (path, name, 990);
   for (i = 0; i < 100; i++)
   {
      bname = strdup (basename (path));
      names[i] = strdup (bname);
      dname = strdup (dirname (path));
      if (strcmp (dname, ".") == 0)
         break;
      strncpy (path, dname, 990);
      if (strcmp (path, names[i]) == 0)
         break;
   } // for

    strncpy (path, get_current_dir_name (), 990);
   for (j = i; j >= 0; j--)
   {
      if (! (dir = opendir (path)))
      {
         endwin ();
         playfile (PREFIX"share/daisy-player/error.wav", "1");
         printf (gettext ("\nCannot read %s\n"), get_current_dir_name ());
         fflush (stdout);
         _exit (1);
      } // if
      found = 0;
      while ((dirent = readdir (dir)) != NULL)
      {
         if (strcasecmp (dirent->d_name, names[j]) == 0)
         {
            if (path[strlen (path) - 1] != '/')
               strcat (path, "/");
            strcat (path, dirent->d_name);
            found = 1;
            break;
         } // if
      } // while
      closedir (dir);
      if (! found)
         return;
   } // for
   strncpy (name, path, 90);
} // realname

void get_attributes (xmlTextReaderPtr reader)
{
   if (xmlTextReaderGetAttribute (reader, (const xmlChar *) "class"))
      strncpy (my_attribute.class, (char *) xmlTextReaderGetAttribute
               (reader, (const xmlChar *) "class"), 90);
   if (xmlTextReaderGetAttribute (reader, (const xmlChar *) "clip-begin"))
      strncpy (my_attribute.clip_begin, (char *) xmlTextReaderGetAttribute
               (reader, (const xmlChar *) "clip-begin"), 90);
   if (xmlTextReaderGetAttribute (reader, (const xmlChar *) "clipBegin"))
      strncpy (my_attribute.clip_begin, (char *) xmlTextReaderGetAttribute
               (reader, (const xmlChar *) "clipBegin"), 90);
   if (xmlTextReaderGetAttribute (reader, (const xmlChar *) "clip-end"))
      strncpy (my_attribute.clip_end, (char *) xmlTextReaderGetAttribute
               (reader, (const xmlChar *) "clip-end"), 90);
   if (xmlTextReaderGetAttribute (reader, (const xmlChar *) "clipEnd"))
      strncpy (my_attribute.clip_end, (char *) xmlTextReaderGetAttribute
               (reader, (const xmlChar *) "clipEnd"), 90);
   if (xmlTextReaderGetAttribute (reader, (const xmlChar *) "href"))
      strncpy (my_attribute.href, (char *) xmlTextReaderGetAttribute
               (reader, (const xmlChar *) "href"), 90);
   if (xmlTextReaderGetAttribute (reader, (const xmlChar *) "id"))
      strncpy (my_attribute.id, (char *) xmlTextReaderGetAttribute
               (reader, (const xmlChar *) "id"), 90);
   if (xmlTextReaderGetAttribute (reader, (const xmlChar *) "idref"))
      strncpy (my_attribute.idref, (char *) xmlTextReaderGetAttribute
               (reader, (const xmlChar *) "idref"), 90);
   if (xmlTextReaderGetAttribute (reader, (const xmlChar *) "item"))
      current = atoi ((char *) xmlTextReaderGetAttribute
                (reader, (const xmlChar *) "item"));
   if (xmlTextReaderGetAttribute (reader, (const xmlChar *) "level"))
      level = atoi ((char *) xmlTextReaderGetAttribute
                (reader, (const xmlChar *) "level"));
   if (xmlTextReaderGetAttribute (reader, (const xmlChar *) "media-type"))
      strncpy (my_attribute.media_type, (char *) xmlTextReaderGetAttribute
               (reader, (const xmlChar *) "media-type"), 90);
   if (xmlTextReaderGetAttribute (reader, (const xmlChar *) "name"))
   {
      char name[100], content[100];

      *name = 0;
      if (xmlTextReaderGetAttribute (reader, (const xmlChar *) "name"))
         strncpy (name, (char *) xmlTextReaderGetAttribute (reader,
                  (const xmlChar *) "name"), 90);
      *content = 0;
      if (xmlTextReaderGetAttribute (reader, (const xmlChar *) "content"))
         strncpy (content, (char *) xmlTextReaderGetAttribute (reader,
                  (const xmlChar *) "content"), 90);
      if (strcasestr (name, "dc:format"))
         strncpy (daisy_version, content, 20);
      if (strcasestr (name, "dc:title") && ! *daisy_title)
         strncpy (daisy_title, content, 90);
/* don't use it
      if (strcasestr (name, "dtb:depth"))
         depth = atoi (content);
*/
      if (strcasestr (name, "dtb:totalPageCount"))
         total_pages = atoi (content);
      if (strcasestr (name, "ncc:depth"))
         depth = atoi (content);
      if (strcasestr (name, "ncc:maxPageNormal"))
         total_pages = atoi (content);
      if (strcasestr (name, "ncc:pageNormal"))
         total_pages = atoi (content);
      if (strcasestr (name, "ncc:page-normal"))
            total_pages = atoi (content);
      if (strcasestr (name, "dtb:totalTime")  ||
          strcasestr (name, "ncc:totalTime"))
      {
         strncpy (ncc_totalTime, content, 90);
         if (strchr (ncc_totalTime, '.'))
            *strchr (ncc_totalTime, '.') = 0;
      } // if
   } // if
   if (xmlTextReaderGetAttribute (reader, (const xmlChar *) "playorder"))
      strncpy (my_attribute.playorder, (char *) xmlTextReaderGetAttribute
               (reader, (const xmlChar *) "playorder"), 90);
   if (xmlTextReaderGetAttribute (reader, (const xmlChar *) "phrase"))
      phrase_nr = atoi ((char *) xmlTextReaderGetAttribute       
                (reader, (const xmlChar *) "phrase"));
   if (xmlTextReaderGetAttribute (reader, (const xmlChar *) "sound_dev"))
      strncpy (sound_dev, (char *) xmlTextReaderGetAttribute
               (reader, (const xmlChar *) "sound_dev"), 90);
   if (xmlTextReaderGetAttribute (reader, (const xmlChar *) "speed"))
      speed = atof ((char *) xmlTextReaderGetAttribute
         (reader, (const xmlChar *) "speed"));
   if (xmlTextReaderGetAttribute (reader, (const xmlChar *) "src"))
      strncpy (my_attribute.src, (char *) xmlTextReaderGetAttribute
               (reader, (const xmlChar *) "src"), 90);
   if (xmlTextReaderGetAttribute (reader, (const xmlChar *) "tts"))
       tts_no = atof ((char *) xmlTextReaderGetAttribute
               (reader, (const xmlChar *) "tts"));
   if (xmlTextReaderGetAttribute (reader, (const xmlChar *) "toc"))
      strncpy (my_attribute.toc, (char *) xmlTextReaderGetAttribute
               (reader, (const xmlChar *) "toc"), 90);
   if (xmlTextReaderGetAttribute (reader, (const xmlChar *) "value"))
      strncpy (my_attribute.value, (char *) xmlTextReaderGetAttribute
               (reader, (const xmlChar *) "value"), 90);
} // get_attributes

int get_tag_or_label (xmlTextReaderPtr local_reader)
{
   int type;

   *tag = *label = 0;
   *my_attribute.class = *my_attribute.clip_begin = *my_attribute.clip_end =
   *my_attribute.href = *my_attribute.id = *my_attribute.media_type =
   *my_attribute.playorder = * my_attribute.smilref = *my_attribute.src =
   *my_attribute.toc = 0, *my_attribute.value = 0;

   switch (xmlTextReaderRead (local_reader))
   {
   case -1:
      endwin ();
      printf ("Can't handle this DTB structure!\n");
      printf ("Don't know how to handle it yet, sorry. :-(\n");
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      fflush (stdout);
      _exit (1);
   case 0:
      return 0;
   case 1:
      break;
   } // swwitch
   type = xmlTextReaderNodeType (local_reader);
   switch (type)
   {
   int n, i;
   char *p;

   case -1:
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf (gettext ("\nCannot read\n"));
      fflush (stdout);
      _exit (1);
   case XML_READER_TYPE_ELEMENT:
      strncpy (tag, (char *) xmlTextReaderConstName (local_reader), 90);
      n = xmlTextReaderAttributeCount (local_reader);
      for (i = 0; i < n; i++)
         get_attributes (local_reader);
      return 1;
   case XML_READER_TYPE_END_ELEMENT:
      snprintf (tag, 90, "/%s", (char *) xmlTextReaderName (local_reader));
      return 1;
   case XML_READER_TYPE_TEXT:
      strncpy (label, (char *) xmlTextReaderConstValue (local_reader),
                      max_phrase_len);
      p = label;
      while (*p)
      {
         if (! isascii (*p) || *p == '\n')
            *p = 32;
         p++;
      } // while
      p = label;
      while (isspace (*p++));
      strncpy (label, --p, max_phrase_len);
      return 1;
   case XML_READER_TYPE_ENTITY_REFERENCE:   
   case XML_READER_TYPE_DOCUMENT_TYPE:
   case XML_READER_TYPE_SIGNIFICANT_WHITESPACE:
      return 1;
   default:
      return 1;
   } // switch
   return 0;
} // get_tag_or_label                       

void parse_text_file (char *text_file)
// page-number
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
      fflush (stdout);
      _exit (1);
   } // if
   if (anchor)
   {
      do
      {
         if (! get_tag_or_label (textptr))
         {
            xmlTextReaderClose (textptr);
            xmlFreeDoc (doc);
            return;
         } // if
      } while (strcasecmp (my_attribute.id, anchor) != 0);
   } // if
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
      fflush (stdout);
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

void parse_manifest (char *name, char *id_ptr)
{
   xmlTextReaderPtr manifest;
   char *id, *toc;

   id = strdup (id_ptr);
   if (! *id)
      return;
   toc = strdup (my_attribute.toc);
   realname (name);
   xmlDocPtr doc = xmlRecoverFile (name);
   if (! (manifest = xmlReaderWalker (doc)))
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf (gettext ("\nCannot read %s\n"), name);
      fflush (stdout);
      _exit (1);
   } // if
   do
   {
      if (! get_tag_or_label (manifest))
         break;
      if (strcasecmp (tag, "dc:language") == 0  && ! *daisy_language)
      {
         do
         {
            if (! get_tag_or_label (manifest))
               break;
            if (*label)
               break;
         } while (strcasecmp (tag, "/dc:language") != 0);
         strncpy (daisy_language, label, 90);
      } // if dc:language
      if (strcasecmp (tag, "dc:title") == 0 && ! *daisy_title)
      {
         do
         {
            if (! get_tag_or_label (manifest))
               break;
            if (*label)
               break;
         } while (strcasecmp (tag, "/dc:title") != 0);
         strncpy (daisy_title, label, 90);
      } // if dc:title
   } while (strcasecmp (tag, "manifest") != 0);
   while (1)
   {
      if (! get_tag_or_label (manifest))
         break;
      if (*toc && strcasecmp (my_attribute.id, id) == 0)
      {
         realname (my_attribute.href);
         parse_ncx (my_attribute.href);
         return;
      } // if
      if (*id && strcasecmp (my_attribute.id, id) == 0)
      {
         realname (my_attribute.href);
         strncpy (daisy[current].smil_file, my_attribute.href, 250);
         snprintf (daisy[current].label, 5, "%d", current + 1);
         daisy[current].screen = current / max_y;
         daisy[current].y = current - (daisy[current].screen * max_y);
         daisy[current].x = 1;
         current++;
      } // if (strcasecmp (my_attribute.id, id) == 0)
   } // while
} // parse_manifest

void parse_opf (char *name, char *flag)
{
   xmlTextReaderPtr opf;

   realname (name);
   xmlDocPtr doc = xmlRecoverFile (name);
   if (! (opf = xmlReaderWalker (doc)))
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf (gettext ("\nCannot read %s\n"), name);
      fflush (stdout);
      _exit (1);
   } // if
   current = displaying = 0;
   while (1)
   {
      if (! get_tag_or_label (opf))
         break;
      if (strcasecmp (tag, "dc:language") == 0)
      {
         do
         {
            if (! get_tag_or_label (opf))
               break;
         } while (! *label);
         strncpy (daisy_language, label, 90);
      } // if dc:language
      if (strcasecmp (tag, "dc:title") == 0 && ! *daisy_title)
      {
         do
         {
            if (! get_tag_or_label (opf))
               break;
         } while (! *label);
         strncpy (daisy_title, label, 90);
      } // if dc:title
      if (strcasecmp (tag, "manifest") == 0)
      {
         do
         {
            if (! get_tag_or_label (opf))
               break;
         } while (strcasecmp (tag, "/manifest") != 0);
      } // if (strcasecmp (tag, "manifest") == 0)
      if (strcasecmp (tag, "spine") == 0)
      {
         if (*my_attribute.toc && strcasecmp (flag, "ignore_ncx") != 0)
         {
            parse_manifest (name, my_attribute.toc);
            return;
         } // if
         do
         {
            if (! get_tag_or_label (opf))
               break;
            if (strcasecmp (tag, "itemref") == 0)
               parse_manifest (name, my_attribute.idref);
         } while (strcasecmp (tag, "/spine") != 0);
      } // if spine
   } // while
   total_items = current;
} // parse_opf

char *convert (char *s)
{
   int x = 0, n = 0;
   static char new[255];

   do
   {
      if (s[x] == '%')
      {
         char hex[10];

         x++;
         hex[0] = '0';
         hex[1] = 'x';
         hex[2] = s[x++];
         hex[3] = s[x++];
         hex[4] = 0;
         new[n++] = strtod (hex, NULL);
      }
      else
         new[n++] = s[x++];
   } while (s[x - 1]);
   return new;
} // convert

void get_label (int item, int indent)
{
   char *ptr;

   ptr = label;
   do
   {
      if (isspace (*ptr))
         ptr++;
   } while (isspace (*ptr));
   strncpy (daisy[item].label, ptr, 80);
   daisy[item].label[64 - daisy[item].x] = 0;
   if (displaying == max_y)
      displaying = 1;
   if (strcasecmp (daisy[item].class, "pagenum") == 0)
      daisy[item].x = 0;
   else
      if (daisy[item].x == 0)
         daisy[item].x = indent + 3;
      daisy[item].screen = item / max_y;
      daisy[item].y = item - daisy[item].screen * max_y;
} // get_label

void parse_ncx (char *name)
{
   int indent, found_page = 0;
   xmlTextReaderPtr ncx;
   xmlDocPtr doc;

   realname (name);
   doc = xmlRecoverFile (name);
   if (! (ncx = xmlReaderWalker (doc)))
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf (gettext ("\nCannot read %s\n"), name);
      fflush (stdout);
      _exit (1);
   } // if
   current = displaying = level = depth = 0;
   while (1)
   {
      if (! get_tag_or_label (ncx))
         break;
      if (strcasecmp (tag, "docTitle") == 0)
      {
         do
         {
            if (! get_tag_or_label (ncx))
               break;
         } while (! *label);
         strncpy (daisy_title, label, 90);
         strncpy (bookmark_title, label, 90);
      } // if (strcasecmp (tag, "docTitle") == 0)
      if (strcasecmp (tag, "docAuthor") == 0)
      {
         do
         {
            if (! get_tag_or_label (ncx))
               break;
         } while (strcasecmp (tag, "/docAuthor") != 0);
      } // if (strcasecmp (tag, "docAuthor") == 0)
      if (strcasecmp (tag, "navPoint") == 0)
      {
         level++;
         if (level > depth)
            depth = level;
         daisy[current].page_number = 0;
         daisy[current].playorder = atoi (my_attribute.playorder);
         daisy[current].level = level;
         daisy[current].x = level + 2;
         indent = daisy[current].x = (level - 1) * 3 + 1;
         strncpy (daisy[current].class, my_attribute.class, 90);
         while (1)
         {
            if (! get_tag_or_label (ncx))
               break;
            if (strcasecmp (tag, "text") == 0)
            {
               while (1)
               {
                  if (! get_tag_or_label (ncx))
                     break;
                  if (strcasecmp (tag, "/text") == 0)
                     break;
                  if (*label);
                  {
                     get_label (current, indent);
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
      if (strcasecmp (tag, "/navpoint") == 0)
         level--;
      if (strcasecmp (tag, "page") == 0 || found_page)
      {
         found_page = 0;
         strncpy (daisy[++current].label, my_attribute.number, 90);
         strncpy (daisy[current].smil_file, name, 250);
         daisy[current].screen = current / max_y;
         daisy[current].y = current - (daisy[current].screen * max_y);
         daisy[current].x = 2;
      } // if (strcasecmp (tag, "page") == 0)
   } // while
   xmlTextReaderClose (ncx);
   xmlFreeDoc (doc);
} // parse_ncx
