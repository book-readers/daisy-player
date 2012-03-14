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

#include "daisy-player.h"
#include "entities.h"

#define VERSION "7.1.1"

int discinfo_fp, discinfo = 00, multi = 0, displaying = 0;
int playing, just_this_item, current_playorder = 1;
int current_page_number, total_pages;
double current_begin;
WINDOW *screenwin, *titlewin;
mxml_node_t *node, *tree;
char label[255], bookmark_title[100];
char tag[255], element[1024], search_str[30], tmp_wav[255];
char daisy_version[25];
pid_t player_pid, daisy_player_pid;
double clip_begin, clip_end;
char daisy_title[255], prog_name[255], daisy_language[255];
int current, max_y, max_x, total_items, level, depth, displaying;
double audio_total_length, tempo;
char NCC_HTML[100], discinfo_html[255], ncc_totalTime[100];
char sound_dev[16], daisy_mp[255];
time_t start_time;
daisy_t daisy[2000];
my_attribute_t my_attribute;
struct dirent *dirent;

extern void open_smil_file (char *, char *);
extern void save_rc ();
extern void quit_daisy_player ();
extern void get_attributes (char *, char *);

void playfile (char *filename, char *tempo) 
{
  sox_format_t *in, *out; /* input and output files */
  sox_effects_chain_t *chain;
  sox_effect_t *e;
  char *args[100], str[100], str2[100];

  sox_globals.verbosity = 0;
  sox_init();
  if (! (in = sox_open_read (filename, NULL, NULL, NULL)))
    return;
  while (! (out =
         sox_open_write (sound_dev, &in->signal, NULL, "alsa", NULL, NULL)))
  {
    strncpy (sound_dev, "hw:0", 8);
    save_rc ();
    if (out)
      sox_close (out);
  } // while

  chain = sox_create_effects_chain (&in->encoding, &out->encoding);

  e = sox_create_effect (sox_find_effect ("input"));
  args[0] = (char *) in, sox_effect_options (e, 1, args);
  sox_add_effect (chain, e, &in->signal, &in->signal);

  snprintf (str, 90, "%f", clip_begin);
  snprintf (str2, 90, "%f", clip_end - clip_begin);
/* Don't use the sox trim effect. It works nice, but is far too slow
  e = sox_create_effect (sox_find_effect ("trim"));
  args[0] = str;
  args[1] = str2;
  sox_effect_options (e, 2, args);
  sox_add_effect (chain, e, &in->signal, &in->signal);
*/

  e = sox_create_effect (sox_find_effect ("tempo"));
  args[0] = tempo, sox_effect_options (e, 1, args);
  sox_add_effect (chain, e, &in->signal, &in->signal);

  snprintf (str, 90, "%lf", out->signal.rate);
  e = sox_create_effect (sox_find_effect ("rate"));
  args[0] = str, sox_effect_options (e, 1, args);
  sox_add_effect (chain, e, &in->signal, &in->signal);

  snprintf (str, 90, "%i", out->signal.channels);
  e = sox_create_effect (sox_find_effect ("channels"));
  args[0] = str, sox_effect_options (e, 1, args);
  sox_add_effect (chain, e, &in->signal, &in->signal);

  e = sox_create_effect (sox_find_effect ("output"));
  args[0] = (char *) out, sox_effect_options (e, 1, args);
  sox_add_effect (chain, e, &in->signal, &out->signal);

  sox_flow_effects (chain, NULL, NULL);
  sox_delete_effects_chain (chain);
  sox_close (out);
  sox_close (in);
  sox_quit ();
} // playfile

void realname (char *name_in)
{
   DIR *dir;
   struct dirent *dirent;
   char *name;

   if (! (dir = opendir (daisy_mp)))
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf (gettext ("\nCannot read %s\n"), daisy_mp);
      fflush (stdout);
      _exit (1);
   } // if
   name = name_in;
   while ((dirent = readdir (dir)) != NULL)
   {
      if (strcasecmp (dirent->d_name, name) == 0)
      {
         strncpy (name, dirent->d_name, strlen (name));
         closedir (dir);
         break;
      } // if
   } // while
} // realname

double read_time (char *p)
{
   char *h, *m, *s;

   s = strrchr (p, ':') + 1;
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
   return atoi (h) * 3600 + atoi (m) * 60 + atof (s);
} // read_time

void get_clips ()
{
   char *t;

// fill begin
   t = my_attribute.clip_begin;
   while (! isdigit (*t))
      t++;
   if (strchr (t, 's'))
      *strchr (t, 's') = 0;
   if (! strchr (t, ':'))
      clip_begin = atof (t);
   else
      clip_begin = read_time (t);

// fill end
   t = my_attribute.clip_end;
   while (! isdigit (*t))
      t++;
   if (strchr (t, 's'))
      *strchr (t, 's') = 0;
   if (! strchr (t, ':'))
      clip_end = atof (t);
   else
      clip_end = read_time (t);
} // get_clips

void put_bookmark ()
{
   int w;
   char str[255];

   snprintf (str, 250, "%s/.daisy-player", getenv ("HOME"));
   mkdir (str, 0755);
   snprintf (str, 250, "%s/.daisy-player/%s",
                       getenv ("HOME"), bookmark_title);
   if ((w = creat (str, 0644)) != -1)
   {
      if (playing >= 0)
         dprintf (w, "%d\n", playing);
      else
         dprintf (w, "%d\n", current);
      dprintf (w, "%f\n", clip_begin);
      dprintf (w, "%d\n", level);
      close (w);
   } // if
} // put_bookmark

mxml_node_t *get_tag_or_label (mxml_node_t *node, mxml_node_t *tree)
{
   char name[100], content[100];

   *name = *content = 0;
   *tag = *label = 0;
   *my_attribute.class = 0;
   *my_attribute.clip_begin = 0;
   *my_attribute.clip_end = 0;
   *my_attribute.content = 0;
   *my_attribute.dc_format = 0;
   *my_attribute.dc_title = 0;
   *my_attribute.dtb_depth = 0,
   *my_attribute.dtb_totalPageCount = 0;
   *my_attribute.href = 0;
   *my_attribute.http_equiv = 0;
   *my_attribute.id = 0;
   *my_attribute.media_type = 0;
   *my_attribute.name = 0;
   *my_attribute.ncc_depth = 0;
   *my_attribute.ncc_maxPageNormal = 0,
   *my_attribute.ncc_totalTime = 0;
   *my_attribute.playorder = 0;
   *my_attribute.src = 0;
   *my_attribute.value = 0;

   if (! (node = mxmlWalkNext (node, tree, MXML_DESCEND)))
      return NULL;
   switch (node->type)
   {
      int x;

      case MXML_IGNORE:
      case MXML_INTEGER:
      case MXML_OPAQUE:
      case MXML_REAL:
      case MXML_CUSTOM:
         return NULL;
      case MXML_ELEMENT:
         strncpy (tag, node->value.element.name, 250);
         for (x = 0; x < node->value.element.num_attrs; x++)
         {
            get_attributes (node->value.element.attrs[x].name,
                            node->value.element.attrs[x].value);
            if (strcasecmp (node->value.element.attrs[x].name, "name") == 0)
               strncpy (name, node->value.element.attrs[x].value, 90);
            if (strcasecmp (node->value.element.attrs[x].name,
                            "content") == 0)
               strncpy (content, node->value.element.attrs[x].value, 90);
         } // for
         if (strcasecmp (name, "dc:format") == 0)
            strncpy (daisy_version, content, 20);
         if (strcasecmp (name, "dc:title") == 0)
            strncpy (daisy_title, content, 90);
         if (strcasecmp (name, "dtb:depth") == 0)
            depth = atoi (content);
         if (strcasecmp (name, "dtb:totalPageCount") == 0)
            total_pages = atoi (content);
         if (strcasecmp (name, "dtb:totalTime") == 0)
         {
            strncpy (ncc_totalTime, content, 90);
            *strchr (ncc_totalTime, '.') = 0;
         } // if
         if (strcasecmp (name, "ncc:depth") == 0)
            depth = atoi (content);
         if (strcasecmp (name, "ncc:maxPageNormal") == 0)
            total_pages = atoi (content);
         if (strcasecmp (name, "ncc:totalTime") == 0)
         {
            strncpy (ncc_totalTime, content, 90);
            if (strchr (ncc_totalTime, '.'))
               *strchr (ncc_totalTime, '.') = 0;
         } // if
         break;
      case MXML_TEXT:
         if (*node->value.text.string)
            strncpy (label, mxmlSaveAllocString (node, MXML_NO_CALLBACK), 90);
         break;
   } // switch
   return node;
} // get_tag_or_label

void get_bookmark ()
{
   char str[255];
   double begin;
   FILE *r;

   if (! *bookmark_title)
      return;
   snprintf (str, 250, "%s/.daisy-player/%s",
                       getenv ("HOME"), bookmark_title);
   realname (str);
   if ((r = fopen (str, "r")) == NULL)
      return;
   if (fscanf (r, "%d\n%lf\n%d", &current, &begin, &level) == EOF)
   {
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      _exit (1);
   } // if
   fclose (r);
   if (current <= 0)
      current = 0;
   if (current >= total_items)
      current = 0;           
   open_smil_file (daisy[current].smil_file, daisy[current].anchor);
   while (1)
   {
      if ((node = get_tag_or_label (node, tree)) == NULL)
         return;
      if (strcasecmp (tag, "audio") == 0)
      {
         get_clips ();
         if (clip_begin == begin)
            break;
      } // if
   } // while
   if (level < 1)
      level = 1;
   displaying = playing = current;
   just_this_item = -1;
   skip_left ();
} // get_bookmark

void get_attributes (char *name, char *value)
{
   if (strcasecmp (name, "class") == 0)
      strncpy (my_attribute.class, value, 90);
   if (strcasecmp (name, "clip-begin") == 0)
      strncpy (my_attribute.clip_begin, value, 90);
   if (strcasecmp (name, "clipBegin") == 0)
      strncpy (my_attribute.clip_begin, value, 90);
   if (strcasecmp (name, "clip-end") == 0)
      strncpy (my_attribute.clip_end, value, 90);
   if (strcasecmp (name, "clipEnd") == 0)
      strncpy (my_attribute.clip_end, value, 90);
   if (strcasecmp (name, "href") == 0)
      strncpy (my_attribute.href, value, 90);
   if (strcasecmp (name, "http-equiv") == 0)
      strncpy (my_attribute.http_equiv, value, 90);
   if (strcasecmp (name, "id") == 0)
      strncpy (my_attribute.id, value, 90);
   if (strcasecmp (name, "idref") == 0)
       strncpy (my_attribute.idref, value, 90);
   if (strcasecmp (name, "media-type") == 0)
      strncpy (my_attribute.media_type, value, 90);
   if (strcasecmp (name, "playOrder") == 0)
      strncpy (my_attribute.playorder, value, 90);
   if (strcasecmp (name, "src") == 0)
      strncpy (my_attribute.src, value, 90);
} // get_attributes

void get_page_number (char *src)
{
   char file[100], *anchor = 0;
   mxml_node_t *page_node, *page_tree;

   if (! *src)
      return;
   if (strstr (daisy_version, "2.02"))
   {
      FILE *src_r;
      char *anchor = 0;

      if (strchr (src, '#'))
      {
         anchor = strchr (src, '#') + 1;
         *strchr (src, '#') = 0;
      } // if
      realname (src);
      if (! (src_r = fopen (src, "r")))
      {
         endwin ();
         printf ("get_page_number(): %s: %s\n", src, strerror (errno));
         playfile (PREFIX"share/daisy-player/error.wav", "1");
         fflush (stdout);
         _exit (1);
      } // if
      page_tree = mxmlLoadFile (NULL, src_r, MXML_NO_CALLBACK);
      page_node =
         mxmlFindElement (page_tree, page_tree, NULL, NULL, NULL, MXML_DESCEND_FIRST);
      fclose (src_r);
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
         if (strcasecmp (tag, "span") == 0)
            break;
      } // while
      while (1)
      {
         if ((page_node = get_tag_or_label (page_node, page_tree)) == NULL)
            return;
         if (*label)
            break;
      } // while
      current_page_number = atoi (label);
      return;
   } // if (strstr (daisy_version, "2.02"))

// daisy_version, "3"
   FILE *r;

   strncpy (file, src, 90);
   if (strchr (file, '#'))
   {
      anchor = strchr (file, '#') + 1;
      *strchr (file, '#') = 0;
   } // if
   realname (file);
   r = fopen (file, "r");
   page_tree = mxmlLoadFile (NULL, r, MXML_NO_CALLBACK);
   fclose (r);
   page_node = mxmlFindElement (page_tree, page_tree, NULL, NULL, NULL,
                                MXML_DESCEND_FIRST);
   while (1)
   {
      if ((page_node = get_tag_or_label (page_node, page_tree)) == NULL)
         return;
      if (strcasecmp (my_attribute.id, anchor) == 0)
      if (strcasecmp (tag, "pagenum") == 0)
      {
         while (1)
         {
            if ((page_node = get_tag_or_label (page_node, page_tree)) == NULL)
               return;
            if (*label)
               break;
         } // while
         current_page_number = atoi (label);
         break;
      } // if
      if (strcasecmp (tag, "text") == 0)
         break;
   } // while
} // get_page_number

int get_next_clips ()
{
   while (1)
   {
      if ((node = get_tag_or_label (node, tree)) == NULL)
         return 0;
      if (strcasecmp (my_attribute.id, daisy[playing + 1].anchor) == 0 &&
          playing + 1 != total_items)
         return 0;
      if (strcasecmp (tag, "audio") == 0)
      {
         get_clips ();
         return 1;
      } // if
      if (strcasecmp (tag, "text") == 0)
         get_page_number (my_attribute.src);
   } // while
} // get_next_clips

int compare_playorder (const void *p1, const void *p2)
{
   return (*(int *)p1 - *(int*)p2);
} // compare_playorder

void insert_page_numbers ()
{
   int x;

   for (x = 0; x < total_items; x++)
   {
      // ... jos
   } // for
} // insert_page_numbers

void parse_smil ()
{
   int x;
   FILE *smil_file_r;

   for (x = 0; x < total_items; x++)
   {
      if (*daisy[x].smil_file == 0)
         continue;
      realname (daisy[x].smil_file);
      if (! (smil_file_r = fopen (daisy[x].smil_file, "r")))
      {
         endwin ();
         playfile (PREFIX"share/daisy-player/error.wav", "1");
         printf (gettext ("\nCannot read %s\n"), daisy[x].smil_file);
         fflush (stdout);
         _exit (1);
      } // if
      tree = mxmlLoadFile (NULL, smil_file_r, MXML_NO_CALLBACK);
      fclose (smil_file_r);
      node = mxmlFindElement (tree, tree, NULL, NULL, NULL,
                              MXML_DESCEND_FIRST);

// parse this smil
      while (1)
      {
         if ((node = get_tag_or_label (node, tree)) == NULL)
            break;

// find anchor
         if (strcasecmp (my_attribute.id, daisy[x].anchor) == 0)
            break;
      } // while
      while (1)
      {
         if ((node = get_tag_or_label (node, tree)) == NULL)
            break;

// get clip_begin
         if (strcasecmp (tag, "audio") == 0)
         {
            realname (my_attribute.src);
            strncpy (daisy[x].audio_file, my_attribute.src, 90);
            get_clips ();
            daisy[x].begin = clip_begin;
            daisy[x].end   = clip_end;

// get clip_end
            while (1)
            {
               if ((node = get_tag_or_label (node, tree)) == NULL)
                  break;
               if (*daisy[x + 1].anchor)
                  if (strcasecmp (my_attribute.id, daisy[x + 1].anchor) == 0)
                     break;
               if (strcasecmp (tag, "audio") == 0)
               {
                  get_clips ();
                  daisy[x].end = clip_end;
               } // IF
            } // while
            if (*daisy[x + 1].anchor)
               if (strcasecmp (my_attribute.id, daisy[x + 1].anchor) == 0)
                  break;
         } // if (strcasecmp (tag, "audio") == 0)
      } // while
   } // for
} // parse_smil

void view_screen ()
{
   int i, x, x2;

   mvwaddstr (titlewin, 1, 0, "----------------------------------------");
   waddstr (titlewin, "----------------------------------------");
   mvwprintw (titlewin, 1, 0, gettext ("'h' for help "));
   mvwprintw (titlewin, 1, 28, gettext (" level: %d of %d "), level, depth);
   if (total_pages)
      mvwprintw (titlewin, 1, 15, gettext (" %d pages "), total_pages);
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
if (! strstr (daisy_version, "3"))
// not implemented yet
         mvwprintw (screenwin, daisy[i].y, 74, "%02d:%02d",
                 (int) (daisy[i].end - daisy[i].begin + .5) / 60, \
                 (int) (daisy[i].end - daisy[i].begin + .5) % 60);
      if (i >= total_items - 1)
         break;
   } while (daisy[++i].screen == daisy[current].screen);
   if (just_this_item != -1 && daisy[current].screen == daisy[playing].screen)
      mvwprintw (screenwin, daisy[displaying].y, 0, "J");
   wmove (screenwin, daisy[current].y, daisy[current].x);
   wrefresh (screenwin);
} // view_screen

void remake_structure (int level)
{
   int c, x;

   for (c = 0; c < total_items; c++)
   {
      if (daisy[c + 1].level > level)
      {
         for (x = c + 1; daisy[x].level > level && x < total_items + 1; x++)
            daisy[c].end += daisy[x].end;
      } // if
   } // for
} // remake_structure

void html_entities_to_utf8 (char *s)
{
// convert entities to UTf-8: "&lt;" -> "<"
//                            "&amp;" -> "&"

  int e_flag, x;
  char entity[10], *e, new[255], *n, *orig, *s2;

  orig = s;
  n = new;
  while (*s)
  {
    if (*s == '&')
    {
      e_flag = 0;
      e = entity;
      s2 = s;
      s++;
      while (*s != ';')
      {
        *e++ = *s++;
        if (e - entity == 9)
        {
          s = s2 + 1;
          *n++ = '&';
          e_flag = 1;
          break;
        } // if
      } // while
      if (e_flag)
        continue;
      *e = 0;
      *n = ' ';
      for (x = 0;
           x < sizeof (unicode_entities) / sizeof (UC_entity_info); x++)
      {
        if (strcmp (unicode_entities[x].name, entity) == 0)
        {
          char buf[10];
          int num;

          num = stringprep_unichar_to_utf8 (unicode_entities[x].code, buf);
          strncpy (n, buf, num);
          n += num;
        } // if
      } // for
      if (! *(++s))
        break;
    } // if (*s == '&')
    else
      *n++ = *s++;
  } // while
  *n = 0;
  strncpy (orig, new, 250);
} // html_entities_to_utf8

void get_label (int flag, int indent)
{
   html_entities_to_utf8 (label);
   strncpy (daisy[current].label, label, 250);
   if (strchr (daisy[current].label, '\n'))
      *strchr (daisy[current].label, '\n') = 0;
   daisy[current].label[58 - daisy[current].x] = 0;
   if (flag)
   {
      mvwprintw (titlewin, 1, 0, "----------------------------------------");
      wprintw (titlewin, "----------------------------------------");
      mvwprintw (titlewin, 1, 0, gettext ("Reading %s... "),
         daisy[current].label);
      wrefresh (titlewin);
   } // if
   if (displaying == max_y)
      displaying = 0;
   daisy[current].y = displaying;
   daisy[current].screen = current / max_y;
   if (daisy[current].x == 0)
      daisy[current].x = indent + 3;
} // get_label

void read_daisy_structure (int flag)
{
   int indent = 0;
   FILE *ncc_file_r;

   if (strstr (daisy_version, "3"))
      return;
   realname (NCC_HTML);
   ncc_file_r = fopen (NCC_HTML, "r");
   tree = mxmlLoadFile (NULL, ncc_file_r, MXML_NO_CALLBACK);
   fclose (ncc_file_r);
   node = mxmlFindElement (tree, tree, NULL, NULL, NULL, MXML_DESCEND_FIRST);
   current = displaying = 0;
   while (1)
   {
      if ((node = get_tag_or_label (node, tree)) == NULL)
         break;
      if (strcasecmp (tag, "h1") == 0 ||
          strcasecmp (tag, "h2") == 0 ||
          strcasecmp (tag, "h3") == 0 ||
          strcasecmp (tag, "h4") == 0 ||
          strcasecmp (tag, "h5") == 0 ||
          strcasecmp (tag, "h6") == 0)
      {
         int l;

         daisy[current].page_number = 0;
         l = tag[1] - '0';
         daisy[current].level = l;
         indent = daisy[current].x = (l - 1) * 3 + 1;
         do
         {
            if ((node = get_tag_or_label (node, tree)) == NULL)
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
            if ((node = get_tag_or_label (node, tree)) == NULL)
               break;
         } while (*label == 0);
         get_label (flag, indent);
         current++;
         displaying++;
      } // if (strcasecmp (tag, "h1") == 0 || ...
      if (strcasecmp (tag, "span") == 0)
      {
         while (1)
         {
            if ((node = get_tag_or_label (node, tree)) == NULL)
               break;
            if (isdigit (*label))
            {
               if (daisy[current - 1].page_number > 0)
                  break;                         
               daisy[current - 1].page_number = atoi (label);
               break;
            } // if
         } // while
      } // if (strcasecmp (tag, "span") == 0)
   } // while
   total_items = current;
   if (strstr (daisy_version, "3"))
      qsort (&daisy, total_items, sizeof (daisy[1]), compare_playorder);
   parse_smil ();
} // read_daisy_structure

void player_ended ()
{
   wait (NULL);
} // player_ended

void play_now ()
{
   if (playing == -1)
      return;
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

   snprintf (cmd, 250, "madplay -Q %s -s %f -t %f -o \"%s\"",
                  my_attribute.src, clip_begin,
                 clip_end - clip_begin, tmp_wav);
   if (system (cmd) != 0)
      return;
   snprintf (tempo_str, 10, "%lf", tempo);
   playfile (tmp_wav, tempo_str);
/* When the sox trim effedct is used, use this
   playfile (daisy[playing].audio_file, tempo_str);
*/
   _exit (0);
} // play_now

void open_smil_file (char *smil_file, char *anchor)
{
   FILE *smil_file_r;

   realname (smil_file);
   if (! (smil_file_r = fopen (smil_file, "r")))
   {
      endwin ();
      printf ("open_smil_file(): %s: %s\n", smil_file, strerror (errno));
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      fflush (stdout);
      _exit (1);
   } // if
   tree = mxmlLoadFile (NULL, smil_file_r, MXML_NO_CALLBACK);
   fclose (smil_file_r);
   node = mxmlFindElement (tree, tree, NULL, NULL, NULL, MXML_DESCEND_FIRST);
   start_time = time (NULL);

// skip to anchor
   while (1)
   {
      if ((node = get_tag_or_label (node, tree)) == NULL)
         return;
      if (strcasecmp (my_attribute.id, anchor) == 0)
         break;
   } // while
} // open_smil_file

void pause_resume ()
{
   if (playing != -1)
   {
      playing = -1;
      kill (player_pid, SIGQUIT);
   }
   else
   {
      playing = displaying;
      kill (player_pid, SIGCONT);
      node = mxmlWalkPrev (node, tree, MXML_NO_DESCEND);
   } // if
} // pause_resume

void write_wav (char *infile, char *outfile)
{
  sox_format_t *out = NULL;
  sox_format_t *in;
  sox_sample_t samples[2048];
  size_t number_read;
  char str[255];

  in = sox_open_read (infile, NULL, NULL, NULL);
  snprintf (str, 250, "%s/%s", getenv ("HOME"), outfile);
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
   write_wav (my_attribute.src, str);
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

   if (daisy[playing].screen == daisy[current].screen)
   {
      time_t cur_time, addition;
      int played_time, remain_time;

      wattron (screenwin, A_BOLD);
      if (current_page_number)
         mvwprintw (screenwin, daisy[playing].y, 63,
                    "%3d", current_page_number);
      cur_time = time (NULL);
      addition = (time_t) (clip_begin - daisy[playing].begin);
      played_time = cur_time - start_time + addition;
if (! strstr (daisy_version, "3"))
// not implemented yet
      mvwprintw (screenwin, daisy[playing].y, 68, "%02d:%02d", \
                 played_time / 60, played_time % 60);
      remain_time =  daisy[playing].end - played_time;
if (! strstr (daisy_version, "3"))
// not implemented yet
      mvwprintw (screenwin, daisy[playing].y, 74, "%02d:%02d", \
                 remain_time / 60, remain_time % 60);
      wattroff (screenwin, A_BOLD);
      wmove (screenwin, daisy[current].y, daisy[current].x);
      wrefresh (screenwin);
   } // if
   if (kill (player_pid, 0) == 0)
// if still playing
      return;
   player_pid = -2;
   if (get_next_clips ())
   {
      play_now ();
      start_time = time (NULL);
      return;
   } // if

// go to next item
   if (++playing >= total_items)
   {
      quit_daisy_player ();
      snprintf (str, 250, "%s/.daisy-player/%s",
                getenv ("HOME"), bookmark_title);
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
   double latest = 0, current_clip_begin;

   if (playing < 0)
   {
      beep ();
      return;
   } // if
   if (player_pid > 0)
      while (kill (player_pid, SIGQUIT) != 0);
   *my_attribute.clip_begin = 0;
   current_page_number = daisy[playing].page_number;
   if (clip_begin == daisy[playing].begin)
   {
// go to end of previous item
      current = displaying = --playing;
      if (current < 0)
      {
         beep ();
         current = displaying = playing = 0;
         return;
      } // if
      open_smil_file (daisy[playing].smil_file, daisy[playing].anchor);
      while (1)
      {
// search last clip
         if ((node = get_tag_or_label (node, tree)) == NULL)
         {
            view_screen ();
            play_now ();
            return;
         } // if
         if (*my_attribute.clip_begin)
            get_clips ();
      } // while
   } // if (clip_begin == daisy[playing].begin)

   current_clip_begin = clip_begin;
   open_smil_file (daisy[playing].smil_file, daisy[playing].anchor);
   if (*my_attribute.src)
      get_page_number (my_attribute.src);
   while (1)
   {
      if ((node = get_tag_or_label (node, tree)) == NULL)
         return;
      if (strcasecmp (tag, "text") == 0)
         get_page_number (my_attribute.src);
      if (*my_attribute.clip_begin)
      {
         get_clips ();
         if (clip_begin == current_clip_begin)
         {
            clip_begin = latest;
            play_now ();
            return;
         } // if
         latest = clip_begin;
      } // if
   } // while
} // skip_left

void skip_right ()
{
   if (playing == -1)
   {
      beep ();
      return;
   } // if
   kill (player_pid, SIGQUIT);
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
   read_daisy_structure (0);
   current = c;
   remake_structure (level);
   if (daisy[current].level > level)
      previous_item ();
   view_screen ();
   wmove (screenwin, daisy[current].y, daisy[current].x);
} // change_level

void read_rc ()
{
   FILE *r;
   char line[255], *p;
   struct passwd *pw = getpwuid (geteuid ());

   if (chdir (pw->pw_dir) == -1)
   {
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      _exit (1);
   } // if
   strncpy (sound_dev, "hw:0", 8);
   if ((r = fopen (".daisy-player.rc", "r")) == NULL)
      return;
   while (fgets (line, 250, r))
   {
      if (strchr (line, '#'))
         *strchr (line, '#') = 0;
      if ((p = strstr (line, "sound_dev")))
      {
         p += 8;
         while (*++p != 0)
         {
            if (*p == '=')
            {
               while (! *++p);
               if (*p == 0)
                  break;
               strncpy (sound_dev, p, 15);
               sound_dev[15] = 0;
            } // if
         } // while
      } // if
      if ((p = strstr (line, "speed")))
      {
         p += 4;
         while (*++p != 0)
         {
            if (*p == '=')
            {
               while (! isdigit (*++p))
                  if (*p == 0)
                     return;
               tempo = atof (p);
               break;
            } // if
         } // while
      } // if
   } // while
   fclose (r);
} // read_rc

void save_rc ()
{
   FILE *w;
   struct passwd *pw;
   
   if (! (pw = getpwuid (geteuid ())))
   {
      endwin ();
      printf ("getuid (): %d\n", getuid ());
      printf ("getpwuid ()\n");
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      fflush (stdout);
      _exit (1);
   } // if
   if (chdir (pw->pw_dir) == -1)
   {
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      _exit (1);
   } // if
   if ((w = fopen (".daisy-player.rc", "w")) == NULL)
      return;
   fputs ("# This file contains the name of the desired audio device and  the\n", w);
   fputs ("# desired playing speed.\n", w);
   fputs ("#\n", w);
   fputs ("# WARNING\n", w);
   fputs ("# If you edit this file by hand, be sure there is no daisy-player active\n", w);
   fputs ("# otherwise your changes will be lost.\n", w);
   fputs ("#\n", w);
   fputs ("# On which ALSA-audio device should daisy-player play the DTB?\n", w);
   fprintf (w, "sound_dev=%s\n", sound_dev);
   fputs ("#\n", w);
   fputs ("# At wich speed should the DTB be read?\n", w);
   fputs ("# default: speed=1\n", w);
   fprintf (w, "speed=%lf\n", tempo);
   fclose (w);
} // save_rc

void quit_daisy_player ()
{
   endwin ();
   kill (player_pid, SIGQUIT);
   wait (NULL);
   put_bookmark ();
   save_rc ();
   unlink (tmp_wav);
} // quit_daisy_player

void search (int start, char mode)
{
   int c, found = 0;

   if (mode == '/')
   {
      if (playing != -1)
         kill (player_pid, SIGQUIT);
      mvwaddstr (titlewin, 1, 0, "----------------------------------------");
      waddstr (titlewin, "----------------------------------------");
      mvwprintw (titlewin, 1, 0, gettext ("What do you search?                           "));
      echo ();
      wmove (titlewin, 1, 20);
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
      current = c;
      clip_begin = daisy[current].begin;
      just_this_item = -1;
      view_screen ();
      playing = current;
      if (playing != -1)
         kill (player_pid, SIGCONT);
      open_smil_file (daisy[current].smil_file, daisy[current].anchor);
      previous_item ();
   }
   else
   {
      beep ();
      view_screen ();
   } // if
} // search

void go_to_page_number ()
{
   char filename[100], *anchor = 0;
   char pn[15], href[100], prev_href[100];

   kill (player_pid, SIGQUIT);
   mvwaddstr (titlewin, 1, 0, "----------------------------------------");
   waddstr (titlewin, "----------------------------------------");
   mvwprintw (titlewin, 1, 0, gettext ("Go to page number:        "));
   echo ();
   wmove (titlewin, 1, 19);
   wgetnstr (titlewin, pn, 5);
   noecho ();
   view_screen ();
   if (! *pn || atoi (pn) > total_pages)
   {
      beep ();
      return;
   } // if
   if (strstr (daisy_version, "2.02"))
   {
      FILE *ncc_file_r;

      realname (NCC_HTML);
      ncc_file_r = fopen (NCC_HTML, "r");
      tree = mxmlLoadFile (NULL, ncc_file_r, MXML_NO_CALLBACK);
      fclose (ncc_file_r);
      node = mxmlFindElement (tree, tree, NULL, NULL, NULL,
                              MXML_DESCEND_FIRST);
      *prev_href = *href = 0;
      while (1)
      {
         if ((node = get_tag_or_label (node, tree)) == NULL)
            return;
         if (strcasecmp (tag, "a") == 0)
         {
            strncpy (prev_href, href, 90);
            strncpy (href, my_attribute.href, 90);
            while (1)
            {
               if ((node = get_tag_or_label (node, tree)) == NULL)
                  return;
               if (*label)
                  break;
            } // while
            *strchr (label, '\n') = 0;
            if (strcmp (label, pn) == 0)
               break;
         } // if (strcasecmp (tag, "a") == 0)
      } // while
      if (*prev_href)
      {
         strncpy (filename, prev_href, 90);
         if (strchr (filename, '#'))
         {
            anchor = strchr (filename, '#') + 1;
            *strchr (filename, '#') = 0;
         } // if
      } // if (*href)
      for (current = 0; current <= total_items; current++)
         if (strcasecmp (daisy[current].smil_file, filename) == 0)
            break;
      playing = displaying = current;
      view_screen ();
      current_page_number = atoi (pn);
      wmove (screenwin, daisy[current].y, daisy[current].x);
      just_this_item = -1;
      open_smil_file (daisy[current].smil_file, anchor);
   } // if (strstr (daisy_version, "2.02"))

   if (strstr (daisy_version, "3"))
   {
      FILE *r;
      mxml_node_t *node, *tree;
      int text = 0, content = 0;
      char file[100], *anchor = 0;

      realname (NCC_HTML);
      r = fopen (NCC_HTML, "R");
      tree = mxmlLoadFile (NULL, r, MXML_NO_CALLBACK);
      fclose (r);
      node = mxmlFindElement (tree, tree, NULL, NULL, NULL,
                              MXML_DESCEND_FIRST);
      while (1)
      {
         if ((node = get_tag_or_label (node, tree)) == NULL)
            return;
         if (strcasecmp (tag, "pageTarget") == 0)
            break;
      } // while
      while (1)
      {
         if ((node = get_tag_or_label (node, tree)) == NULL)
            return;
         if (*label && atoi (pn) == atoi (label))
            text = 1;
         if (strcasecmp (tag, "content") == 0)
         {
            strncpy (file, my_attribute.src, 90);
            content = 1;
         } // if
         if (text && content)
            break;
      } // while
      if (strchr (file, '#'))
      {
         anchor = strchr (file, '#') + 1;
         *strchr (file, '#') = 0;
      } // if
      open_smil_file (file, anchor);
   } // if (strstr (daisy_version, "3"))
} // go_to_page_number

void select_next_output_device ()
{
   FILE *r;
   int n, y;
   char list[10][1024], trash[1024];

   wclear (screenwin);
   wprintw (screenwin, "\nSelect an soundcard:\n\n");
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
      *list[n] = 0;
      if (! fgets (list[n], 1000, r))
      {
         playfile (PREFIX"share/daisy-player/error.wav", "1");
         _exit (1);
      } // if
      if (! fgets (trash, 1000, r))
      {
         playfile (PREFIX"share/daisy-player/error.wav", "1");
         _exit (1);
      } // if
      if (! *list[n])
         break;
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
   char str[255];
   int old_screen;

   current = 0;
   just_this_item = playing = -1;
   get_bookmark ();
   remake_structure (level);
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
         if (discinfo)
         {
            snprintf (str, 250, "%s -m %s -d %s", prog_name,
                   dirname (daisy[current].smil_file), sound_dev);
            if (system (str) != 0)
               return;
            view_screen ();
            break;
         } // if
         playing = displaying = current;
         current_begin = 0;
         current_page_number = 0;
         kill (player_pid, SIGQUIT);
         open_smil_file (daisy[current].smil_file, daisy[current].anchor);
         break;
      case '/':
         search (current + 1, '/');
         break;
      case ' ':
         pause_resume ();
         break;
      case 'd':
         store_to_disk ();
         break;
      case 'e':
         if (! discinfo && multi)
         {
            beep ();
            break;
         } // if
         quit_daisy_player ();
         if (system ("udisks --unmount /dev/sr0 > /dev/null") != 0)
            return;
         if (system ("udisks --eject /dev/sr0 > /dev/null") != 0)
            return;
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
         kill (player_pid, SIGQUIT);
         help ();
            kill (player_pid, SIGCONT);
         break;
      case 'j':
         if (just_this_item != -1)
            just_this_item = -1;
         else
            just_this_item = current;
         mvwprintw (screenwin, daisy[displaying].y, 0, " ");
         if (playing == -1)
         {
            just_this_item = displaying = playing = current;
            kill (player_pid, SIGQUIT);
            open_smil_file (daisy[current].smil_file, daisy[current].anchor);
         } // if
         if (just_this_item != -1 &&
             daisy[displaying].screen == daisy[playing].screen)
            mvwprintw (screenwin, daisy[displaying].y, 0, "J");
         else
            mvwprintw (screenwin, daisy[displaying].y, 0, " ");
         wrefresh (screenwin);
         break;
      case 'l':
         change_level ('l');
         break;
      case 'L':
         change_level ('L');
         break;
      case 'n':
         search (current + 1, 'n');
         break;
      case 'N':
         search (current - 1, 'N');
         break;
      case 'o':
         if (playing != -1)
            kill (player_pid, SIGQUIT);
         select_next_output_device ();
         if (playing != -1)
            kill (player_pid, SIGCONT);
         break;
      case 'p':
         put_bookmark();
         break;
      case 'q':
         quit_daisy_player ();
         _exit (0);
      case 's':
         kill (player_pid, SIGQUIT);
         playing = just_this_item = -1;
         view_screen ();
         wmove (screenwin, daisy[current].y, daisy[current].x);
         break;
      case KEY_DOWN:
         next_item ();
         break;
      case KEY_UP:
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
         skip_right ();
         break;
      case KEY_LEFT:
         skip_left ();
         break;
      case KEY_NPAGE:
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
         if (tempo >= 100)
         {
            beep ();
            break;
         } // if
         tempo += 0.1;
         kill (player_pid, SIGQUIT);
         play_now ();
         break;
      case 'D':
         if (tempo <= 0.1)
         {
            beep ();
            break;
         } // if
         tempo -= 0.1;
         kill (player_pid, SIGQUIT);
         play_now ();
         break;
      case KEY_HOME:
         tempo = 1;
         kill (player_pid, SIGQUIT);
         play_now ();
         break;
      default:
         beep ();
         break;
      } // switch

      if (playing != -1)
         play_clip ();

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
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      puts (gettext ("\nCannot read /proc/mounts."));
      fflush (stdout);
      _exit (1);
   } // if
   do
   {
      if (getline (&str, &len, proc) == -1)
         break;
   } while (! strstr (str, "iso9660"));
   fclose (proc);
   if (strstr (str, "iso9660"))
   {
      strncpy (daisy_mp, strchr (str, ' ') + 1, 90);
      *strchr (daisy_mp, ' ') = 0;
      return daisy_mp;
   } // if
   return NULL;
} // get_mount_point

int main (int argc, char *argv[])
{
   int opt;
   char str[255];
   FILE *ncc_file_r;

   fclose (stderr); // discard SoX messages
   strncpy (prog_name, basename (argv[0]), 90);
   daisy_player_pid = getpid ();
   tempo = 1;
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
      case 'm':
         multi = 1;
         break;
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
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      endwin ();
      printf (gettext ("\nDaisy-player needs the \"madplay\" programme.\n"));
      printf (gettext ("Please install it and try again.\n"));
      fflush (stdout);
      _exit (1);
   } // if
   fflush (stdout);
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
         snprintf (daisy_mp, 250, "%s", argv[optind]);
      else
         snprintf (daisy_mp, 250, "%s/%s", getenv ("PWD"), argv[optind]);
      if (stat (daisy_mp, &st_buf) == -1)
      {
         printf ("stat: %s: %s\n", strerror (errno), daisy_mp);
         playfile (PREFIX"share/daisy-player/error.wav", "1");
         fflush (stdout);
         _exit (1);
      } // if
   } // if
   initscr ();
   titlewin = newwin (2, 80,  0, 0);
   screenwin = newwin (23, 80, 2, 0);
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
   strncpy (NCC_HTML, "ncc.html", 10);
   realname (NCC_HTML);
   if ((ncc_file_r = fopen (NCC_HTML, "r")))
   {
      mxmlDelete (tree);
      tree = mxmlLoadFile (NULL, ncc_file_r, MXML_NO_CALLBACK);
      fclose (ncc_file_r);
      node = mxmlFindElement (tree, tree, NULL, NULL, NULL,
                              MXML_DESCEND_FIRST);
      while (1)
      {
         if ((node = get_tag_or_label (node, tree)) == NULL)
            break;
         if (*daisy_version)
         {
            strncpy (daisy_version, my_attribute.dc_format, 20);
            break;
         } // if
         if (strcasecmp (tag, "title") == 0)
         {
            if ((node = get_tag_or_label (node, tree)) == NULL)
               break;
            if (*label)
            {
               int x;

               for (x = strlen (label) - 1; x >= 0; x--)
                  if (isspace (label[x]))
                     label[x] = 0;
                  else
                     break;
               strncpy (bookmark_title, label, 90);
               break;
            } // if
         } // if
      } // while
   }
   else
   {
      daisy3 (daisy_mp);
      strncpy (daisy_version, "3", 2);
   } // if
   read_daisy_structure (1);
   wattron (titlewin, A_BOLD);
   snprintf (str, 250, gettext
             ("Daisy-player - Version %s - (C)2012 J. Lemmens"), VERSION);
   mvwprintw (titlewin, 0, 0, str);
   wrefresh (titlewin);

   snprintf (tmp_wav, 200, "/tmp/daisy_player.XXXXXX");
   if (! mkstemp (tmp_wav))
   {
      playfile (PREFIX"share/daisy-player/error.wav", "1");
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
