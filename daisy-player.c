/* daisy-player - A parser to play Daisy cd's.
 *  Copyright (C) 2003-2011 J. Lemmens
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

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <ncursesw/curses.h>
#include <fcntl.h>
#include <libgen.h>
#include <time.h>
#include <dirent.h>
#include <pwd.h>
#include <sys/mount.h>
#include <locale.h>
#include <libintl.h>
#include "entities.h"
#include <stringprep.h>
#include <sox.h>
#include <errno.h>

#define VERSION "7.0.4"

WINDOW *screenwin, *titlewin;
int smil_file_fd, discinfo_fp, discinfo = 00, multi = 0, displaying = 0;
int playing, just_this_item;
int bytes_read, current_page_number, total_pages;
char label[255], clip_str_b[15], clip_str_e[15], bookmark_title[100];
char tag[255], element[255], search_str[30], tmp_ncx[255], tmp_wav[255];
char daisy_version[25];
pid_t player_pid, daisy_player_pid;
double clip_begin, clip_end;
char daisy_title[255], prog_name[255];
struct
{
   int x, y, screen;
   double begin, end;
   char smil_file[255], anchor[255], label[255];
   char audio_file[255], class[100];
   int level, page_number;
} daisy[2000];

struct
{
   char a[100],
        class[100],
        clip_begin[100],
        clip_end[100],
        content[100],
        dc_format[100],
        dc_title[100],
        dtb_depth[100],
        dtb_totalPageCount[100],
        href[100],
        id[100],
        media_type[100],
        name[100],
        ncc_depth[100],
        ncc_maxPageNormal[100],
        ncc_totalTime[100],
        playorder[100],
        src[100],  
        value[100];
} attribute;

int current, max_y, max_x, total_items, level, depth;
double audio_total_length, tempo;
char NCC_HTML[255], discinfo_html[255], ncc_totalTime[10];
char sound_dev[16], daisy_mp[255];
time_t start_time;
DIR *dir;
struct dirent *dirent;

extern void play_now ();
extern void kill_player (int sig);
extern void view_screen ();
extern void open_smil_file (char *, char *);
extern int get_next_clips (int);
extern void save_rc ();
extern void previous_item ();
extern void quit_daisy_player ();
extern void get_tag ();
extern void skip_left ();
extern void skip_right ();

void playfile (char *filename, char *tempo)
{
  sox_format_t *in, *out; /* input and output files */
  sox_effects_chain_t *chain;
  sox_effect_t *e;
  char *args[100], str[100], str2[100];

  sox_globals.verbosity = 0;
  sox_init();
  in = sox_open_read (filename, NULL, NULL, NULL);
  while (! (out =
         sox_open_write (sound_dev, &in->signal, NULL, "alsa", NULL, NULL)))
  {
    strncpy (sound_dev, "default", 8);
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

char *realname (char *name)
{
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
      if (strcasecmp (dirent->d_name, name) == 0)
      {
         closedir (dir);
         return dirent->d_name;
      } // if
   } // while
   closedir (dir);
   return name;
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
   t = attribute.clip_begin;
   while (! isdigit (*t))
      t++;
   if (strchr (t, 's'))
      *strchr (t, 's') = 0;
   if (! strchr (t, ':'))
      clip_begin = atof (t);
   else
      clip_begin = read_time (t);

// fill end
   t = attribute.clip_end;
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
      dprintf (w, "%d\n", current);
      if (clip_end == daisy[current - 1].end)
         clip_begin = 0;
      dprintf (w, "%lf\n", clip_begin);
      dprintf (w, "%d\n", level);
      close (w);
   } // if
} // put_bookmark

void get_bookmark ()
{
   char str[255];
   double begin;
   FILE *r;

   snprintf (str, 250, "%s/.daisy-player/%s",
                       getenv ("HOME"), bookmark_title);
   if ((r = fopen (str, "r")) == NULL)
      return;
   fscanf (r, "%d\n%lf\n%d", &current, &begin, &level);
   fclose (r);
   if (begin >= daisy[current].end)
   {
      current++;
      begin = daisy[current].begin;
   } // if
   if (level < 1)
      level = 1;
   displaying = playing = current;
   just_this_item = -1;
   open_smil_file (daisy[current].smil_file, daisy[current].anchor);
   clip_begin = 0;
   while (clip_begin < begin)
   {
      get_next_clips (smil_file_fd);
      skip_right ();
   } // while
   skip_left ();
} // get_bookmark

void html_entities_to_utf8 (char *s)
{
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

void get_attributes (char *p)
{
   char name[255], *value, *begin;
   int break2;

   *attribute.class = 0;
   *attribute.clip_begin = 0;
   *attribute.clip_end = 0;
   *attribute.content = 0;
   *attribute.dc_format = 0;
   *attribute.dc_title = 0;
   *attribute.dtb_depth = 0,
   *attribute.dtb_totalPageCount = 0;
   *attribute.href = 0;
   *attribute.id = 0;
   *attribute.media_type = 0;
   *attribute.name = 0;
   *attribute.ncc_depth = 0;
   *attribute.ncc_maxPageNormal = 0,
   *attribute.ncc_totalTime = 0;
   *attribute.playorder = 0;
   *attribute.src = 0;
   *attribute.value = 0;
   begin = p;

// skip to first attribute
   while (! isspace (*p))
   {
      if (*p == '>' || *p == '?')
         return;
      if (p - begin > 250)
      {
         *p = 0;
         return;
      } // if
      p++;
   } // while
   break2 = 0;
   while (1)
   {
      while (isspace (*++p))
      {
         if (*p == '>' || *p == '?')
         {
            break2 = 1;
            break;
         } // if
         if (p - begin > 250)
         {
            *p = 0;
            break2 = 1;
            break;
         } // if
      } // while
      if (break2)
        break;
      strncpy (name, p, 250);
      p = name;
      while (! isspace (*p) && *p != '=')
      {
         if (*p == '>' || *p == '?')
         {
            break2 = 1;
            break;
         } // if
         if (p - begin > 250)
         {
            *p = 0;
            break2 = 1;
            break;
         } // if
         p++;
      } // while
      if (break2)
         break;
      *p = 0;
      while (*p != '"')
      {
         if (*p == '>' || *p == '?')
         {
            break2 = 1;
            break; 
         } // if
         if (p - begin > 250)
         {
            *p = 0;
            break2 = 1;
            break; // return;
         } // if
         p++;
      } // while
      if (break2)
         break;
      p++;

      value = p;
      p = value;
      while (*p != '"' && *p != '>' && *p != '?')
      {
         if (p - begin > 250)
         {
            *p = 0;
            break2 = 1;
            break;
         } // if
         p++;
      } // while
      if (break2)
         break;
      *p = 0;

      if (strcasecmp (name, "class") == 0)
         strncpy (attribute.class, value, 90);
      if (strcasecmp (name, "clip-begin") == 0)
         strncpy (attribute.clip_begin, value, 90);
      if (strcasecmp (name, "clipbegin") == 0)
         strncpy (attribute.clip_begin, value, 90);
      if (strcasecmp (name, "clip-end") == 0)
         strncpy (attribute.clip_end, value, 90);
      if (strcasecmp (name, "clipend") == 0)
         strncpy (attribute.clip_end, value, 90);
      if (strcasecmp (name, "content") == 0)
         strncpy (attribute.content, value, 90);
      if (strcasecmp (name, "href") == 0)
         strncpy (attribute.href, value, 90);
      if (strcasecmp (name, "id") == 0)
         strncpy (attribute.id, value, 90);
      if (strcasecmp (name, "media-type") == 0)
         strncpy (attribute.media_type, value, 90);
      if (strcasecmp (name, "name") == 0)
      {
         if (strcasecmp (value, "dc:format") == 0)
            strncpy (attribute.dc_format, "prepare for content", 90);
         if (strcasecmp (value, "dc:title") == 0)
            strncpy (attribute.dc_title, "prepare for content", 90);
         if (strcasecmp (value, "dtb:depth") == 0)
            strncpy (attribute.dtb_depth, "prepare for content", 90);
         if (strcasecmp (value, "dtb:totalPageCount") == 0)
            strncpy (attribute.ncc_maxPageNormal, "prepare for content", 90);
         if (strcasecmp (value, "dtb:totalTime") == 0)
            strncpy (attribute.ncc_totalTime, "prepare for content", 90);
         if (strcasecmp (value, "ncc:depth") == 0)
            strncpy (attribute.ncc_depth, "prepare for content", 90);
         if (strcasecmp (value, "ncc:maxPageNormal") == 0)
            strncpy (attribute.ncc_maxPageNormal, "prepare for content", 90);
         if (strcasecmp (value, "ncc:totalTime") == 0)
            strncpy (attribute.ncc_totalTime, "prepare for content", 90);
      } // if
      if (strcasecmp (name, "playorder") == 0)
         strncpy (attribute.playorder, value, 90);
      if (strcasecmp (name, "src") == 0)
         strncpy (attribute.src, value, 90);
      if (strcasecmp (name, "value") == 0)
         strncpy (attribute.value, value, 90);
   } // while
   if (*attribute.dc_format)
      strncpy (attribute.dc_format, attribute.content, 90);
   if (*attribute.dc_title)
      strncpy (attribute.dc_title, attribute.content, 90);
   if (*attribute.dtb_depth)
      depth = atoi (attribute.content);
   if (*attribute.dtb_totalPageCount)
      total_pages = atoi (attribute.content);
   if (*attribute.ncc_depth)
      depth = atoi (attribute.content);
   if (*attribute.ncc_maxPageNormal)
      total_pages = atoi (attribute.content);
   if (*attribute.ncc_totalTime)
   {
      strncpy (attribute.ncc_totalTime, attribute.content, 90);
      if (strchr (attribute.ncc_totalTime, '.'))
         *strchr (attribute.ncc_totalTime, '.') = 0;
   } // if
} // get_attributes

int get_element_tag_or_label (int r)
{
   char *p, h;
   static char read_flag = 0;

   *tag = *label = 0;
   p = element;
   do
   {
      switch (read (r, p, 1))
      {
      case -1:
         endwin ();
         playfile (PREFIX"share/daisy-player/error.wav", "1");
         puts (gettext ("Corrupt daisy structure!"));
         fflush (stdout);
         _exit (1);
      case 0:
         return EOF;
      } // switch
   } while (isspace (*p));
   h = *p;

   if (read_flag)
   {
      *p++ = '<';
      *p = h;
   } // if
   if (*p == '<' || read_flag)
   {
      read_flag = 0;
      *label = 0;
      do
      {
         if (p - element > 250)
         {
            *p = 0;
            strncpy (tag, element + 1, 250);
            get_tag ();
            get_attributes (element);
            return 0;
         } // if
         switch (read (r, ++p, 1))
         {
         case -1:
            endwin ();
            playfile (PREFIX"share/daisy-player/error.wav", "1");
            printf ("get_element_tag_or_label: %s\n", p);
            fflush (stdout);
            _exit (1);
         case 0:
            *++p = 0;
            strncpy (tag, element + 1, 250);
            get_tag ();
            get_attributes (element);
            return EOF;
         } // switch
      } while (*p != '>');
      *++p = 0;
      strncpy (tag, element + 1, 250);
      get_tag ();
      get_attributes (element);
      return 0;
   } // if
   *label = *p;
   *element = 0;
   p = label;
   do
   {
      if (p - label > 250)
      {
         *p = 0;
         return 0;
      } // if
      switch (read (r, ++p, 1))
      {
      case -1:
         endwin ();
         playfile (PREFIX"share/daisy-player/error.wav", "1");
         puts (gettext ("Maybe a read-error occurred!"));
         fflush (stdout);
         _exit (1);
      case 0:
         *p = 0;
         return EOF;
      } // switch
      if (*p == '\n')
         p--;
   } while (*p != '<');
   read_flag = 1;
   *p = 0;
   return 0;
} // get_element_tag_or_label

void get_tag ()
{
   char *p;
   p = tag;
   while (*p != ' ' && *p != '>' && p - tag <= 250)
     p++;
   *p = 0;
} // get_tag

void get_page_number ()
{
   int fd;
   char file[100], *anchor;

   if (strstr (daisy_version, "2.02"))
   {
      if (! strcasestr (element, attribute.src))
         return;
      strncpy (file, attribute.src, 90);
      if (strchr (file, '#'))
      {
         anchor = strchr (file, '#') + 1;
         *strchr (file, '#') = 0;
      } // if
      if ((fd = open (realname (file), O_RDONLY)) == -1)
      {
         endwin ();
         playfile (PREFIX"share/daisy-player/error.wav", "1");
         printf ("get_page_number(): %s\n", file);
         fflush (stdout);
         _exit (1);
      } // if
      while (1)
      {
         if (get_element_tag_or_label (fd) == EOF)
         {
            close (fd);
            return;
         } // if
         if (strcasecmp (attribute.id, anchor) == 0)
            break;
      } // while
      while (1)
      {
         if (get_element_tag_or_label (fd) == EOF)
         {
            close (fd);
            return;
         } // if
         if (strcasecmp (tag, "span") == 0)
            break;
         if (tolower (tag[0]) == 'h' && isdigit (tag[1]))
         {
            close (fd);
            return;
         } // if
      } // while
      while (1)
      {
         if (get_element_tag_or_label (fd) == EOF)
         {
            close (fd);
            return;
         } // if
         if (isdigit (*label))
         {
            current_page_number = atoi (label);
            close (fd);
            return;
         } // if
      } // while
      close (fd);
      return;
   } // if
   if (strcmp (daisy_version, "3") == 0)
   {
return; // jos
      fd = open (NCC_HTML, O_RDONLY);
      do
      {
         if (get_element_tag_or_label (fd) == EOF)
            break;
      } while (atoi (attribute.playorder) != current);
      do
      {
         if (get_element_tag_or_label (fd) == EOF)
            break;
      } while (! *label);
      current_page_number = atoi (label);
      close (fd);
      return;
   } // if
} // get_page_number

int get_next_clips (int fd)
{
   if (fd == EOF)
      return 0;
   while (1)
   {
      if (get_element_tag_or_label (fd) == EOF)
      {
         close (fd);
         return 0;
      } // if
      if (strcasecmp (tag, "text") == 0)
         get_page_number ();
      if (strcasecmp (tag, "audio") == 0)
      {
         if (strcasecmp (daisy[playing].audio_file, attribute.src) != 0)
         {
            close (fd);
            return 0;
         } // if
         get_clips ();
         if (clip_begin < daisy[playing].begin)
            continue;
         if (clip_end <= daisy[playing].end)
            return 1;
         close (fd);
         return 0;
      } // if
   } // while
} // get_next_clips

void parse_smil ()
{
   int r, x, got_begin;

   for (x = 0; x < total_items; x++)
   {
      r = open (daisy[x].smil_file, O_RDONLY);
      do
      {
         get_element_tag_or_label (r);
      } while (strcasecmp (attribute.id, daisy[x].anchor) != 0);
      if (strcasecmp (attribute.class, "pagenum") == 0)
      {
         do
         {
            get_element_tag_or_label (r);
// discard pagenumbers
//            if (strcasecmp (tag, "audio") == 0)
//               strncpy (daisy[x].audio_file, realname (attribute.src), 250);
         } while (strcasecmp (tag, "/par") != 0);
      } // if
      got_begin = 0;
      while (1)
      {
         if (get_element_tag_or_label (r) == EOF)
            break;
         if (strcasecmp (tag, "audio") == 0)
         {
            strncpy (daisy[x].audio_file, realname (attribute.src), 250);
            get_clips ();
            if (! got_begin)
            {
               got_begin = 1;
               daisy[x].begin = clip_begin;
            } // if
            daisy[x].end = clip_end;
         } // if
         if (*daisy[x + 1].anchor)
            if (strcasecmp (attribute.id, daisy[x + 1].anchor) == 0)
               break;
      } // while

      if (daisy[x].end < daisy[x].begin)
      {
         endwin ();
         playfile (PREFIX"share/daisy-player/error.wav", "1");
         printf (gettext ("clip_end (%lf) is smaller than clip_begin (%lf). \n\
This is not possible!\n\
Daisy-player can't handle this DTB.\n"),
daisy[x].end, daisy[x].begin);
         fflush (stdout);
         _exit (1);
      } // if
      close (r);
   } // for
} // parse_smil

void view_screen ()
{
   int i, x, x2;

   mvwaddstr (titlewin, 1, 0, "----------------------------------------");
   waddstr (titlewin, "----------------------------------------");
   mvwprintw (titlewin, 1, 0, gettext ("'h' for help "));
   if (! discinfo)
   {
      mvwprintw (titlewin, 1, 28,
                 gettext (" level: %d of %d "), level, depth);
      if (total_pages)
         mvwprintw (titlewin, 1, 15, gettext (" %d pages "), total_pages);
      mvwprintw (titlewin, 1, 47,
                 gettext (" total length: %s "), ncc_totalTime);
      mvwprintw (titlewin, 1, 74, " %d/%d ", \
              daisy[current].screen + 1, daisy[total_items - 1].screen + 1);
   } // if
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
      if (! discinfo)
      {
         if (daisy[i].page_number)
            mvwprintw (screenwin, daisy[i].y, 62,
                       "(%3d)", daisy[i].page_number);
         if (daisy[i].level <= level)
            mvwprintw (screenwin, daisy[i].y, 74, "%02d:%02d",
                 (int) (daisy[i].end - daisy[i].begin + .5) / 60, \
                 (int) (daisy[i].end - daisy[i].begin + .5) % 60);
      } // if
      if (i >= total_items - 1)
         break;
   } while (daisy[++i].screen == daisy[current].screen);
   if (just_this_item != -1 &&
       daisy[current].screen == daisy[playing].screen)
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

void get_label (int flag, int indent)
{
   html_entities_to_utf8 (label);
   strncpy (daisy[current].label, label, 250);
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

   if (strcasecmp (daisy[current].class, "pagenum") == 0)
      daisy[current].x = 0;
   else
      if (daisy[current].x == 0)
         daisy[current].x = indent + 3;
} // get_label

void read_daisy_structure (char *xml_file, int flag)
{
   int indent = 0, r;

   if ((r = open (xml_file, O_RDONLY)) == -1)
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf (gettext ("Corrupt daisy structure %s\n"), xml_file);
      fflush (stdout);
      _exit (1);
   } // if
   current = displaying = 0;
   do
   {
      if (get_element_tag_or_label (r) == EOF)
         break;
      if (*attribute.dc_title)
         strncpy (daisy_title, attribute.dc_title, 90);
      if (*attribute.ncc_totalTime)
         strncpy (ncc_totalTime, attribute.ncc_totalTime, 8);
      if (strcasecmp (tag, "navPoint") == 0)
      {
         int l, got_label = 0;

         daisy[current].page_number = 0;
         l = attribute.class[1] - '0';
         daisy[current].level = l;
         indent = daisy[current].x = (l - 1) * 3 + 1;
         strncpy (daisy[current].class, attribute.class, 90);
         do
         {
            get_element_tag_or_label (r);
            if (*label)
            {
               get_label (flag, indent);
               got_label = 1;
            } // if
            if (strcasecmp (tag, "content") == 0)
            {
               strncpy (daisy[current].smil_file, attribute.src, 250);
               if (strchr (daisy[current].smil_file, '#'))
               {
                  strncpy (daisy[current].anchor,
                           strchr (daisy[current].smil_file, '#') + 1, 250);
                  *strchr (daisy[current].smil_file, '#') = 0;
               } // if
            } // if
         } while (strcasecmp (tag, "content") != 0);
         if (got_label)
         {
            current++;
            displaying++;
         } // if
      } // if (strcasecmp (tag, "navPoint") == 0)
      if (strcasecmp (tag, "pageTarget") == 0)
      {
         int got_label = 0;

         daisy[current].page_number = 0;
         strncpy (daisy[current].class, attribute.class, 90);
         do
         {
            get_element_tag_or_label (r);
            if (*label)
            {
               get_label (flag, indent);
               got_label = 1;
               daisy[current].page_number = atoi (daisy[current].label);
            } // if
            if (strcasecmp (tag, "content") == 0)
            {
               strncpy (daisy[current].smil_file, attribute.src, 250);
               if (strchr (daisy[current].smil_file, '#'))
               {
                  strncpy (daisy[current].anchor,
                           strchr (daisy[current].smil_file, '#') + 1, 250);
                  *strchr (daisy[current].smil_file, '#') = 0;
               } // if
            } // if
         } while (strcasecmp (tag, "content") != 0);
         if (got_label)
         {
            current++;
            displaying++;
         } // if
      } // if (strcasecmp (tag, "pageTarget") == 0)
      if (strcasecmp (tag, "docTitle") == 0)
      {
         do
         {
            get_element_tag_or_label (r);
         } while (strcasecmp (tag, "text") != 0);
         do
         {
            get_element_tag_or_label (r);
         } while (*label == 0);
         strncpy (daisy_title, label, 250);
      } // if (strcasecmp (tag, "docTitle") == 0)
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
            get_element_tag_or_label (r);
         } while (strcasecmp (tag, "a") != 0);
         strncpy (daisy[current].smil_file, attribute.href, 250);
         if (strchr (daisy[current].smil_file, '#'))
         {
            strncpy (daisy[current].anchor,
                     strchr (daisy[current].smil_file, '#') + 1, 250);
            *strchr (daisy[current].smil_file, '#') = 0;
         } // if
         do
         {
            get_element_tag_or_label (r);
         } while (*label == 0);
         get_label (flag, indent);
         current++;
         displaying++;
      } // if (strcasecmp (tag, "h1") == 0 || ...
      if (strcasecmp (tag, "span") == 0)
      {
         do
         {
            get_element_tag_or_label (r);
            if (! daisy[current - 1].page_number && isdigit (*label))
            {
               daisy[current - 1].page_number = atoi (label);
               break;
            } // if
         } while (strcasecmp (tag, "/span") != 0);
      } // if (strcasecmp (tag, "span") == 0)
   } while (strcasecmp (tag, "/html") != 0 && strcasecmp (tag, "/ncx") != 0);
   close (r);
   total_items = current;
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
   if (! *daisy[playing].audio_file)
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf ("No audio_file\n");
      _exit (1);
   } // if
   switch (player_pid = fork ())
   {
   case -1:
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      puts ("fork()");
      fflush (stdout);
      _exit (1);
   case 0:
      player_pid = setsid ();
      break;
   default:
      return;
   } // switch

   char tempo_str[15], cmd[255];

   snprintf (cmd, 250, "madplay -Q %s -s %f -t %f -o \"%s\"",
                 daisy[playing].audio_file, clip_begin,
                 clip_end - clip_begin, tmp_wav);
   system (cmd);
   snprintf (tempo_str, 10, "%lf", tempo);
   playfile (tmp_wav, tempo_str);
/* When the sox trim effedct is used, use this
   playfile (daisy[playing].audio_file, tempo_str);
*/
   _exit (0);
} // play_now

void open_smil_file (char *smil_file, char *anchor)
{
   if (smil_file_fd > -1)
   {
      close (smil_file_fd);
   } // if
   if ((smil_file_fd = open (realname (smil_file), O_RDONLY)) == -1)
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf ("open_smil_file(): %s\n", smil_file);
      fflush (stdout);
      _exit (1);
   } // if
   current_page_number = 0;
   while (1)
   {
      if (get_element_tag_or_label (smil_file_fd) == EOF)
      {
         close (smil_file_fd);
         return;
      } // if
      if (*anchor)
      {
         if (strcasecmp (anchor, attribute.id) == 0)
         {
            get_page_number ();
            break;
         } // if
      }
      else
      {
         if (strcasecmp (tag, "audio") == 0)
         {
            get_clips ();
            break;
         } // if
      } // if
   } // while
   start_time = time (NULL);
} // open_smil_file

void pause_resume ()
{
   if (playing != -1)
      playing = -1;    
   else
      playing = displaying;
   if (smil_file_fd == -1)
         return;
   else
   {
      if (playing != -1)
         kill_player (SIGCONT);
      else
         kill_player (SIGSTOP);
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
   write_wav (daisy[current].audio_file, str);
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

void play_clip (int smil_file_fd)
{
   if (daisy[playing].screen == daisy[current].screen)
   {
      time_t cur_time, addition;
      int played_time, remain_time;

      wattron (screenwin, A_BOLD);
      if (current_page_number)
         mvwprintw (screenwin, daisy[playing].y, 63, "%3i",
                                                     current_page_number);
      cur_time = time (NULL);
      addition = (time_t) (clip_begin - daisy[playing].begin);
      played_time = cur_time - start_time + addition;
      mvwprintw (screenwin, daisy[playing].y, 68, "%02d:%02d", \
                 played_time / 60, played_time % 60);
      remain_time =  daisy[playing].end - played_time;
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
   if (get_next_clips (smil_file_fd))
   {
      play_now ();
      start_time = time (NULL);
      return;
   } // if

// go to next item
   if (++playing >= total_items)
   {
      quit_daisy_player ();
      exit (0);
   } // if
   if (daisy[playing].level <= level)
      displaying = current = playing;
   open_smil_file (daisy[playing].smil_file, daisy[playing].anchor);
   if (just_this_item != -1)
      if (daisy[playing].level <= level)
         playing = just_this_item = -1;
   view_screen ();
} // play_clip    

void skip_left ()
{
   char last_element[255];
   double current_clip;

   if (playing <= 0)
   {
      beep ();
      return;
   } // if
   if (smil_file_fd == -1)
      return;
   kill_player (SIGTERM);
   if (clip_begin == daisy[playing].begin)
   {
// go to previous item
      close (smil_file_fd);
      if (--playing < displaying)
      {
         current = --displaying;
         previous_item ();
         displaying = current;
      } // if
      if (playing < 0)
      {
         beep ();
         playing = 0;
      } // if
      if ((smil_file_fd = open (daisy[playing].smil_file, O_RDONLY)) == -1)
      {
         endwin ();
         playfile (PREFIX"share/daisy-player/error.wav", "1");
         printf (gettext ("Something is wrong with %s\n"),
                 daisy[playing].smil_file);
         fflush (stdout);
         _exit (1);
      } // if
      while (get_element_tag_or_label (smil_file_fd) != EOF)
      {
         if (strcasecmp (tag, "text") == 0)
            get_page_number ();
         if (strcasecmp (tag, "audio") == 0)
            strncpy (last_element, element, 250);
      } // while
      get_attributes (last_element);
      get_clips ();
      play_now ();
      view_screen ();
      return;
   } // if (clip_begin == daisy[playing].begin)

   double old_clip;

   lseek (smil_file_fd, 0, SEEK_SET);
   current_clip = clip_begin;
   while (1)
   {
      if (get_element_tag_or_label (smil_file_fd) == EOF)
         return;
      if (strcasecmp (tag, "text") == 0)
         get_page_number ();
      if (strcasecmp (tag, "audio") == 0)
      {
         old_clip = clip_begin;
         get_clips ();
         if (clip_begin == current_clip)
            break;
      } // if
   } // while
   clip_begin = old_clip;
   play_now ();
} // skip_left

void skip_right ()
{
   if (playing == -1)
   {
      beep ();
      return;
   } // if
   kill_player (SIGTERM);
} // skip_right

void change_level (char key)
{
   int c;

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
   read_daisy_structure (NCC_HTML, 0);
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

   chdir (pw->pw_dir);     
   strncpy (sound_dev, "default", 8);
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
   struct passwd *pw = getpwuid (geteuid ());
   chdir (pw->pw_dir);
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
   fputs ("# default: sound_dev=default\n", w);
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
   kill_player (SIGTERM);
   wait (NULL);
   put_bookmark ();
   save_rc ();
   if (smil_file_fd > -1)
      close (smil_file_fd);
   if (*tmp_ncx)
      unlink (tmp_ncx);
   unlink (tmp_wav);
} // quit_daisy_player

void search (int start, char mode)
{
   int c, found = 0;

   if (mode == '/')
   {
      if (playing != -1)
         kill_player (SIGSTOP);
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
         kill_player (SIGCONT);
      open_smil_file (daisy[current].smil_file, daisy[current].anchor);
      previous_item ();
   }
   else
   {
      beep ();
   } // if
} // search

void kill_player (int sig)
{
   killpg (player_pid, SIGKILL);
   wait (NULL);
   if (sig == SIGCONT)
      play_now ();
} // kill_player

void go_to_page_number ()
{
   int fd;
   char filename[100], anchor[100], pre_filename[100], pre_anchor[100];
   char pn[15];

   kill_player (SIGTERM);
   mvwaddstr (titlewin, 1, 0, "----------------------------------------");
   waddstr (titlewin, "----------------------------------------");
   mvwprintw (titlewin, 1, 0, gettext ("Go to page number:        "));
   echo ();
   wmove (titlewin, 1, 19);
   wgetnstr (titlewin, pn, 5);
   noecho ();
   view_screen ();
   if (! *pn || ! isdigit (*pn))
   {
      beep ();
      skip_left ();
      return;
   } // if
   if ((fd = open (NCC_HTML, O_RDONLY)) == -1)
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf (gettext ("Something is wrong with the %s file\n"), NCC_HTML);
      fflush (stdout);
      _exit (1);
   } // if
   if (strstr (daisy_version, "2.02"))
   {
      do
      {
         if (get_element_tag_or_label (fd) == EOF)
         {
            beep ();
            close (fd);
            return;
         } // if
         if (strcasecmp (tag, "a") == 0)
         {
            strncpy (pre_filename, filename, 90);
            strncpy (pre_anchor, anchor, 90);
            strncpy (filename, attribute.href, 90);
            if (strchr (filename, '#'))
            {
               strncpy (anchor, strchr (filename, '#') + 1, 90);
               *strchr (filename, '#') = 0;
            } // if
         } // if "a"
         if (strcmp (label, pn) == 0)
         {
            close (fd);
            for (current = 0; current <= total_items; current++)
            {
               if (strcasecmp (daisy[current].smil_file, pre_filename) == 0)
                  break;
            } // for
            view_screen ();
            wmove (screenwin, daisy[current].y, daisy[current].x);
            playing = current;
            just_this_item = -1;
            open_smil_file (pre_filename, pre_anchor);
            return;
         } // if
      } while (strcasecmp (tag, "/html") != 0);
   } // if (strstr (daisy_version, "2.02"))
   if (strcmp (daisy_version, "3") == 0)
   {
      while (1)
      {
         if (get_element_tag_or_label (fd) == EOF)
         {
            beep ();
            close (fd);
            return;
         } // if
         if (strcasecmp (tag, "pageTarget") == 0)
         {
            if (strcmp (pn, attribute.value) == 0)
            {
               do
               {
                  if (get_element_tag_or_label (fd) == EOF)
                  {
                     beep ();
                     close (fd);
                     return;
                  } // if
                  if (strcasecmp (tag, "/pageTarget") == 0)
                  {
                     beep ();
                     close (fd);
                     return;
                  } // if
               } while (strcasecmp (tag, "content") != 0);
               strncpy (filename, attribute.src, 90);
               if (strchr (filename, '#'))
               {
                  strncpy (anchor, strchr (filename, '#') + 1, 90);
                  *strchr (filename, '#') = 0;
               } // if
               for (current = 0; current <= total_items; current++)
               {
                  if (strcasecmp (daisy[current].anchor, anchor) == 0)
                     break;
               } // for
               view_screen ();
               wmove (screenwin, daisy[current].y, daisy[current].x);
               playing = current;
               just_this_item = -1;
               open_smil_file (filename, anchor);
               return;
            } // if
         } // if "pageTarget"
      } // while
   } // if (strcmp (daisy_version, "3") == 0)
   beep ();
} // go_to_page_number

void select_next_output_device ()
{
   FILE *r;
   int n, bx, by, y;
   char list[10][255], trash[100];

   getyx (screenwin, by, bx);
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
      fgets (list[n], 100, r);
      fgets (trash, 100, r);
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
         snprintf (sound_dev, 15, "default:%i", y - 3);
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
   smil_file_fd = -1;
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
            system (str);
            view_screen ();
            break;
         } // if
         playing = displaying = current;
         kill_player (SIGTERM);
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
         system ("udisks --unmount /dev/sr0 > /dev/null");
         system ("udisks --eject /dev/sr0 > /dev/null");
         exit (0);
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
         kill_player (SIGSTOP);
         help ();
            kill_player (SIGCONT);
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
            kill_player (SIGTERM);
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
            kill_player (SIGSTOP);
         select_next_output_device ();
         if (playing != -1)
            kill_player (SIGCONT);
         break;
      case 'p':
         put_bookmark();
         break;
      case 'q':
         quit_daisy_player ();
         exit (0);
      case 's':
         playing = just_this_item = -1;
         view_screen ();
         wmove (screenwin, daisy[current].y, daisy[current].x);
         kill_player (SIGTERM);
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
         kill_player (SIGTERM);
         play_now ();
         break;
      case 'D':
         if (tempo <= 0.1)
         {
            beep ();
            break;
         } // if
         tempo -= 0.1;
         kill_player (SIGTERM);
         play_now ();
         break;
      case KEY_HOME:
         tempo = 1;
         kill_player (SIGTERM);
         play_now ();
         break;
      default:
         beep ();
         break;
      } // switch

      if (playing != -1)
         play_clip (smil_file_fd);

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
   puts ("(C)2003-2011 J. Lemmens");
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

char *read_opf (char *name, char *type)
{
   int opf;
   DIR *dir;

   if (! (dir = opendir (name)))
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf (gettext ("Cannot read %s\n"), name);
      fflush (stdout);
      _exit (1);
   } // if
   while ((dirent = readdir (dir)) != NULL)
   {
      if (! strcmp (dirent->d_name, ".") || ! strcmp (dirent->d_name, ".."))
         continue;
      if (strcasecmp (dirent->d_name + strlen (dirent->d_name) - 4,
                 ".opf") == 0)
      {
         closedir (dir);
         break;
      } // if
   } // while
   opf = open (dirent->d_name, O_RDONLY);
   while (1)
   {
      if (get_element_tag_or_label (opf) == EOF)
      {
         close (opf);
         closedir (dir);
         return NULL;
      } // if
      if (*attribute.ncc_totalTime)
         strncpy (ncc_totalTime, attribute.ncc_totalTime, 8);
      if (strcasecmp (tag, "dc:title") == 0)
      {
         do
         {
            get_element_tag_or_label (opf);
         } while (! *label);
         strncpy (daisy_title, label, 90);
      } // if
      if (strcasecmp (attribute.media_type, type) == 0)
      {
         close (opf);
         return attribute.href;
      } // if
   } // while
} // read_opf

char *sort_by_playorder ()
{
   int n, r, w;

   snprintf (tmp_ncx, 200, "/tmp/daisy-player.XXXXXX");
   mkstemp (tmp_ncx);
   unlink (tmp_ncx);
   strcat (tmp_ncx, ".ncx");
   if ((r = open (NCC_HTML, O_RDONLY)) == -1)
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf (gettext ("Something is wrong with the %s file\n"), NCC_HTML);
      fflush (stdout);
      _exit (1);
   } // if
   if ((w = open (tmp_ncx, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) == -1)
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf ("sort_by_playorder(%s)\n", tmp_ncx);
      fflush (stdout);
      _exit (1);
   } // if
   do
   {
      if (get_element_tag_or_label (r) == EOF)
         break;
      if (*element)
         dprintf (w, "%s\n", element);
      else
         dprintf (w, "%s\n", label);
   } while (strcasecmp (tag, "navmap") != 0);
   n = 1;
   do
   {
      *attribute.playorder = 0;
      if (get_element_tag_or_label (r) == EOF)
         break;
      if (atoi (attribute.playorder) == n)
      {
         dprintf (w, "%s\n", element);
         do
         {
            if (get_element_tag_or_label (r) == EOF)
               break;
            if (*element)
               dprintf (w, "%s\n", element);
            else
               dprintf (w, "%s\n", label);
         } while (strcasecmp (tag, "content") != 0);
         n++;
         lseek (r, 0, SEEK_SET);
      } // if
   } while (strcasecmp (tag, "/ncx") != 0);
   close (w);
   close (r);
   return tmp_ncx;
} // sort_by_playorder

int main (int argc, char *argv[])
{
   int r, x, opt;
   char str[255];

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
   puts ("(C)2003-2011 J. Lemmens");
   printf (gettext ("Daisy-player - Version %s\n"), VERSION);
   puts (gettext ("A parser to play Daisy CD's with Linux"));
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
   strncpy (clip_str_b, "clip-begin", 10);
   strncpy (clip_str_e, "clip-end", 10);
   strncpy (NCC_HTML, realname ("ncc.html"), 250);
   if ((r = open (NCC_HTML, O_RDONLY)) != -1)
   {
      while (1)
      {
         if (get_element_tag_or_label (r) == EOF)
            break;
         if (*attribute.dc_format)
         {
            strncpy (daisy_version, attribute.dc_format, 20);
            break;
         } // if
      } // while
   }
   else
   {
// look if this is daisy3
      strncpy (NCC_HTML, read_opf (daisy_mp, "application/x-dtbncx+xml"), 250);
      strncpy (NCC_HTML, sort_by_playorder (), 250);
      if ((r = open (NCC_HTML, O_RDONLY)) == -1)
      {
         endwin ();
         playfile (PREFIX"share/daisy-player/error.wav", "1");
         printf (gettext ("Cannot find a daisy structure in %s\n"), daisy_mp);
         fflush (stdout);
         _exit (1);
      } // if
      strncpy (daisy_version, "3", 2);
      strncpy (clip_str_b, "clipbegin", 10);
      strncpy (clip_str_e, "clipend", 10);
   } // if
/* discinfo.html
   if (r = open (NCC_HTML, O_RDONLY)) == -1)
   {
      strncpy (discinfo_html, "discinfo.html", 15);
      if ((discinfo_fp = open (discinfo_html, O_RDONLY)) >= 0)
      {
         discinfo = 1;
         read_daisy_structure (discinfo_html, 0);
         browse ();
         close (discinfo_fp);
         exit (0);
      }
      else
      {
         endwin ();
         puts ("");
         playfile (PREFIX"share/daisy-player/error.wav", "1");
         printf (gettext (CANNOT_FIND_A_DAISY_STRUCTURE), NCC_HTML);
         fflush (stdout);
         _exit (1);
      } // if
   } // if
discinfo.html */
   wattron (titlewin, A_BOLD);
   snprintf (str, 250, gettext ("Daisy-player - Version %s - (C)2011 J. Lemmens"), VERSION);
   wprintw (titlewin, str);
   wrefresh (titlewin);
   lseek (r, 0, SEEK_SET);
   if (strstr (daisy_version, "2.02"))
   {
      while (1)
      {
         if (get_element_tag_or_label (r) == EOF)
            break;
         if (strcasecmp (tag, "title") == 0)
         {
            if (get_element_tag_or_label (r) == EOF)
               break;
            if (*label)
            {
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
   } // if
   if (strcmp (daisy_version, "3") == 0)
   {
      while (1)
      {
         if (get_element_tag_or_label (r) == EOF)
            break;
         if (strcasecmp (tag, "docTitle") == 0)
         {
            do
            {
               if (get_element_tag_or_label (r) == EOF)
                  break;
            } while (! *label);
            strncpy (daisy_title, label, 90);
            strncpy (bookmark_title, label, 90);
         } // if
      } // while
   } // if
   close (r);
   read_daisy_structure (NCC_HTML, 1);
   snprintf (tmp_wav, 200, "/tmp/daisy_player.XXXXXX");
   mkstemp (tmp_wav);
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
