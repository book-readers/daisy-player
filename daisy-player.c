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
#include <cdio/cdio.h>
#include <locale.h>
#include <libintl.h>
#include "entities.h"
#include <stringprep.h>
#include <sox.h>
#include <errno.h>

#define VERSION "6.4"

WINDOW *screenwin, *titlewin;
int smil_file_fd, discinfo_fp, discinfo = 00, multi = 0, displaying = 0;
int playing, just_this_item;
int bytes_read, ncc_maxPageNormal, current_book_page;
char ncc_timeInThisSmil[10], page_number[15], label[255];
char tag[255], element[255], search_str[30], tmp[100];
pid_t player_pid, daisy_player_pid;
char clip_begin[100], clip_end[100], daisy_title[255], prog_name[255];
struct
{
   int x, y, screen;
   float begin, end;
   char smil_file[255], anchor[255], label[255];
   char audio_file[255];
   int level, book_page;
} daisy[2000];
int current, max_y, max_x, max_items, level, depth;
float audio_total_length, tempo;
char NCC_HTML[255], discinfo_html[255], ncc_totalTime[10];
char sound_dev[16], daisy_mp[100];
time_t start_time;
DIR *dir;
struct dirent *dirent;

extern void kill_player (int sig);
extern void view_screen ();
extern void open_smil_file (char *, char *);
extern int get_next_clips (int);
extern void save_rc ();
extern void skip_left ();
extern void replay_playing_clip ();
extern void previous_item ();
extern void quit_daisy_player ();
extern int madplay (int, char **);

char *to_lowercase (char *name)
{
   char *p;

   p = name;
   while (*p)
   {
      *p = tolower (*p);
      p++;
   } // while
   return name;
} // to_lowercase

void playfile (char *filename, char *tempo)
{
  sox_format_t *in, *out; /* input and output files */
  sox_effects_chain_t *chain;
  sox_effect_t *e;
  char *args[100], str[100];

  sox_globals.verbosity = 0;
  sox_init();
  in = sox_open_read (filename, NULL, NULL, NULL);
  while (! (out =
         sox_open_write (sound_dev, &in->signal, NULL, "alsa", NULL, NULL)))
  {
    strcpy (sound_dev, "default");
    save_rc ();
    if (out)
      sox_close (out);
  } // while

  chain = sox_create_effects_chain (&in->encoding, &out->encoding);

  e = sox_create_effect (sox_find_effect ("input"));
  args[0] = (char *) in, sox_effect_options (e, 1, args);
  sox_add_effect (chain, e, &in->signal, &in->signal);

  e = sox_create_effect (sox_find_effect ("tempo"));
  args[0] = tempo, sox_effect_options (e, 1, args);
  sox_add_effect (chain, e, &in->signal, &in->signal);

  sprintf (str, "%f", out->signal.rate);
  e = sox_create_effect (sox_find_effect ("rate"));
  args[0] = str, sox_effect_options (e, 1, args);
  sox_add_effect (chain, e, &in->signal, &in->signal);

  sprintf (str, "%i", out->signal.channels);
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

void put_bookmark ()
{
   int w;
   char str[255];

   sprintf (str, "%s/.daisy-player", getenv ("HOME"));
   mkdir (str, 0755);
   sprintf (str, "%s/.daisy-player/%s", getenv ("HOME"), daisy_title);
   if ((w = creat (str, 0644)) != -1)
   {
      if (playing == -1)
         playing = current;
      dprintf (w, "%s\n", daisy[playing].smil_file);
      dprintf (w, "%s\n", clip_begin);
      dprintf (w, "%d\n", level);
      close (w);                   
   } // if
} // put_bookmark

void get_bookmark ()
{
   char str[255], begin[50];
   FILE *r;

   sprintf (str, "%s/.daisy-player/%s", getenv ("HOME"), daisy_title);
   if ((r = fopen (str, "r")) == NULL)
      return;
   fscanf (r, "%s\n%s\n%d", str, begin, &level);
   fclose (r);
   for (playing = 0; playing <= max_items; playing++)
   {
      if (strcasecmp (daisy[playing].smil_file, str) == 0)
         break;
   } // for
   if (playing > max_items + 1)
      playing = 0;
   current = playing;
   previous_item ();
   view_screen ();
   wmove (screenwin, daisy[current].y, daisy[current].x);
   displaying= current;
   just_this_item = -1;
   open_smil_file(daisy[playing].smil_file, daisy[playing].anchor);
   while (strcmp (clip_begin, begin) != 0)
   {
      if (get_next_clips (smil_file_fd) == EOF)
      {
         if ((smil_file_fd =
                open (realname (daisy[playing].smil_file), O_RDONLY)) == -1)
         {
            endwin ();
            playfile (PREFIX"share/daisy-player/error.wav", "1");
            kill (player_pid, SIGTERM);
            printf ("get_bookmark(): %s.\n", realname (daisy[playing].smil_file));
            fflush (stdout);
            _exit (1);
         } // if
         return;
      } // if
   } // while
   replay_playing_clip ();
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
  strcpy (orig, new);
} // html_entities_to_utf8

int get_element_or_label (int r)
{
   char *p, h;
   static char read_flag = 0;

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
            return 0;
         switch (read (r, ++p, 1))
         {
         case -1:
            endwin ();
            playfile (PREFIX"share/daisy-player/error.wav", "1");
            printf ("get_element_or_label: %s\n", p);
            fflush (stdout);
            _exit (1);
         case 0:
            *++p = 0;
            return EOF;
         } // switch
      } while (*p != '>');
      *++p = 0;
      return 0;
   } // if
   *label = *p;
   *element = 0;
   p = label;
   do
   {
      if (p - label > 250)
         return 0;
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
   } while (*p != '<');
   read_flag = 1;
   *p = 0;
   return 0;
} // get_element_or_label

void get_tag ()
{
   char *p;

   p = tag;
   while (*p != ' ' && *p != '>')
     p++;
   *p = 0;
} // get_tag

void replay_playing_clip ()
{
   if (smil_file_fd < 0)
      return;
   lseek (smil_file_fd, 0, SEEK_SET);
   while (get_element_or_label (smil_file_fd) != EOF)
      if (strstr (element, clip_begin) != NULL)
         return;
} // replay_playing_clip

void get_book_page ()
{
   int fd;
   char *p, file[100], anchor[100];

   p = strcasestr (element, "src");
   while (*++p != '=');
   while (*p != '\'' && *p != '"')
      p++;
   strcpy (file, p + 1);
   p = file;
   while (*p != '"' && *p != '\'' && *p != '>' && *p != '#')
      p++;
   *p++ = 0;
   strcpy (anchor, p);
   p = anchor;
   while (*p != '"' && *p != '\'' && *p != '>' && *p != '#')
      p++;
   *p = 0;
   if ((fd = open (realname (file), O_RDONLY)) == -1)
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf ("get_book_page(): %s\n", file);
      fflush (stdout);
      _exit (1);
   } // if
   while (1)
   {
      if (get_element_or_label (fd) == EOF)
      {
         close (fd);
         return;
      } // if
      if (strcasestr (element, anchor))
         break;
   } // while
   while (1)
   {
      if (get_element_or_label (fd) == EOF)
      {
         close (fd);
         return;
      } // if
      strcpy (tag, element + 1);
      get_tag ();
      if ((tag[0] == 'h' || tag[0] == 'H') && isdigit (tag[1]))
      {
         close (fd);
         return;
      } // if
      if (strcasecmp (tag, "span") == 0)
         break;
   } // while
   while (1)
   {
      if (get_element_or_label (fd) == EOF)
      {
         close (fd);
         return;           
      } // if
      if (isdigit (*label))
      {
         current_book_page = atoi (label);
         close (fd);
         return;
      } // if
   } // while
   close (fd);
} // get_book_page

int get_next_clips (int fd)
{
   char *p;

   if (fd != EOF)
   {
      while (1)
      {
         if (get_element_or_label (fd) == EOF)
         {
            close (fd);
            return EOF;
         } // if
         strcpy (tag, element + 1);
         get_tag ();
         if (strcasecmp (tag, "text") == 0)
            get_book_page ();
         if (strcasestr (element, "clip-begin"))
            break;
      } // while
   } // if
   p = strcasestr (element, "clip-begin");
   while (! isdigit (*++p));
   strcpy (clip_begin, p);
   *strchr (clip_begin, 's') = 0;
   p = strcasestr (element, "clip-end");
   while (! isdigit (*++p));
   strcpy (clip_end, p);
   *strchr (clip_end, 's') = 0;
   return 0;
} // get_next_clips

void compose_audio_file (int r)
{
   char f[255], *p;

// get audio filename
   do
   {
      if (get_element_or_label (r) == EOF)
         return;
   } while (! strcasestr (element, "audio"));
   strcpy (f, strcasestr (element, "src=\"") + 5);
   *strchr (f, '"') = 0;
   strcpy (daisy[current].audio_file, realname (f));

// fill begin
   p = strcasestr (element, "clip-begin");
   while (! isdigit (*++p));
   strcpy (clip_begin, p);
   *strchr (clip_begin, 's') = 0;
   daisy[current].begin = atof (clip_begin);
} // compose_audio_file

void parse_smil (char *smil_file)
{
   char *s;
   int r;

   r = open (smil_file, O_RDONLY);
   while (get_element_or_label (r) != EOF)
   {
      if (strcasestr (element, "ncc:timeInThisSmil") != NULL)
      {
         s = strcasestr (element, "content");
         while (! isdigit (*++s));
         strncpy (ncc_timeInThisSmil, s, 8);
         *(ncc_timeInThisSmil + 2) = 0;
         daisy[current].end  = atof (ncc_timeInThisSmil) * 3600;
         *(ncc_timeInThisSmil + 5) = 0;
         daisy[current].end += atof (ncc_timeInThisSmil + 3) * 60;
         *(ncc_timeInThisSmil + 8) = 0;
         daisy[current].end += atof (ncc_timeInThisSmil + 6);
         break;
      } // if
   } // while
   while (1)
// start at anchor
   {
      if (get_element_or_label (r) == EOF)
      {
         lseek (r, 0, SEEK_SET);
         *daisy[current].anchor = 0;
         break;                              
      } // if
      if (*daisy[current].anchor != 0)
         if (strcasestr (element, daisy[current].anchor))
            break;
   } // while
   compose_audio_file (r);
   close (r);
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
      if (ncc_maxPageNormal)
         mvwprintw (titlewin, 1, 15, gettext (" %d pages "), ncc_maxPageNormal);
      mvwprintw (titlewin, 1, 47,
                 gettext (" total length: %s "), ncc_totalTime);
      mvwprintw (titlewin, 1, 74, " %d/%d ", \
                 daisy[current].screen + 1, max_items / max_y + 1);
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
         if (daisy[i].book_page)
            mvwprintw (screenwin, daisy[i].y, 62,
                       "(%3d)", daisy[i].book_page);
         if (daisy[i].level <= level)
            mvwprintw (screenwin,daisy[i].y, 74, "%02d:%02d",
                 (int) (daisy[i].end - daisy[i].begin + .5) / 60, \
                 (int) (daisy[i].end - daisy[i].begin + .5) % 60);
      } // if
      if (i > max_items)
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

   for (c = 0; c <= max_items; c++)
   {
      if (daisy[c + 1].level > level)
      {
         for (x = c + 1; daisy[x].level > level && x <= max_items + 1; x++)
            daisy[c].end += daisy[x].end;
      } // if
   } // for
} // remake_structure

void read_data_cd (char *html_file, int flag)
{
   int x, indent = 0, r, displaying = -1;
   char *p, *str;

   if ((r = open (html_file, O_RDONLY)) == -1)
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf (gettext ("Corrupt daisy structure %s\n"), html_file);
      fflush (stdout);
      _exit (1);
   } // if
   current = -1;
   do
   {
      if (get_element_or_label (r) == EOF)
         return;
      strcpy (tag, element + 1);
      get_tag ();
      if (strcasecmp (tag, "meta") == 0)
      {
         if (strcasestr (element, "ncc:maxPageNormal") != NULL)
         {
            str = strcasestr (element, "content");
            while (! isdigit (*++str));
            p = str;
            while (isdigit (*++p));
            *p = 0;
            ncc_maxPageNormal = atoi (str);
         } // if
         if (strcasestr (element, "ncc:totalTime") != NULL)
         {
            str = strcasestr (element, "content");
            while (! isdigit (*++str));
            strncpy (ncc_totalTime, str, 8);
         } // if
      } // if (! strcasecmp (tag, "meta"))
      if (! strcasecmp (tag, "h1") ||
          ! strcasecmp (tag, "h2") ||
          ! strcasecmp (tag, "h3") ||
          ! strcasecmp (tag, "h4") ||
          ! strcasecmp (tag, "h5") ||
          ! strcasecmp (tag, "h6"))
      {
         int l;

         current++;
         daisy[current].book_page = 0;
         l = tag[1] - '0';
         daisy[current].level = l;
         if (l > depth)
            depth = l;
         indent = daisy[current].x = (l - 1) * 3 + 1;
      } // if (! strcasecmp (tag, "h1") || ...
      if (strcasecmp (tag, "a") == 0)
      {
         p = element;
         while (*p++ != '"');
         x = 0;
         while (*p != '#' && *p != '"')
            daisy[current].smil_file[x++] = *p++;
         daisy[current].smil_file[x] = 0;
         strcpy (daisy[current].smil_file, realname (daisy[current].smil_file));
         strcpy (daisy[current].anchor, ++p);
         p = daisy[current].anchor;
         while (*p++ != '"');
         *--p = 0;

// read label
         get_element_or_label (r);
         html_entities_to_utf8 (label);
         strcpy (daisy[current].label, label);
         daisy[current].label[58 - daisy[current].x] = 0;

         if (flag)
         {
            mvwprintw (titlewin, 1, 0, "----------------------------------------");
            wprintw (titlewin, "----------------------------------------");
            mvwprintw (titlewin, 1, 0, gettext ("Reading %s... "),
               daisy[current].label);
            wrefresh (titlewin);
         } // if
         if (++displaying == max_y)
            displaying = 0;
         daisy[current].y = displaying;
         daisy[current].screen = current / max_y;

         if (daisy[current].x == 0)
            daisy[current].x = indent + 3;
         if (! discinfo)
            parse_smil (daisy[current].smil_file);
      } // if (! strcasecmp (tag, "a"))
      if (strcasecmp (tag, "span") == 0)
      {
         do
         {
            get_element_or_label (r);
            strcpy (tag, element + 1);
            get_tag ();
            if (! daisy[current].book_page && isdigit (*label))
            {
               daisy[current].book_page = atoi (label);
               break;
            } // if
         } while (strcasecmp (tag, "/span") != 0);
         continue;
      } // if (! strcasecmp (tag, "span"))
   } while (strcasecmp (tag, "/html") != 0);
   max_items = current - 1;
   close (r);
} // read_data_cd

void player_ended ()
{
   wait (NULL);
} // player_ended

void player ()
{
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

   sprintf (cmd, "madplay -Q %s -s %s -t %f -o %s",
                 daisy[playing].audio_file, clip_begin,
                 atof (clip_end) - atof (clip_begin), tmp);
   system (cmd);
   sprintf (tempo_str, "%f", tempo);
   playfile (tmp, tempo_str);
   _exit (0);
} // player

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
      printf ("open_smil_file(): %s\n", realname (smil_file));
      fflush (stdout);
      _exit (1);
   } // if
   current_book_page = 0;
   while (1)
   {
      if (get_element_or_label (smil_file_fd) == EOF)
      {
         close (smil_file_fd);
         return;
      } // if
      strcpy (tag, element + 1);
      get_tag ();
      if (*anchor)
      {
         if (strcasestr (element, anchor))
         {
            get_book_page ();
            break;
         } // if
      }
      else
      {
         if (strcasecmp (tag, "audio") == 0)
            break;
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
  sprintf (str, "%s/%s", getenv ("HOME"), outfile);
  out = sox_open_write (str, &in->signal, NULL, NULL, NULL, NULL);
  while ((number_read = sox_read (in, samples, 2048)))
    sox_write (out, samples, number_read);
  sox_close (in);
  sox_close (out);
} // write_wav

void store_to_disk ()
{
sleep (1);
   char str[100];

   wclear (screenwin);
   sprintf (str, "%s.wav", daisy[current].label);
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
   if (current > max_items)
   {
      beep ();
      return;
   } // if
   while (daisy[++current].level > level)
   {
      if (current >= max_items)
      {
         beep ();
         previous_item ();
         return;
      } // if
   } // while
   view_screen ();
   wmove (screenwin, daisy[current].y, daisy[current].x);
} // next_item

void play_datacd (int smil_file_fd)
{
   if (daisy[displaying].screen == daisy[current].screen)
   {
      time_t cur_time, addition;
      int played_time, remain_time;

      wattron (screenwin, A_BOLD);
      if (current_book_page)
         mvwprintw (screenwin, daisy[displaying].y, 63, "%3i",
                                                     current_book_page);
      cur_time = time (NULL);
      addition = (time_t) (atof (clip_begin) - daisy[displaying].begin);
      played_time = cur_time - start_time + addition;
      mvwprintw (screenwin, daisy[displaying].y, 68, "%02d:%02d", \
                 played_time / 60, played_time % 60);
         remain_time =  daisy[displaying].end - played_time;
      mvwprintw (screenwin, daisy[displaying].y, 74, "%02d:%02d", \
                 remain_time / 60, remain_time % 60);
      wattroff (screenwin, A_BOLD);
      wmove (screenwin, daisy[current].y, daisy[current].x);
      wrefresh (screenwin);
   } // if
   if (kill (player_pid, 0) == 0)
      return;
   player_pid = -2;
   if (get_next_clips (smil_file_fd) != EOF)
   {
      player ();
      start_time = time (NULL);
      return;
   } // if

// go to next item
   playing++;
   if (playing > max_items + 1)
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
} // play_datacd

void skip_left ()
{
   char last_clip[255];

   if (playing == -1)
   {
      beep ();
      return;
   } // if
   if (smil_file_fd == -1)
      return;
   kill_player (SIGTERM);
   if (atof (clip_begin) == daisy[playing].begin)
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
      while (get_element_or_label (smil_file_fd) != EOF)
      {
         strcpy (tag, element + 1);
         get_tag ();
         if (strcasecmp (tag, "text") == 0)
            get_book_page ();
         if (strstr (element, "clip-begin"))
            strcpy (last_clip, element);
      } // while
      strcpy (element, last_clip);
      get_next_clips (-1);
      player ();
      view_screen ();
      return;
   } // if (atof (clip_begin) == daisy[playing].begin)
   lseek (smil_file_fd, 0, SEEK_SET);
   while (1)
   {
      get_element_or_label (smil_file_fd);
      strcpy (tag, element + 1);
      get_tag ();
      if (strcasecmp (tag, "text") == 0)
         get_book_page ();
      if (strstr (element, clip_begin))
         break;
      if (strcasestr (element, "/body"))
         return;
   } // while
   get_next_clips (-1);
   player ();
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
   read_data_cd (NCC_HTML, 0);
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
   strcpy (sound_dev, "default");
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
   fprintf (w, "speed=%f\n", tempo);
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
   unlink (tmp);
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
      for (c = start; c <= max_items + 1; c++)
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
         for (c = max_items + 1; c > start; c--)
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
      just_this_item = -1;
      view_screen ();
      playing = current;
      if (playing != -1)
         kill_player (SIGCONT);
      open_smil_file (daisy[current].smil_file, daisy[current].anchor);
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
      replay_playing_clip ();
} // kill_player

void go_to_page_number ()
{
   int fd;
   char *p, filename[100], anchor[100], pre_filename[100], pre_anchor[100];

   kill_player (SIGTERM);
   mvwaddstr (titlewin, 1, 0, "----------------------------------------");
   waddstr (titlewin, "----------------------------------------");
   mvwprintw (titlewin, 1, 0, gettext ("Go to page number:        "));
   echo ();
   wmove (titlewin, 1, 19);
   wgetnstr (titlewin, page_number, 5);
   noecho ();
   view_screen ();
   if (! *page_number || ! isdigit (*page_number))
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
   do
   {
      if (get_element_or_label (fd) == EOF)
      {
         beep ();
         close (fd);
         return;
      } // if
      strcpy (tag, element + 1);
      get_tag ();
      if (strcasecmp (tag, "a") == 0 && ! *label)
      {
         strcpy (pre_filename, filename);
         strcpy (pre_anchor, anchor);
         p = strcasestr (element, "href");
         while (*++p != '=');
         while (*p != '\'' && *p != '"')
            p++;
         strcpy (filename, p + 1);
         p = filename;
         while (*p != '"' && *p != '\'' && *p != '>' && *p != '#')
            p++;
         *p++ = 0;
         strcpy (anchor, p);
         p = anchor;
         while (*p != '"' && *p != '\'' && *p != '>' && *p != '#')
            p++;
         *p = 0;
      } // if "a"
      if (strcmp (label, page_number) == 0)
      {
         close (fd);
         for (current = 0; current <= max_items; current++)
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
         sprintf (sound_dev, "default:%i", y - 3);
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
            sprintf (str, "%s -m %s -d %s", prog_name,
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
         cdio_eject_media_drive ("/dev/sr0");
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
         if (ncc_maxPageNormal)
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
         if (daisy[current].screen == max_items / max_y)
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
         tempo += 0.1;
         kill_player (SIGTERM);
         replay_playing_clip ();
         break;
      case 'D':
         tempo -= 0.1;
         kill_player (SIGTERM);
         replay_playing_clip ();
         break;
      case KEY_HOME:
         tempo = 1;
         kill_player (SIGTERM);
         replay_playing_clip ();
         break;
      default:
         beep ();
         break;
      } // switch

      if (playing != -1)
         play_datacd (smil_file_fd);

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
      strcpy (daisy_mp, strchr (str, ' ') + 1);
      *strchr (daisy_mp, ' ') = 0;
      return daisy_mp;
   } // if
   return NULL;
} // get_mount_point

int main (int argc, char *argv[])
{
   int r, x, opt;
   char str[255];

   fclose (stderr); // discard SoX messages
   strcpy (prog_name, basename (argv[0]));
   daisy_player_pid = getpid ();
   tempo = 1;
   atexit (quit_daisy_player);
   strcpy (tmp, "/tmp/daisy-player.XXXXXX");
   mkstemp (tmp);
   unlink (tmp);
   strcat (tmp, ".wav");
   read_rc ();
   setlocale (LC_ALL, getenv ("LANG"));
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
         strcpy (sound_dev, optarg);
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
      if (*argv[optind] == '/')
         strcpy (daisy_mp, argv[optind]);
      else
         sprintf (daisy_mp, "%s/%s", getenv ("PWD"), argv[optind]);
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
   strcpy (NCC_HTML, realname ("ncc.html"));
   if ((r = open (NCC_HTML, O_RDONLY)) == -1)
   {
      endwin ();
      playfile (PREFIX"share/daisy-player/error.wav", "1");
      printf (gettext ("Cannot find a daisy structure in %s\n"), daisy_mp);
      fflush (stdout);
      _exit (1);
   } // if
/*
   if (r = open (NCC_HTML, O_RDONLY)) == -1)
   {
      strcpy (discinfo_html, "discinfo.html");
      if ((discinfo_fp = open (discinfo_html, O_RDONLY)) >= 0)
      {
         discinfo = 1;
         read_data_cd (discinfo_html, 0);
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
*/
   wattron (titlewin, A_BOLD);
   sprintf (str, gettext ("Daisy-player - Version %s - (C)2011 J. Lemmens"), VERSION);
   wprintw (titlewin, str);
   wrefresh (titlewin);
   do
   {
      if (get_element_or_label (r) == EOF)
         break;
      strcpy (tag, element + 1);
      get_tag ();
   } while (strcasecmp (tag, "title"));
   get_element_or_label (r);
   for (x = strlen (label) - 1; x >= 0; x--)
      if (isspace (label[x]))
         label[x] = 0;
      else
         break;
   strcpy (daisy_title, label);
   close (r);
   read_data_cd (NCC_HTML, 1);
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
