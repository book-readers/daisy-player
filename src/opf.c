/* daisy3.c - functions to insert daisy3 info into a struct. (opf)
 *
 * Copyright (C) 2019 J. Lemmens
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

void get_label_opf (misc_t *misc, my_attribute_t *my_attribute,
                    daisy_t *daisy, int i)
{
   htmlDocPtr doc;
   xmlTextReaderPtr xml;
   int found_id, found_h1;

   if (! (doc = htmlParseFile (daisy[i].xml_file, "UTF-8")))
   {
      misc->opf_failed = 1;
      return;
   } // if
   if (! (xml = xmlReaderWalker (doc)))
   {
      misc->opf_failed = 1;
      return;
   } // if

   daisy[i].level = 1;
   found_id = found_h1 = 0;
   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, xml))
         break;

// some EPUB use this
      if (strcasecmp (misc->tag, "i") == 0)
      {
         if (misc->verbose)
         {
            printf ("\n\r%d ", i + 1);
            fflush (stdout);
         } // if
         i--;
      } // if

// or
      if (strcasecmp (misc->tag, "title") == 0 ||
          strcasecmp (misc->tag, "h1") == 0 ||
          strcasecmp (misc->tag, "h2") == 0 ||
          strcasecmp (misc->tag, "h3") == 0 ||
          strcasecmp (misc->tag, "h4") == 0 ||
          strcasecmp (misc->tag, "h5") == 0 ||
          strcasecmp (misc->tag, "h6") == 0)
      {
         found_h1 = 1;
         if (strcasecmp (misc->tag, "title") == 0)
            daisy[i].level = 1;
         else
            daisy[i].level = misc->tag[1] - '0';
         if (daisy[i].level > misc->depth)
            misc->depth = daisy[i].level;
         daisy[i].x = daisy[i].level * 3 - 1;
         if (xmlTextReaderIsEmptyElement (xml))
         {
            sprintf (daisy[i].label, "%d", i + 1);
            if (misc->verbose)
            {
               printf ("\n\r%d %s ", i + 1, daisy[i].label);
               fflush (stdout);
            } // if
         } // if IsEmpty
         daisy[i].x = daisy[i].level * 3 - 1;
         daisy[i].screen = i / misc->max_y;
         daisy[i].y = i - (daisy[i].screen * misc->max_y);
      } // if (strcasecmp (misc->tag, "h1") == 0 ||

// others use this
      if (strcasecmp (my_attribute->class, "h1") == 0 ||
          strcasecmp (my_attribute->class, "h2") == 0 ||
          strcasecmp (my_attribute->class, "h3") == 0 ||
          strcasecmp (my_attribute->class, "h4") == 0 ||
          strcasecmp (my_attribute->class, "h5") == 0 ||
          strcasecmp (my_attribute->class, "h6") == 0)
      {
         break;
      } // if (strcasecmp (my_attribute->class, "h1") == 0 ||

      if (*my_attribute->id &&
          strcasecmp (my_attribute->id, daisy[i].xml_anchor) == 0)
      {
         found_id = 1;
      } // if id

      if (*misc->label)
      {
         strncpy (daisy[i].label, misc->label, MAX_STR - 1);
         if (found_id && found_h1)
         {
            xmlTextReaderClose (xml);
            xmlFreeDoc (doc);
            return;
         } // if
      } // if
   } // while
   xmlTextReaderClose (xml);
   xmlFreeDoc (doc);
} // get_label_opf

void parse_smil_opf (misc_t *misc, my_attribute_t *my_attribute,
                     daisy_t *daisy, int i)
{
   htmlDocPtr (doc);
   xmlTextReaderPtr smil;

   if (! (doc = htmlParseFile (daisy[i].smil_file, "UTF-8")))
   {
      misc->opf_failed = 1;
      return;
   } // if
   if (! (smil = xmlReaderWalker (doc)))
   {
      misc->opf_failed = 1;
      return;
   } // if

   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, smil))
         break;
      if (*my_attribute->id)
      {
         strcpy (daisy[i].first_id, my_attribute->id);
      } // if
      if (strcasecmp (misc->tag, "text") == 0)
      {
         daisy[i].xml_anchor = strdup ("");
         if (strchr (my_attribute->src, '#'))
         {
            daisy[i].xml_anchor = strdup
                            (strchr (my_attribute->src, '#') + 1);
            *strchr (my_attribute->src, '#') = 0;
         } // if
         daisy[i].xml_file = realloc
               (daisy[misc->current].xml_file, strlen
               (misc->daisy_mp) + strlen (my_attribute->src) + 5);
         get_realpath_name (misc->daisy_mp,
              convert_URL_name (misc, my_attribute->src),
              daisy[i].xml_file);
         get_label_opf (misc, my_attribute, daisy, i);
         return;
      } // if
   } // while
   xmlTextReaderClose (smil);
} // parse_smil_opf

void fill_smil_anchor_opf (misc_t *misc, my_attribute_t *my_attribute,
                          daisy_t *daisy)
{
// first of all fill daisy struct smil_file and anchor
   misc->depth = misc->current = 0;
   if (misc->verbose)
      printf ("\r\n");
   misc->total_items = misc->current;

   if (*daisy[0].smil_file == 0)
   {
// if there is still no smil_file
      htmlDocPtr doc;
      xmlTextReaderPtr opf;

      if (! (doc = htmlParseFile (misc->opf_name, "UTF-8")))
      {
         misc->opf_failed = 1;
         return;
      } // if
      if ((opf = xmlReaderWalker (doc)) == NULL)
      {
         misc->opf_failed = 1;
         return;
      } // if

      if (misc->verbose)
      {
         printf ("\n\rFilling SMIL");
         fflush (stdout);
      } // if

      misc->current = 0;
      while (1)
      {
         if (! get_tag_or_label (misc, my_attribute, opf))
            break;
         if (strcasestr (my_attribute->media_type, "smil"))
         {
            free (daisy[misc->current].xml_file);
            daisy[misc->current].xml_file = strdup ("");
            free (daisy[misc->current].xml_anchor);
            daisy[misc->current].xml_anchor = strdup   ("");
            free (daisy[misc->current].smil_file);
            daisy[misc->current].smil_file = strdup ("");
            free (daisy[misc->current].smil_anchor);
            daisy[misc->current].smil_anchor = strdup   ("");
            daisy[misc->current].x = 0;
            *daisy[misc->current].label = 0;
            daisy[misc->current].page_number = 0;
            daisy[misc->current].smil_file = malloc (strlen
                         (misc->daisy_mp) + strlen (my_attribute->href) + 5);
            get_realpath_name (misc->daisy_mp,
                      convert_URL_name (misc, my_attribute->href),
                      daisy[misc->current].smil_file);

            parse_smil_opf (misc, my_attribute, daisy, misc->current);
            misc->current++;
         } // if
      } // while
      xmlTextReaderClose (opf);
      xmlFreeDoc (doc);
   } // if still no smil
} // fill_smil_anchor_opf

void parse_opf (misc_t *misc, my_attribute_t *my_attribute, daisy_t *daisy)
{
   htmlDocPtr doc;
   xmlTextReaderPtr opf;

   if (misc->use_OPF == 0 && misc->items_in_opf == 0)
   {
      misc->opf_failed = 1;
      return;
   } // if

   misc->total_items = misc->items_in_opf;
   if (misc->total_items == 0)
      misc->total_items = 1;
   if (! (doc = htmlParseFile (misc->opf_name, "UTF-8")))
   {
      misc->opf_failed = 1;
      return;
   } // if
   if ((opf = xmlReaderWalker (doc)) == NULL)
   {
      misc->opf_failed = 1;
      return;
   } // if

   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, opf))
         break;
      if (misc->use_OPF == 0 &&
          strcasestr (my_attribute->media_type, "ncx") &&
          misc->items_in_ncx > 1)
      {
// first look if there is a reference to "application/x-dtbncx+xml"
         xmlTextReaderClose (opf);
         xmlFreeDoc (doc);
         parse_ncx (misc, my_attribute, daisy);
         return;
      } // if
   } // while
   xmlTextReaderClose (opf);

   opf = xmlReaderWalker (doc);
   while (1)
   {
      if (! (get_tag_or_label (misc, my_attribute, opf)))
         break;
      if (strcasestr (misc->tag, "title"))
      {
         int found;

         found = 0;
         while (1)
         {
            if (! get_tag_or_label (misc, my_attribute, opf))
               break;
            if (*misc->label)
            {
               strcpy (misc->daisy_title, misc->label);
               found = 1;
               break;
            } // if
         } // while
         if (found)
            break;
      } // if
   } // while
   xmlTextReaderClose (opf);
   xmlFreeDoc (doc);

   fill_smil_anchor_opf (misc, my_attribute, daisy);
   misc->items_in_opf = misc->current;
   misc->total_items = misc->items_in_opf;
} // parse_opf
