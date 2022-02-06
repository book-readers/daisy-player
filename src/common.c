/* common.c - common functions used by daisy-player and eBook-speaker.
 *
 * Copyright (C)2017 J. Lemmens
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

void set_volume (misc_t *misc)
{
   char dev[5];
   snd_mixer_t *handle;
   snd_mixer_selem_id_t *sid;
   snd_mixer_elem_t *elem;

   if (snd_mixer_open (&handle, 0) != 0)
      failure (misc, "snd_mixer_open", errno);
   snprintf (dev, 3, "hw:%s", misc->sound_dev);
   snd_mixer_attach (handle, dev);
   snd_mixer_selem_register (handle, NULL, NULL);
   snd_mixer_load (handle);
   snd_mixer_selem_id_alloca (&sid);
   snd_mixer_selem_id_set_index (sid, 0);
   snd_mixer_selem_id_set_name (sid, "Master");
   if ((elem = snd_mixer_find_selem (handle, sid)) == NULL)
      failure (misc, "No ALSA device found", errno);
   snd_mixer_selem_set_playback_volume_all (elem, misc->volume);
   snd_mixer_close (handle);
} // set_volume

char *get_dir_content (misc_t *misc, char *dir_name, char *search_str)
{
   char *found;
   DIR *dir;
   struct dirent *entry;

   if (strcasestr (dir_name, search_str) && *search_str)
      return dir_name;
   if (! (dir = opendir (dir_name)))
      failure (misc, dir_name, errno);
   if (! (entry = readdir (dir)))
      failure (misc, "readdir ()", errno);
   do
   {
      if (strcmp (entry->d_name, ".") == 0 ||
          strcmp (entry->d_name, "..") == 0)
         continue;
      if (strcasestr (entry->d_name, search_str) && *search_str)
      {
         found = malloc (strlen (dir_name) + strlen (entry->d_name) + 5);
         sprintf (found, "%s/%s\n", dir_name, entry->d_name);
         found[strlen (found) - 1] = 0;
         closedir (dir);
         return found;
      } // if
      if (entry->d_type == DT_DIR)
      {
         char *path;

         if (strcmp (entry->d_name, ".") == 0 ||
             strcmp (entry->d_name, "..") == 0)
            continue;
         path = malloc (strlen (dir_name) + strlen (entry->d_name) + 10);
         sprintf (path, "%s/%s", dir_name, entry->d_name);
         found = malloc (MAX_STR);
         strcpy (found, get_dir_content (misc, path, search_str));
         if (strcasestr (found, search_str) && *search_str)
            return found;
         free (path);
      } // if
   } while ((entry = readdir (dir)));
   return "";
} // get_dir_content

char *get_real_name (misc_t *misc, char *dir, char *name)
{
   char *found;
   DIR *DIR;
   struct dirent *entry;

   if (! (DIR = opendir (dir)))
      failure (misc, name, errno);
   if (! (entry = readdir (DIR)))
      failure (misc, "readdir ()", errno);
   found = strdup ("");
   do
   {
      if (strcasecmp (entry->d_name, name) == 0)
      {
         found = malloc (strlen (dir) + strlen (entry->d_name) + 5);
         sprintf (found, "%s/%s\n", dir, entry->d_name);
         found[strlen (found) - 1] = 0;
         closedir (DIR);
         return found;
      } // if
   } while ((entry = readdir (DIR)));
   endwin ();
   beep ();
   printf ("   \"%s\": %s\n\n", name, strerror (ENOENT));
   printf ("%s\n", gettext ("eBook-speaker cannot handle this file."));
   fflush (stdout);
   remove_tmp_dir (misc);
   _exit (-1);
} // get_real_name

char *real_name (misc_t *misc, char *file)
{
// open file case insensitively
   char *dir, *path;
   int i, j;

   *misc->str = j = 0;
   for (i = 0; i < (int) strlen (file) - 2; i++)
   {
      if (file[i] == '%' && isxdigit (file[i + 1]) && isxdigit (file[i + 2]))
      {
         char hex[10];

         hex[0] = '0';
         hex[1] = 'x';
         hex[2] = file[i + 1];
         hex[3] = file[i + 2];
         hex[4] = 0;
         sscanf (hex, "%x", (unsigned int *) &misc->str[j++]);
         i += 2;
      }
      else
         misc->str[j++] = file[i];
   } // for
   misc->str[j++] = file[i++];
   misc->str[j++] = file[i];
   misc->str[j] = 0;
   file = misc->str;
   dir  = strdup (file);
   dir  = dirname (dir);
   path = get_real_name (misc, dir, basename (file));
   return path;
} // real_name

void failure (misc_t *misc, char *str, int e)
{
   endwin ();
   beep ();
   fprintf (stderr, "\n%s: %s\n", str, strerror (e));
   fflush (stdout);
   remove_tmp_dir (misc);
   _exit (-1);
} // failure

void playfile (misc_t *misc, char *in_file, char *in_type,
               char *out_file, char *out_type, char *tempo)
{
/*
   This function goes wrong with pulseaudio. I can't find the solution.
   For now, the external command sox will be used instead.
*/

   char *cmd;

   fclose (stdin);
   fclose (stdout);
   fclose (stderr);
   if (strcmp (in_type, "cdda") == 0)
   {
      cmd = malloc (strlen (in_type) + strlen (in_file) +
                    strlen (out_type) + strlen (out_file) + 50);
      sprintf (cmd, "sox -t cdda -L \"%s\" -t %s \"%s\" tempo -m %s",
               in_file, out_type, out_file, tempo);
   }
   else
   {
      cmd = malloc (strlen (in_type) + strlen (in_file) +
                    strlen (out_type) + strlen (out_file) + 50);
      sprintf (cmd, "sox -t %s \"%s\" -t %s \"%s\" tempo -s %s",
               in_type, in_file, out_type, out_file, tempo);
   } // if
   switch (system (cmd));
   free (cmd);
   unlink (in_file);
   unlink (misc->tmp_wav);

/*
   sox_format_t *sox_in, *sox_out;
   sox_effects_chain_t *chain;
   sox_effect_t *e;
   char *args[MAX_STR];

   sox_globals.verbosity = 0;
   sox_globals.stdout_in_use_by = NULL;
   sox_init ();
   if ((sox_in = sox_open_read (in_file, NULL, NULL, in_type)) == NULL)
   {
      int e;

      e = errno;
      beep ();
      endwin ();
      printf ("\n%s: %s\n", in_file, strerror (e));
      fflush (stdout);
      remove_tmp_dir (misc);
      kill (0, SIGTERM);
   } // if
   if ((sox_out = sox_open_write (out_file, &sox_in->signal,
       NULL, out_type, NULL, NULL)) == NULL)
   {
      beep ();
      endwin ();
      printf ("\n%s: %s\n", out_file, strerror (EBUSY));
      kill (0, SIGTERM);
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

/ *
   Don't use the sox trim effect. It works nice, but is far too slow.
   Use madplay instead.
   char str2[MAX_STR];
   snprintf (str,  MAX_STR - 1, "%f", misc->clip_begin);
   snprintf (str2, MAX_STR - 1, "%f", misc->clip_end - misc->clip_begin);
   e = sox_create_effect (sox_find_effect ("trim"));
   args[0] = str;
   args[1] = str2;
   sox_effect_options (e, 2, args);
   sox_add_effect (chain, e, &sox_in->signal, &sox_in->signal);
* /

   e = sox_create_effect (sox_find_effect ("tempo"));
#ifdef DAISY_PLAYER
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      args[0] = "-m";
   else
#endif
      args[0] = "-s";
   args[1] = tempo;
   sox_effect_options (e, 2, args);
   sox_add_effect (chain, e, &sox_in->signal, &sox_in->signal);

   e = sox_create_effect (sox_find_effect ("rate"));
   snprintf (misc->str, MAX_STR - 1, "%lf", sox_out->signal.rate);
   args[0] = misc->str, sox_effect_options (e, 1, args);
   sox_add_effect (chain, e, &sox_in->signal, &sox_in->signal);

   snprintf (misc->str, MAX_STR - 1, "%i", sox_out->signal.channels);
   e = sox_create_effect (sox_find_effect ("channels"));
   args[0] = misc->str, sox_effect_options (e, 1, args);
   sox_add_effect (chain, e, &sox_in->signal, &sox_in->signal);

   e = sox_create_effect (sox_find_effect ("output"));
   args[0] = (char *) sox_out, sox_effect_options (e, 1, args);
   sox_add_effect (chain, e, &sox_in->signal, &sox_out->signal);

   sox_flow_effects (chain, NULL, NULL);
   sox_delete_effects_chain (chain);
   sox_close (sox_out);
   sox_close (sox_in);
   unlink (in_file);
   sox_quit ();
   unlink (misc->tmp_wav);
*/
} // playfile    

void player_ended ()
{
   wait (NULL);
} // player_ended

void find_index_names (misc_t *misc)
{
   *misc->ncc_html = 0;
   strncpy (misc->ncc_html,
            get_dir_content (misc, misc->daisy_mp, "ncc.html"),
            MAX_STR - 1);
   *misc->ncx_name = 0;
   strncpy (misc->ncx_name,
            get_dir_content (misc, misc->daisy_mp, ".ncx"),
            MAX_STR - 1);
   *misc->opf_name = 0;
   strncpy (misc->opf_name,
            get_dir_content (misc, misc->daisy_mp, ".opf"),
            MAX_STR - 1);
} // find_index_names

int get_page_number_2 (misc_t *misc, my_attribute_t *my_attribute,
                       daisy_t *daisy, char *attr)
{
// function for daisy 2.02
   if (daisy[misc->playing].page_number == 0)
      return 0;
#ifdef DAISY_PLAYER
   char *src, *anchor;
   xmlTextReaderPtr page;
   htmlDocPtr doc;

   src = malloc (strlen (misc->daisy_mp) + strlen (attr) + 3);
   strcpy (src, misc->daisy_mp);
   strcat (src, "/");
   strcat (src, attr);
   anchor = strdup ("");
   if (strchr (src, '#'))
   {
      anchor = strdup (strchr (src, '#') + 1);
      *strchr (src, '#') = 0;
   } // if
   doc = htmlParseFile (src, "UTF-8");
   if (! (page = xmlReaderWalker (doc)))
   {
      int e;
      char str[MAX_STR];

      e = errno;
      snprintf (str, MAX_STR, gettext ("Cannot read %s"), src);
      failure (misc, str, e);
   } // if
   if (*anchor)
   {
      do
      {
         if (! get_tag_or_label (misc, my_attribute, page))
         {
            xmlTextReaderClose (page);
            xmlFreeDoc (doc);
            return 0;
         } // if
      } while (strcasecmp (my_attribute->id, anchor) != 0);
   } // if
   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, page))
      {
         xmlTextReaderClose (page);
         xmlFreeDoc (doc);
         return 0;
      } // if
      if (*misc->label)
      {
         xmlTextReaderClose (page);
         xmlFreeDoc (doc);
         misc->current_page_number = atoi (misc->label);
         return 1;
      } // if
   } // while
#endif

#ifdef EBOOK_SPEAKER
   while (1)
   {
      if (*misc->label)
      {
         misc->current_page_number = atoi (misc->label);
         return 1;
      } // if
      if (! get_tag_or_label (misc, my_attribute, misc->reader))
         return 0;
   } // while
   attr = attr; // don't need it in eBook-speaker
#endif
} // get_page_number_2

int get_page_number_3 (misc_t *misc, my_attribute_t *my_attribute)
{
// function for daisy 3
   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, misc->reader))
         return 0;
#ifdef DAISY_PLAYER
      if (strcasecmp (misc->tag, "text") == 0)
      {
         char *file, *anchor;
         xmlTextReaderPtr page;
         htmlDocPtr doc;

         anchor = strdup ("");
         if (strchr (my_attribute->src, '#'))
         {
            anchor = strdup (strchr (my_attribute->src, '#') + 1);
            *strchr (my_attribute->src, '#') = 0;
         } // if
         file = real_name (misc, my_attribute->src);
         doc = htmlParseFile (file, "UTF-8");
         if (! (page = xmlReaderWalker (doc)))
         {
            int e;
            char str[MAX_STR];

            e = errno;
            snprintf (str, MAX_STR, gettext ("Cannot read %s"), file);
            failure (misc, str, e);
         } // if
         if (*anchor)
         {
            do
            {
               if (! get_tag_or_label (misc, my_attribute, page))
               {
                  xmlTextReaderClose (page);
                  xmlFreeDoc (doc);
                  return 0;
               } // if
            } while (strcasecmp (my_attribute->id, anchor) != 0);
         } // if anchor
         while (1)
         {
            if (! get_tag_or_label (misc, my_attribute, page))
            {
               xmlTextReaderClose (page);
               xmlFreeDoc (doc);
               return 0;
            } // if
            if (*misc->label)
            {
               xmlTextReaderClose (page);
               xmlFreeDoc (doc);
               misc->current_page_number = atoi (misc->label);
               return 1;
            } // if
         } // while
      } // if text
#endif

#ifdef EBOOK_SPEAKER
      if (*misc->label)
      {
         misc->current_page_number = atoi (misc->label);
         return 1;
      } // if                                 
      if (! get_tag_or_label (misc, my_attribute, misc->reader))
         return 0;
#endif
   } // while
} // get_page_number_3

void kill_player (misc_t *misc)
{
   while (killpg (misc->player_pid, SIGHUP) == 0);
#ifdef EBOOK_SPEAKER
   unlink (misc->eBook_speaker_txt);
   unlink (misc->tmp_wav);
#endif
#ifdef DAISY_PLAYER
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      while (killpg (misc->cdda_pid, SIGKILL) == 0);
   unlink (misc->tmp_wav);
#endif
} // kill_player

void skip_right (misc_t *misc, daisy_t *daisy)
{
#ifdef DAISY_PLAYER
   if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      return;
#endif
   if (misc->playing == -1)
   {
      beep ();
      return;
   } // if
   misc->current = misc->displaying = misc->playing;
   wmove (misc->screenwin, daisy[misc->current].y, daisy[misc->current].x);
   kill_player (misc);
} // skip_right

#ifdef DAISY_PLAYER
daisy_t *create_daisy_struct (misc_t *misc, my_attribute_t *my_attribute,
                              daisy_t *daisy)
#endif
#ifdef EBOOK_SPEAKER
daisy_t *create_daisy_struct (misc_t *misc, my_attribute_t *my_attribute)
#endif
{
   htmlDocPtr doc;
   xmlTextReaderPtr ptr;

   misc->total_pages = 0;
   *misc->daisy_version = 0;\
   *misc->ncc_html = *misc->ncx_name = *misc->opf_name = 0;
   find_index_names (misc);

// lookfor "ncc.html"
   if (*misc->ncc_html)
   {
      htmlDocPtr doc;
      xmlTextReaderPtr ncc;

      misc->daisy_mp = strdup (misc->ncc_html);
      misc->daisy_mp = dirname (misc->daisy_mp);
      strncpy (misc->daisy_version, "2.02", 4);
      doc = htmlParseFile (misc->ncc_html, "UTF-8");
      ncc = xmlReaderWalker (doc);
      misc->total_items = 0;
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
            misc->total_items++;
         } // if
      } // while
      xmlTextReaderClose (ncc);
      xmlFreeDoc (doc);
      return (daisy_t *) malloc ((misc->total_items + 1) * sizeof (daisy_t));
   } // if ncc.html

// lookfor *.ncx
   if (*misc->ncx_name)
      strncpy (misc->daisy_version, "3", 2);
   if (strlen (misc->ncx_name) < 4)
      *misc->ncx_name = 0;

// lookfor "*.opf""
   if (*misc->opf_name)
      strncpy (misc->daisy_version, "3", 2);
   if (strlen (misc->opf_name) < 4)
      *misc->opf_name = 0;

   if (*misc->ncc_html == 0 && *misc->ncx_name == 0 && *misc->opf_name == 0)
   {
      beep ();
      endwin ();
      printf ("%s\n", gettext (
        "This book has no audio. Play this book with eBook-speaker"));
      _exit (0);
   } // if

// count items in opf
   misc->items_in_opf = 0;
   doc = htmlParseFile (misc->opf_name, "UTF-8");
   ptr = xmlReaderWalker (doc);
   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, ptr))
         break;
      if (strcasecmp (misc->tag, "itemref") == 0)
         misc->items_in_opf++;
   } // while
   xmlTextReaderClose (ptr);
   xmlFreeDoc (doc);

// count items in ncx
   misc->items_in_ncx = 0;
   doc = htmlParseFile (misc->ncx_name, "UTF-8");
   ptr = xmlReaderWalker (doc);
   while (1)
   {
      if (! get_tag_or_label (misc, my_attribute, ptr))
         break;
      if (strcasecmp (misc->tag, "navpoint") == 0)
         misc->items_in_ncx++;
   } // while
   xmlTextReaderClose (ptr);
   xmlFreeDoc (doc);

   if (misc->items_in_ncx == 0 && misc->items_in_opf == 0)
   {
      endwin ();
      printf ("%s\n",
              gettext ("Please try to play this book with daisy-player"));
      beep ();
#ifdef DAISY_PLAYER
      quit_daisy_player (misc, daisy);
#endif
#ifdef EBOOK_SPEAKER
      quit_eBook_speaker (misc);
#endif
      _exit (0);
   } // if

   misc->total_items = misc->items_in_ncx;
   if (misc->items_in_opf > misc->items_in_ncx)
      misc->total_items = misc->items_in_opf;
   switch (chdir (misc->daisy_mp));
#ifdef EBOOK_SPEAKER
   snprintf (misc->eBook_speaker_txt, MAX_STR,
             "%s/eBook-speaker.txt", misc->daisy_mp);
   snprintf (misc->tmp_wav, MAX_STR,
             "%s/eBook-speaker.wav", misc->daisy_mp);
#endif
   if (misc->total_items == 0)
      misc->total_items = 1;
   return (daisy_t *) malloc (misc->total_items * sizeof (daisy_t));
} // create_daisy_struct

void make_tmp_dir (misc_t *misc)
{
   misc->tmp_dir = strdup ("/tmp/" PACKAGE ".XXXXXX");
   if (! mkdtemp (misc->tmp_dir))
   {
      int e;

      e = errno;
      beep ();
      failure (misc, misc->tmp_dir, e);
   } // if      
#ifdef EBOOK_SPEAKER
   switch (chdir (misc->tmp_dir))
   {
   case 01:
      failure (misc, misc->tmp_dir, errno);
   default:
      break;
   } // switch
#endif
} // make_tmp_dir

void remove_tmp_dir (misc_t *misc)
{
   if (strncmp (misc->tmp_dir, "/tmp/", 5) == 0)
   {
// Be sure not to remove wrong files
#ifdef DAISY_PLAYER
      if (misc->cd_type == CDIO_DISC_MODE_CD_DA)
      {
         rmdir (misc->tmp_dir);
         return;
      } // if
#endif

      snprintf (misc->cmd, MAX_CMD - 1, "rm -rf %s", misc->tmp_dir);
      if (system (misc->cmd) != 0)
      {
         int e;

         e = errno;
         snprintf (misc->cmd, MAX_CMD + strlen (strerror (e)),
                   "%s: %s\n", misc->cmd, strerror (e));
         failure (misc, misc->str, e);
      } // if
   } // if
} // remove_tmp_dir               

void clear_tmp_dir (misc_t *misc)
{
   if (strstr (misc->tmp_dir, "/tmp/"))
   {
// Be sure not to remove wrong files
      snprintf (misc->cmd, MAX_CMD - 1, "rm -rf %s/*", misc->tmp_dir);
      switch (system (misc->cmd));
   } // if
} // clear_tmp_dir

void get_attributes (misc_t *misc, my_attribute_t *my_attribute,
                     xmlTextReaderPtr ptr)
{
   char attr[MAX_STR + 1];

   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, BAD_CAST "class"));
   if (strcmp (attr, "(null)"))
      snprintf (my_attribute->class, MAX_STR - 1, "%s", attr);
#ifdef EBOOK_SPEAKER
   if (misc->option_b == 0)
   {
      snprintf (attr, MAX_STR - 1, "%s", (char *)
                xmlTextReaderGetAttribute
                               (ptr, (xmlChar *) "break_on_eol"));
      if (strcmp (attr, "(null)"))
      {
         misc->break_on_EOL = attr[0];
         if (misc->break_on_EOL != 'y' && misc->break_on_EOL != 'n')
            misc->break_on_EOL = 'n';
      } // if
   } // if
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, BAD_CAST "my_class"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->my_class, attr, MAX_STR - 1);
#endif
#ifdef DAISY_PLAYER
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, BAD_CAST "clip-begin"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->clip_begin, attr, MAX_STR - 1);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, BAD_CAST "clipbegin"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->clip_begin, attr, MAX_STR - 1);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, BAD_CAST "clip-end"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->clip_end, attr, MAX_STR - 1);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "clipend"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->clip_end, attr, MAX_STR - 1);
#endif
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "href"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->href, attr, MAX_STR - 1);
   if (xmlTextReaderGetAttribute (ptr, (const xmlChar *) "id") != NULL)
   {
      my_attribute->id = strdup ((char *) xmlTextReaderGetAttribute (ptr,
               (const xmlChar *) "id"));
#ifdef DAISY_PLAYER
      if (strcmp (my_attribute->id, misc->current_id) != 0)
         misc->current_id = strdup (my_attribute->id);
#endif
   } // if
   if (xmlTextReaderGetAttribute (ptr, (const xmlChar *) "idref") != NULL)
   {                  
      my_attribute->idref = strdup ((char *) xmlTextReaderGetAttribute (ptr,
               (const xmlChar *) "idref"));
   } // if
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "item"));
   if (strcmp (attr, "(null)"))
      misc->current = atoi (attr);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "level"));
   if (strcmp (attr, "(null)"))
      misc->level = atoi ((char *) attr);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
          xmlTextReaderGetAttribute (ptr, (const xmlChar *) "media-type"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->media_type, attr, MAX_STR - 1);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "name"));
   if (strcmp (attr, "(null)"))
   {
      char name[MAX_STR], content[MAX_STR];

      *name = 0;
      strncpy (attr, (char *) xmlTextReaderGetAttribute
               (ptr, (const xmlChar *) "name"), MAX_STR - 1);
      if (strcmp (attr, "(null)"))
         strncpy (name, attr, MAX_STR - 1);
      *content = 0;
      snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "content"));
      if (strcmp (attr, "(null)"))
         strncpy (content, attr, MAX_STR - 1);
      if (strcasestr (name, "dc:format"))
         strncpy (misc->daisy_version, content, MAX_STR - 1);
      if (strcasestr (name, "dc:title") && ! *misc->daisy_title)
      {
         strncpy (misc->daisy_title, content, MAX_STR - 1);
         strncpy (misc->bookmark_title, content, MAX_STR - 1);
         if (strchr (misc->bookmark_title, '/'))
            *strchr (misc->bookmark_title, '/') = '-';
      } // if
/* don't use it!!!
      if (strcasestr (name, "dtb:misc->depth"))
         misc->depth = atoi (content);
*/
      if (strcasestr (name, "dtb:totalPageCount"))
         misc->total_pages = atoi (content);
/* don't use it!!!
      if (strcasestr (name, "ncc:misc->depth"))
         misc->depth = atoi (content);
*/
      if (strcasestr (name, "ncc:maxPageNormal"))
         misc->total_pages = atoi (content);
      if (strcasestr (name, "ncc:pageNormal"))
         misc->total_pages = atoi (content);
      if (strcasestr (name, "ncc:page-normal"))
         misc->total_pages = atoi (content);
      if (strcasestr (name, "dtb:totalTime")  ||
          strcasestr (name, "ncc:totalTime"))
      {
         strncpy (misc->ncc_totalTime, content, MAX_STR - 1);
         if (strchr (misc->ncc_totalTime, '.'))
            *strchr (misc->ncc_totalTime, '.') = 0;
      } // if
   } // if
   snprintf (attr, MAX_STR - 1, "%s", (char *)
           xmlTextReaderGetAttribute (ptr, (const xmlChar *) "playorder"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->playorder, attr, MAX_STR - 1);
#ifdef EBOOK_SPEAKER
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "phrase"));
   if (strcmp (attr, "(null)"))
      misc->phrase_nr = atoi ((char *) attr);
#endif
#ifdef DAISY_PLAYER
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "seconds"));
   if (strcmp (attr, "(null)"))
   {
      misc->seconds = atoi (attr);
      if (misc->seconds < 0)
         misc->seconds = 0;
   } // if
#endif
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "smilref"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->smilref, attr, MAX_STR - 1);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
           xmlTextReaderGetAttribute (ptr, (const xmlChar *) "sound_dev"));
   if (strcmp (attr, "(null)"))
      strncpy (misc->sound_dev, attr, MAX_STR - 1);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
        xmlTextReaderGetAttribute (ptr, (const xmlChar *) "ocr_language"));
   if (strcmp (attr, "(null)"))
      strncpy (misc->ocr_language, attr, 5);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "cd_dev"));
   if (strcmp (attr, "(null)"))
      strncpy (misc->cd_dev, attr, MAX_STR - 1);
#ifdef DAISY_PLAYER
   snprintf (attr, MAX_STR - 1, "%s", (char *)
           xmlTextReaderGetAttribute (ptr, (const xmlChar *) "cddb_flag"));
   if (strcmp (attr, "(null)"))
      misc->cddb_flag = (char) attr[0];
#endif      
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "speed"));
   if (strcmp (attr, "(null)"))
   {
      misc->speed = atof (attr);
      if (misc->speed <= 0.1 || misc->speed > 2)
         misc->speed = 1.0;
   } // if
   if (xmlTextReaderGetAttribute (ptr, (const xmlChar *) "src") != NULL)
   {
      my_attribute->src = strdup ((char *) xmlTextReaderGetAttribute (ptr,
               (const xmlChar *) "src"));
   } // if
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "tts"));
   if (strcmp (attr, "(null)"))
      misc->tts_no = atof ((char *) attr);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "toc"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->toc, attr, MAX_STR - 1);
   snprintf (attr, MAX_STR - 1, "%s", (char *)
             xmlTextReaderGetAttribute (ptr, (const xmlChar *) "value"));
   if (strcmp (attr, "(null)"))
      strncpy (my_attribute->value, attr, MAX_STR - 1);
} // get_attributes

int get_tag_or_label (misc_t *misc, my_attribute_t *my_attribute,
                      xmlTextReaderPtr reader)
{
   int type;

   *misc->tag = 0;
   misc->label = strdup ("");
#ifdef EBOOK_SPEAKER
   *my_attribute->my_class = 0;
#endif
   *my_attribute->class = 0;
#ifdef DAISY_PLAYER
   *my_attribute->clip_begin = *my_attribute->clip_end = 0;
#endif
   *my_attribute->href = *my_attribute->media_type =
   *my_attribute->playorder = * my_attribute->smilref =
   *my_attribute->toc = *my_attribute->value = 0;
   my_attribute->id = strdup ("");
   my_attribute->idref = strdup ("");
   my_attribute->src = strdup ("");

   if (! reader)
   {
      return 0;
   } // if
   switch (xmlTextReaderRead (reader))
   {
   case -1:
   {
      failure (misc, "xmlTextReaderRead ()\n"
               "Can't handle this DTB structure!\n"
               "Don't know how to handle it yet, sorry. (-:\n", errno);
      break;
   }
   case 0:
   {
      return 0;
   }
   case 1:
      break;
   } // swwitch
   type = xmlTextReaderNodeType (reader);
   switch (type)
   {
   int n, i;

   case -1:
   {
      int e;
      char str[MAX_STR];

      e = errno;
      snprintf (str, MAX_STR, gettext ("Cannot read type: %d"), type);
      failure (misc, str, e);
      break;
   }
   case XML_READER_TYPE_ELEMENT:
      strncpy (misc->tag, (char *) xmlTextReaderConstName (reader),
               MAX_TAG - 1);
      n = xmlTextReaderAttributeCount (reader);
      for (i = 0; i < n; i++)
         get_attributes (misc, my_attribute, reader);
#ifdef DAISY_PLAYER
      if (strcasecmp (misc->tag, "audio") == 0)
      {
         misc->prev_id = strdup (misc->audio_id);
         misc->audio_id = strdup (misc->current_id);
      } // if
#endif
      return 1;
   case XML_READER_TYPE_END_ELEMENT:
      snprintf (misc->tag, MAX_TAG - 1, "/%s",
                (char *) xmlTextReaderName (reader));
      return 1;
   case XML_READER_TYPE_TEXT:
   case XML_READER_TYPE_CDATA:
   case XML_READER_TYPE_SIGNIFICANT_WHITESPACE:
   {
      int x;
      size_t phrase_len;

      phrase_len = strlen ((char *) xmlTextReaderConstValue (reader));
      x = 0;
      while (1)
      {
         if (isspace (xmlTextReaderValue (reader)[x]))
            x++;
         else
            break;
      } // while
      misc->label = realloc (misc->label, phrase_len + 1);
      snprintf (misc->label, phrase_len - x + 1, "%s", (char *)
               xmlTextReaderValue (reader) + x);
      for (x = strlen (misc->label) - 1; x >= 0 && isspace (misc->label[x]);
           x--)
         misc->label[x] = 0;
      for (x = 0; misc->label[x] > 0; x++)
         if (! isprint (misc->label[x]))
            misc->label[x] = ' ';
      return 1;
   }
   case XML_READER_TYPE_ENTITY_REFERENCE:
      snprintf (misc->tag, MAX_TAG - 1, "/%s",
                (char *) xmlTextReaderName (reader));
      return 1;
   default:
      return 1;
   } // switch
   return 0;
} // get_tag_or_label

void go_to_page_number (misc_t *misc, my_attribute_t *my_attribute,
                          daisy_t *daisy)
{
   char pn[15];

   kill_player (misc);
#ifdef DAISY_PLAYER
   if (misc->cd_type != CDIO_DISC_MODE_CD_DA)
      misc->player_pid = -2;
#endif
   misc->playing = misc->just_this_item = -1;
   view_screen (misc, daisy);
#ifdef EBOOK_SPEAKER
   remove (misc->tmp_wav);
#endif             
   unlink ("old.wav");
   mvwprintw (misc->titlewin, 1, 0,
              "----------------------------------------");
   wprintw (misc->titlewin, "----------------------------------------");
   mvwprintw (misc->titlewin, 1, 0, "%s ", gettext ("Go to page number:"));
   echo ();
   wgetnstr (misc->titlewin, pn, 5);
   noecho ();
   view_screen (misc, daisy);
   if (atoi (pn) == 0 || atoi (pn) > misc->total_pages)
   {
      beep ();
      pause_resume (misc, my_attribute, daisy);
      pause_resume (misc, my_attribute, daisy);
      return;
   } // if

// start searching
   mvwprintw (misc->titlewin, 1, 0,
              "----------------------------------------");
   wprintw (misc->titlewin, "----------------------------------------");
   wattron (misc->titlewin, A_BOLD);
   mvwprintw (misc->titlewin, 1, 0, "%s", gettext ("Please wait..."));
   wattroff (misc->titlewin, A_BOLD);
   wrefresh (misc->titlewin);
   misc->playing = misc->current_page_number = 0;
#ifdef EBOOK_SPEAKER
   misc->phrase_nr = 0;
#endif

#ifdef DAISY_PLAYER
   open_clips_file (misc, my_attribute, daisy[misc->playing].clips_file,
                    daisy[misc->playing].clips_anchor);
#endif
#ifdef EBOOK_SPEAKER
   open_text_file (misc, my_attribute, daisy[misc->playing].xml_file,
                   daisy[misc->playing].anchor);
#endif
   while (1)
   {
      if (misc->current_page_number == atoi (pn))
      {
         misc->displaying = misc->current = misc->playing;
         misc->player_pid = -2;
         view_screen (misc, daisy);
         return;
      } // if (misc->current_page_number == atoi (pn))
      if (misc->current_page_number > atoi (pn))
      {
         misc->current = misc->playing = 0;
         misc->current_page_number = daisy[misc->playing].page_number;
#ifdef DAISY_PLAYER
         open_clips_file (misc, my_attribute, daisy[misc->playing].clips_file,
                          daisy[misc->playing].clips_anchor);
#endif
#ifdef EBOOK_SPEAKER
         open_text_file (misc, my_attribute, daisy[misc->playing].xml_file,
                         daisy[misc->playing].anchor);
#endif
         return;
      } // if
#ifdef DAISY_PLAYER
      get_next_clips (misc, my_attribute, daisy);
#endif
#ifdef EBOOK_SPEAKER
      get_next_phrase (misc, my_attribute, daisy, 0);
#endif
   } // while
} // go_to_page_number

void select_next_output_device (misc_t *misc, my_attribute_t *my_attribute,
                                daisy_t *daisy)
{
   FILE *r;
   int n, y, playing;
   size_t bytes;
   char *list[10], *trash;

   playing= misc->playing;
   pause_resume (misc, my_attribute, daisy);
   wclear (misc->screenwin);
   wprintw (misc->screenwin, "\n%s\n\n", gettext ("Select a soundcard:"));
   if (! (r = fopen ("/proc/asound/cards", "r")))
      failure (misc, gettext ("Cannot read /proc/asound/cards"), errno);
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
      case 13: //
         snprintf (misc->sound_dev, MAX_STR - 1, "%d", y - 3);
         view_screen (misc, daisy);
         nodelay (misc->screenwin, TRUE);
         if (playing > -1)
            pause_resume (misc, my_attribute, daisy);
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
         if (playing > -1)
            pause_resume (misc, my_attribute, daisy);
         return;
      } // switch
   } // for
} // select_next_output_device
