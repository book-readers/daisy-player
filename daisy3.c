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

extern mxml_node_t *get_tag_or_label (mxml_node_t *, mxml_node_t *);
extern daisy_t daisy[];
extern struct my_attribute my_attribute;
extern int current, max_y, total_items;
extern char tag[], label[], daisy_title[], bookmark_title[], NCC_HTML[100];
extern void realname (char *);

char daisy_language[255];

void parse_content (char *src)
{
   FILE *r;
   char file[100], *anchor = 0;
   int found_anchor, found_text;
   mxml_node_t *page_tree, *page_node;

   strncpy (daisy[current].smil_file, src, 250);
   if (strchr (daisy[current].smil_file, '#'))
   {
      strncpy (daisy[current].anchor,
                           strchr (daisy[current].smil_file, '#') + 1, 250);
      *strchr (daisy[current].smil_file, '#') = 0;
   } // if
   realname (daisy[current].smil_file);
   if (! (r = fopen (daisy[current].smil_file, "r")))
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf (gettext ("\nCannot read %s\n"), daisy[current].smil_file);
      fflush (stdout);
      _exit (1);
   } // if
   page_tree = mxmlLoadFile (NULL, r, MXML_NO_CALLBACK);
   fclose (r);
   page_node = mxmlFindElement (page_tree, page_tree, NULL,
                  NULL, NULL, MXML_DESCEND_FIRST);
   found_anchor = found_text = 0;
   while (1)
   {
      if ((page_node = get_tag_or_label (page_node, page_tree)) == NULL)
         return;
      if (strcasecmp (tag, "text") == 0)
      {
         found_text = 1;
         strncpy (file, my_attribute.src, 90);
      } // if
      if (strcasecmp (my_attribute.id, daisy[current].anchor) == 0)
         found_anchor = 1;
      if (found_anchor && found_text)
         break;
   } // while
   if (strchr (file, '#'))
   {
      anchor = strchr (file, '#') + 1;
      *strchr (file, '#') = 0;
   } // if
   realname (file);
   if (! (r = fopen (file, "r")))
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf (gettext ("\nCannot read %s\n"), file);
      fflush (stdout);
      _exit (1);
   } // if
   page_tree = mxmlLoadFile (NULL, r, MXML_NO_CALLBACK);
   fclose (r);
   page_node = mxmlFindElement (page_tree, page_tree, NULL,
            NULL, NULL, MXML_DESCEND_FIRST);
   while (1)
   {
      if ((page_node = get_tag_or_label (page_node, page_tree)) == NULL)
         return;
      if (strcasecmp (my_attribute.id, anchor) == 0)
         break;
   } // while
   while (1)
   {
      if ((page_node = get_tag_or_label (page_node, page_tree)) == NULL)
         return;
      if (*label)
         break;
   } // while
   daisy[current].page_number = atoi (label);
} // parse_content

void parse_opf_ncx (char *name)
{
   int indent, flag = 0;
   FILE *r;
   mxml_node_t *node, *tree;

   realname (name);
   if (! (r = fopen (name, "r")))
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf (gettext ("\nCannot read %s\n"), name);
      fflush (stdout);
      _exit (1);
   } // if
   tree = mxmlLoadFile (NULL, r, MXML_NO_CALLBACK);
   fclose (r);
   node = mxmlFindElement (tree, tree, NULL, NULL, NULL, MXML_DESCEND_FIRST);
   while (1)
   {
      if ((node = get_tag_or_label (node, tree)) == NULL)
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
            if ((node = get_tag_or_label (node, tree)) == NULL)
               break;
         } while (! *label);
         strncpy (daisy_language, label, 90);
      } // if dc:language
      if (strcasecmp (tag, "dc:title") == 0)
      {
         do
         {
            if ((node = get_tag_or_label (node, tree)) == NULL)
               break;
         } while (! *label);
         strncpy (daisy_title, label, 90);
      } // if dc:title
      if (strcasecmp (tag, "spine") == 0)
      {
         while (1)
         {
            if ((node = get_tag_or_label (node, tree)) == NULL)
               break;
            if (strcasecmp (tag, "itemref") == 0)
            {
               char idref[100];
               FILE *r;
               mxml_node_t *node, *tree;

               realname (name);
               if (! (r = fopen (name, "r")))
               {
                  endwin ();
                  playfile (PREFIX"share/daisy-player/error.wav", "1");
                  printf (gettext ("\nCannot read %s\n"), name);
                  fflush (stdout);
                  _exit (1);
               } // if
               tree = mxmlLoadFile (NULL, r, MXML_NO_CALLBACK);
               fclose (r);
               node = mxmlFindElement (tree, tree, NULL, NULL, NULL,
                                       MXML_DESCEND_FIRST);
               strncpy (idref, my_attribute.idref, 90);
               while (1)
               {
                  if ((node = get_tag_or_label (node, tree)) == NULL)
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
            if ((node = get_tag_or_label (node, tree)) == NULL)
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
            if ((node = get_tag_or_label (node, tree)) == NULL)
               break;
            if (strcasecmp (tag, "text") == 0)
            {
               while (1)
               {
                  if ((node = get_tag_or_label (node, tree)) == NULL)
                     break;
                  if (*label);
                  {
                     get_label (flag, indent);
                     daisy[current].screen = current / max_y;
                     daisy[current].y = current - daisy[current].screen * max_y;
                     break;
                  } // if
               } // while
            } // if
            if (strcasecmp (tag, "content") == 0)
            {
               parse_content (my_attribute.src);
               current++;
               break;
            } // if
         } // while
      } // if (strcasecmp (tag, "navPoint") == 0)
      if (strcasecmp (tag, "span") == 0)
      {
         do
         {
            if ((node = get_tag_or_label (node, tree)) == NULL)
               break;
            if (! daisy[current - 1].page_number && isdigit (*label))
            {
               daisy[current - 1].page_number = atoi (label);
               break;
            } // if
         } while (strcasecmp (tag, "/span") != 0);
      } // if (strcasecmp (tag, "span") == 0)
   } // while
} // parse_opf_ncx

void daisy3 (char *daisy_mp)
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
// not yet implemented   qsort (&daisy, total_items, sizeof (daisy[1]), compare_playorder);
   parse_smil ();
} // daisy3
