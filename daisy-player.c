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
#include <errno.h>
#include <err.h>
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
#include <syslog.h>

#define VERSION "6.1.1"
#define BUF_SIZE 2048
#define FACTOR (44100 * 2 * 2)

WINDOW *scherm, *titlewin;
int smil_file_fd, discinfo_fp, discinfo = 00, multi = 0;
int playing, playing_y, playing_page, just_this_item;
char from_page[10], to_page[10], current_book_page[100];
int bytes_read, ncc_maxPageNormal;
char ncc_timeInThisSmil[10], page_number[15], label[255];
char tag[255], element[255], search_str[30];
pid_t player_pid, daysy_player_pid;
char clip_begin[100], clip_end[100], daisy_title[255], prog_name[255];
struct
{
   int y, x;
   char begin[100];
   int end;
   char smil_file[255], anchor[255], label[255], pages[25];
   char audio_file[255];
   int level, page;
} daisy[2000];
int current, max_y, max_x, max_items, cur_page, level, depth;
float audio_total_length, speed;
char NCC_HTML[255], discinfo_html[255], ncc_totalTime[10];
char sound_dev[10], daisy_mp[100];
time_t start_time;
DIR *dir;
struct dirent *dirent;

extern void kill_player (int sig);
extern void view_page (int page);
extern void start_playing (char *, char *);
extern void skip_left ();
extern int get_next_clips (int);

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

void playmp3 (char *filename, char *x, char *y, char *s, int z)
{
   char str[100];

   sprintf (str, "play -q %s", filename);
   system (str);
} // playmp3

char *realname (char *name)
{
   if (! (dir = opendir (daisy_mp)))
   {
      endwin ();
      playmp3 ("/usr/local/share/daisy-player/error.mp3", NULL, NULL, sound_dev, 0);
      fprintf (stderr, gettext ("\nCannot read %s\n"), daisy_mp);
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
   for (current = 0; current <= max_items; current++)
   {
      if (strcasecmp (daisy[current].smil_file, str) == 0)
         break;
   } // for
   if (current > max_items)
   {
      current = 0;
      return;
   } // if
   cur_page = daisy[current].page;
   view_page (cur_page);
   wmove (scherm, daisy[current].y, daisy[current].x);
   playing = current;
   playing_page = daisy[current].page;
   playing_y = daisy[current].y;
   just_this_item = -1;
   start_playing(daisy[current].smil_file, daisy[current].anchor);
   while (strcmp (clip_begin, begin) != 0)
   {
      if (get_next_clips (smil_file_fd) == EOF)
      {
         if ((smil_file_fd =
                open (realname (daisy[current].smil_file), O_RDONLY)) == -1)
         {
            endwin ();
            playmp3 ("/usr/local/share/daisy-player/error.mp3", NULL, NULL, sound_dev, 0);
            kill (player_pid, SIGTERM);
            errx (1, "get_bookmark(): %s.\n", realname (daisy[current].smil_file));
         } // if
         return;
      } // if
   } // while
   skip_left ();
} // get_bookmark

void convert_html_entities (char *s)
{
  int e_flag;
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
      if (! strcmp (entity, "aacute"))
        *n = 'a';
      if (! strcmp (entity, "Aacute"))
        *n = 'A';
      if (! strcmp (entity, "acirc"))
        *n = 'a';
      if (! strcmp (entity, "acute"))
        *n = '\'';
      if (! strcmp (entity, "aelig"))
      {
        *n++ = 'a';
        *n = 'e';
      } // if
      if (! strcmp (entity, "agrave") || ! strcmp (entity, "auml"))
        *n = 'a';
      if (! strcmp (entity, "Agrave") || ! strcmp (entity, "Auml"))
        *n = 'A';
      if (! strcmp (entity, "apos"))
        *n = '\'';
      if (! strcmp (entity, "aring"))
        *n = 'a';
      if (! strcmp (entity, "Aring"))
        *n = 'A';
      if (! strcmp (entity, "atilde"))
        *n = 'a';
      if (! strcmp (entity, "Atilde"))
        *n = 'A';
      if (! strcmp (entity, "amp") || ! strcmp (entity, "AMP") || ! strcmp (entity, "#038"))
        *n = '&';
      if (! strcmp (entity, "Atilde"))
        *n = 'A';
      if (! strcmp (entity, "apos"))
        ++n;
      if (! strcmp (entity, "bdquo"))
        *n = '`';
      if (! strcmp (entity, "bull"))
        *n = 'o';
      if (! strcmp (entity, "cedil"))
        *n = ' ';
      if (! strcmp (entity, "ccedil"))
        *n = 'c';
      if (! strcmp (entity, "Ccedil"))
        *n = 'C';
      if (! strcmp (entity, "copy"))
      {
        strncpy (n, " copyright", 10);
        n += 9;
      } // if
      if (! strcmp (entity, "curren"))
        *n = ' ';
      if (! strcmp (entity, "deg") || ! strcmp (entity, "#176"))
      {
        strncpy (n, " graden", 7);
        n += 6;
      } // if
      if (! strcmp (entity, "eacute"))
        *n = 'e';
      if (! strcmp (entity, "Eacute"))
        *n = 'E';
      if (! strcmp (entity, "ecirc"))
        *n = 'e';
      if (! strcmp (entity, "egrave"))
        *n = 'e';
      if (! strcmp (entity, "Egrave"))
        *n = 'E';
      if (! strcmp (entity, "euml"))
        *n = 'e';
      if (! strcmp (entity, "Euml"))
        *n = 'E';
      if (! strcmp (entity, "euro") || ! strcmp (entity, "#128") || ! strcmp (entity, "#8364"))
      {
        strncpy (n, " euro", 5);
        n += 4;
      } // if
      if (! strcmp (entity, "fnof") || ! strcmp (entity, "#402"))
        *n = 'f';
      if (! strcmp (entity, "frac12") || ! strcmp (entity, "frac14"))
        ++n;
      if (! strcmp (entity, "gt"))
        *n = '>';
      if (! strcmp (entity, "hearts"))
        *n = ' ';
      if (! strcmp (entity, "hellip") || ! strcmp (entity, "#133"))
      {
        strncpy (n, "...", 3);
        n += 2;
      } // if
      if (! strcmp (entity, "iacute"))
        *n = 'i';
      if (! strcmp (entity, "Iacute"))
        *n = 'I';
      if (! strcmp (entity, "icirc") || ! strcmp (entity, "igrave"))
        *n = 'i';
      if (! strcmp (entity, "Icirc"))
        *n = 'I';
      if (! strcmp (entity, "iexcl"))
        ++n;
      if (! strcmp (entity, "iuml"))
        *n = 'i';
      if (! strcmp (entity, "Iuml"))
        *n = 'I';
      if (! strcmp (entity, "laquo"))
        *n = '<';
      if (! strcmp (entity, "ldquo"))
        *n = '"';
      if (! strcmp (entity, "lsquo"))
        *n = '`';
      if (! strcmp (entity, "lt"))
        *n = '<';
      if (! strcmp (entity, "macr"))
        *n = ' ';
      if (! strcmp (entity, "mdash"))
      {
        strncpy (n, "--", 2);
        n++;
      } // if
      if (! strcmp (entity, "micro"))
        *n = ' ';
      if (! strcmp (entity, "minus"))
        *n = '-';
      if (! strcmp (entity, "nbsp"))
        *n = ' ';
      if (! strcmp (entity, "ndash"))
        *n = '-';
      if (! strcmp (entity, "ntilde"))
        *n = 'n';
      if (! strcmp (entity, "Ntilde"))
        *n = 'N';
      if (! strcmp (entity, "oacute"))
        *n = 'o';
      if (! strcmp (entity, "Oacute"))
        *n = 'O';
      if (! strcmp (entity, "ocirc"))
        *n = 'o';
      if (! strcmp (entity, "oelig") || ! strcmp (entity, "#339"))
      {
        *n++ = 'o';
        *n = 'e';
      } // if
      if (! strcmp (entity, "ograve"))
        *n = 'o';
      if (! strcmp (entity, "ordm"))
        *n = 'Y';
      if (! strcmp (entity, "oslash"))
        *n = 'o';
      if (! strcmp (entity, "Oslash"))
        *n = 'O';
      if (! strcmp (entity, "ouml"))
        *n = 'o';
      if (! strcmp (entity, "Ouml"))
        *n = 'O';
      if (! strcmp (entity, "para"))
        *n = '-';
      if (! strcmp (entity, "pound"))
      {
        strncpy (n, "pound", 5);
        n++;
      } // if
      if (! strcmp (entity, "quot"))
        *n = '"';
      if (! strcmp (entity, "raquo"))
        *n = '>';
      if (! strcmp (entity, "rdquo") || ! strcmp (entity, "#039"))
        *n = '\'';
      if (! strcmp (entity, "rsaquo"))
        *n = '>';
      if (! strcmp (entity, "rsquo"))
        *n = '\'';
      if (! strcmp (entity, "scaron") || ! strcmp (entity, "#353"))
        *n = 's';
      if (! strcmp (entity, "Scaron") || ! strcmp (entity, "#352"))
        *n = 'S';
      if (! strcmp (entity, "shy"))
        *n = '-';
      if (! strcmp (entity, "sup2") || ! strcmp (entity, "sup3"))
        ++n;
      if (! strcmp (entity, "szlig"))
      {
        strncpy (n, "ss", 2);
        n++;
      } // if
      if (! strcmp (entity, "times"))
        *n = '*';
      if (! strcmp (entity, "trade") || ! strcmp (entity, "#153") || ! strcmp (entity, "reg"))
      {
        strncpy (n, "(TM)", 4);
        n += 3;
      } // if
      if (! strcmp (entity, "uacute"))
        *n = 'u';
      if (! strcmp (entity, "Uacute"))
        *n = 'U';
      if (! strcmp (entity, "ucirc") || ! strcmp (entity, "ugrave"))
        *n = 'u';
      if (! strcmp (entity, "uml"))
        ++n;
      if (! strcmp (entity, "uuml"))
        *n = 'u';
      if (! strcmp (entity, "Uuml"))
        *n = 'U';
      if (! strcmp (entity, "yacute"))
        *n = 'y';
      if (! strcmp (entity, "Yacute"))
        *n = 'Y';
      if (! strcmp (entity, "yuml"))
        *n = 'y';
      if (! strcmp (entity, "#39"))
        *n = '"';
      if (! strcmp (entity, "#124"))
        *n = '|';
      if (! strcmp (entity, "#130"))
        *n = '\'';
      if (! strcmp (entity, "#131"))
        *n = 'f';
      if (! strcmp (entity, "#132"))
        *n = '"';
      if (! strcmp (entity, "#137"))
        ++n;
      if (! strcmp (entity, "#145"))
        *n = '\'';
      if (! strcmp (entity, "#146"))
        *n = '\'';
      if (! strcmp (entity, "#147"))
        *n = '"';
      if (! strcmp (entity, "#148"))
        *n = '"';
      if (! strcmp (entity, "#149"))
        *n = 'o';
      if (! strcmp (entity, "#150"))
        *n = '-';
      if (! strcmp (entity, "#151"))
      {
        *n++ = '-';
        *n = '-';
      } // if
      if (! strcmp (entity, "#156"))
      {
        *n++ = 'o';
        *n   = 'i';
      } // if
      if (! strcmp (entity, "#157"))
        ++n;
      if (! strcmp (entity, "#158"))
        *n = ' ';
      if (! strcmp (entity, "#169"))
      {
        strncpy (n, " copyright", 10);
        n += 9;
      } // if
      if (! strcmp (entity, "#233"))
        *n = '_';
      if (! strcmp (entity, "#243"))
        *n = '_';
      if (! strcmp (entity, "#263"))
        *n = 'c';
      if (! strcmp (entity, "#279"))
        *n = 'e';
      if (! strcmp (entity, "#351"))
        *n = 's';
      if (! strcmp (entity, "#381"))
        *n = 'Z';
      if (! strcmp (entity, "#382"))
        *n = 'z';
      if (! strcmp (entity, "#10140"))
      {
        strncpy (n, "->", 2);
        n++;
      } // if
      if (! strcmp (entity, "#8211"))
        *n = '-';
      if (! strcmp (entity, "#8212"))
      {
        *n++ = '-';
        *n   = '-';
      } // if
      if (! strcmp (entity, "#8216"))
        *n = '`';
      if (! strcmp (entity, "#8217"))
        *n = '\'';
      if (! strcmp (entity, "#8220") || ! strcmp (entity, "#8222"))
        *n = '"';
      if (! strcmp (entity, "#8221"))
        *n = '"';
      if (! strcmp (entity, "#8230"))
      {
        strncpy (n, "...", 3);
        n += 2;
      } // if
      if (! strcmp (entity, "#61485"))
        *n = '-';
      if (! strcmp (entity, "#61607"))
        *n = ' ';
      if (*n == 'X')
      {
        char *x = orig;
        while (*x)
        {
          if (*x == '"')
            *x = '\'';
          x++;
        } // while
        x = entity;
        while (*x)
        {
          if (*x == '"')
            *x = '\'';
          x++;
        } // while
      } // if
      n++;
      if (! *(++s))
        break;
    } // if (*s == '&')
    else
      *n++ = *s++;
if (s - orig > 1020)
  printf (gettext ("s length: %d\n"), s - orig);
if (n - new > 1020)
  printf (gettext ("n length: %d\n"), n - new);
  } // while
  *n = 0;
  strcpy (orig, new);
} // convert_html_entities

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
         playmp3 ("/usr/local/share/daisy-player/error.mp3", NULL, NULL, sound_dev, 0);
         errx (1, "%s", gettext ("Corrupt daisy structure!"));                 
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
            playmp3 ("/usr/local/share/daisy-player/error.mp3", NULL, NULL, sound_dev, 0);
            errx (1, "get_element_or_label: %s", p);
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
         playmp3 ("/usr/local/share/daisy-player/error.mp3", NULL, NULL, sound_dev, 0);
         errx (1, "%s", gettext ("Maybe a read-error occurred!"));
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
      playmp3 ("/usr/local/share/daisy-player/error.mp3", NULL, NULL, sound_dev, 0);
      errx (1, "get_book_page(): %s", file);
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
      if (*label)
      {
         strcpy (current_book_page, label);
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
   strcpy (daisy[current].begin, clip_begin);
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
         daisy[current].end = atoi (ncc_timeInThisSmil) * 3600;
         *(ncc_timeInThisSmil + 5) = 0;
         daisy[current].end += atoi (ncc_timeInThisSmil + 3) * 60;
         *(ncc_timeInThisSmil + 8) = 0;
         daisy[current].end += atoi (ncc_timeInThisSmil + 6);
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

void view_page (int page)
{
   int c = -1, x;

   mvwaddstr (titlewin, 1, 0, "----------------------------------------");
   waddstr (titlewin, "----------------------------------------");
   mvwprintw (titlewin, 1, 0, gettext ("'h' for help "));
   if (! discinfo)
   {
      mvwprintw (titlewin, 1, 28, gettext (" level: %d of %d "), level, depth);
      if (ncc_maxPageNormal)
         mvwprintw (titlewin, 1, 15, gettext (" %d pages "), ncc_maxPageNormal);
      mvwprintw (titlewin, 1, 47,
         gettext (" total length: %s "), ncc_totalTime);
      mvwprintw (titlewin, 1, 74, " %d/%d ", \
              page + 1, daisy[max_items].page + 1);
   } // if
   wrefresh (titlewin);
   wclear (scherm);                                    
   while (daisy[++c].page != page);
   while (daisy[c].page == page && c <= max_items)
   {
      daisy[c].label[66 - daisy[c].x] = 0;
      mvwprintw (scherm, daisy[c].y, daisy[c].x + 1, daisy[c].label);
      if (! discinfo)
      {
         int y, l;

         y = 63;
         if (*daisy[c].pages)
            y = 53;
         l = strlen (daisy[c].label) + daisy[c].x;
         if (l / 2 * 2 == l)
         {
            l++;
            waddstr (scherm, " ");
         } // if
         for (x = l; x <= y; x += 2)
            waddstr (scherm, " .");
         mvwprintw (scherm, daisy[c].y, 57, daisy[c].pages);
         mvwprintw (scherm, daisy[c].y, 74, "%02d:%02d",
                 (int) (daisy[c].end - atof (daisy[c].begin) + .5) / 60, \
                 (int) (daisy[c].end - atof (daisy[c].begin) + .5) % 60);
      } // if
      c++;
   } // while
   if (just_this_item != -1 && daisy[current].page == daisy[playing].page)
      if (playing == -1)
         mvwprintw (scherm, daisy[current].y, 0, "J");
      else
         mvwprintw (scherm, daisy[playing].y, 0, "J");
   else
      mvwprintw (scherm, daisy[playing].y, 0, " ");
   wmove (scherm, daisy[current].y, daisy[current].x);
   wrefresh (scherm);
} // view_page

void remake_structure (int level)
{
   int c, x;

   for (c = 0; c < max_items; c++)
   {
      if (daisy[c + 1].level > level)
      {
         for (x = c + 1; daisy[x].level > level && x < max_items; x++)
         {
            daisy[c].end += daisy[x].end;
         } // for
         c = x - 1;
      } // if
   } // for
} // remake_structure

void read_data_cd (char *html_file, int flag)
{
   int x, screen_y = 0, page = 0, indent = 0, first_span;
   char *p, *str;
   int r;

   if ((r = open (html_file, O_RDONLY)) == -1)
   {
      endwin ();
      playmp3 ("/usr/local/share/daisy-player/error.mp3", NULL, NULL, sound_dev, 0);
      errx (1, gettext ("Corrupt daisy structure %s"), html_file);
   } // if
   current = 0;
   daisy[current].page = 0;
   *from_page = *to_page = 0;
   while (1)
   {
      get_element_or_label (r);
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
      if (strcasecmp (tag, "span") == 0)
      {
         do
         {
            if (get_element_or_label (r) == EOF)
            {
               close (r);
               return;
            } // if
            strcpy (tag, element + 1);
            get_tag ();
            if (*label)
            {
               if (first_span++ == 0)
                  strcpy (from_page, label);
               else
                  strcpy (to_page, label);
            } // if
         } while (strcasecmp (tag, "/span") != 0);
         continue;
      } // if (! strcasecmp (tag, "span"))
      if (! strcasecmp (tag, "h1") ||
          ! strcasecmp (tag, "h2") ||
          ! strcasecmp (tag, "h3") ||
          ! strcasecmp (tag, "h4") ||
          ! strcasecmp (tag, "h5") ||
          ! strcasecmp (tag, "h6"))
      {
         int l;

         l = tag[1] - '0';
         daisy[current].level = l;
         if (l > depth)
            depth = l;
         indent = daisy[current].x = (l - 1) * 3 + 1;
//          if (daisy[current - 1].level > level)
               first_span = 0;
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
         convert_html_entities (label);
         strcpy (daisy[current].label, label);

// add page numbers to previous label
         if (! *to_page)
            strcpy (to_page, from_page);
         if (*from_page)
            sprintf (daisy[current - 1].pages, 
                        " (%3s-%3s)", from_page, to_page);

         if (flag)
         {
            mvwprintw (titlewin, 1, 0, "----------------------------------------");
            wprintw (titlewin, "----------------------------------------");
            mvwprintw (titlewin, 1, 0, gettext ("Reading %s... "),
               daisy[current].label);
            wrefresh (titlewin);
         } // if

         if (screen_y >= max_y)
         {
            screen_y = 0;
            page++;
         } // if
         daisy[current].y = screen_y++;
         if (daisy[current].x == 0)
            daisy[current].x = indent + 3;
         daisy[current].page = page;
         if (! discinfo)
            parse_smil (daisy[current].smil_file);
         current++;
      } // if (! strcasecmp (tag, "a"))
      if (strcasecmp (tag, "/html") == 0)
      {
// add page numbers to previous label
         if (*from_page)
            sprintf (daisy[current - 1].pages,
                              " (%3s-%3s)", from_page, to_page);
         break;
      } // if
   } // while
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
      playmp3 ("/usr/local/share/daisy-player/error.mp3", NULL, NULL, sound_dev, 0);
      errx (1, "fork()");
   case 0:
      player_pid = setsid ();
      break;
   default:
      return;
   } // switch

   char speed_str[100], cmd[100];

   sprintf (cmd, "madplay -Q %s -o %s -s %s -t %f", daisy[current].audio_file,
            "/tmp/daisy_player.wav", clip_begin,
            atof (clip_end) - atof (clip_begin));
   system (cmd);
   sprintf (speed_str, "%f", speed);
   char *argv[] =
   {
      "sox", "-q", "/tmp/daisy_player.wav",
      "-t", "alsa", sound_dev,
      "tempo", "-s", speed_str, NULL
   };
   execv ("/usr/bin/sox", argv);
} // player

void start_playing (char *smil_file, char *anchor)
{
   if (smil_file_fd > -1)
   {
      close (smil_file_fd);
   } // if
   if ((smil_file_fd = open (realname (smil_file), O_RDONLY)) == -1)
   {
      endwin ();
      playmp3 ("/usr/local/share/daisy-player/error.mp3", NULL, NULL, sound_dev, 0);
      errx (1, "start_playing(): %s", realname (smil_file));
   } // if
   *current_book_page = 0;
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
} // start_playing

void store_to_disk ()
{
   int by, bx;
   char str[255];

   getyx (scherm, by, bx);
   wclear (scherm);
   wprintw (scherm,
            "\nStoring \"%s\" as \"%s.wav\" into your home-folder...",
            daisy[current].label, daisy[current].label);
   wrefresh (scherm);
   sprintf (str, "madplay -q %s -o \"%s/%s\".wav",
                 daisy[current].audio_file, getenv ("HOME"),
                 daisy[current].label);
   system (str);
   view_page (cur_page);
   wmove (scherm, by, bx);
   return;
} // store_to_disk

void help ()
{        
   int y, x;

   getyx (scherm, y, x);
   wclear (scherm);
   waddstr (scherm, gettext ("\nThese commands are available in this version:\n"));
   waddstr (scherm, gettext ("=============================================\n\n"));
   waddstr (scherm, gettext ("cursor down     - move cursor to the next item\n"));
   waddstr (scherm, gettext ("cursor up       - move cursor to the previous item\n"));
   waddstr (scherm, gettext ("cursor right    - skip to next phrase\n"));
   waddstr (scherm, gettext ("cursor left     - skip to previous phrase\n"));
   waddstr (scherm, gettext ("page-down       - view next page\n"));
   waddstr (scherm, gettext ("page-up         - view previous page\n"));
   waddstr (scherm, gettext ("enter           - start playing\n"));
   waddstr (scherm, gettext ("space           - pause/resume playing\n"));
   waddstr (scherm, gettext ("home            - play on normal speed\n"));
   waddstr (scherm, "\n");
   waddstr (scherm, gettext ("Press any key for next page..."));
   nodelay (scherm, FALSE);
   wgetch (scherm);
   nodelay (scherm, TRUE);
   wclear (scherm);
   waddstr (scherm, gettext ("\n/               - search for a label\n"));
   waddstr (scherm, gettext ("d               - store current item to disk\n"));
   waddstr (scherm, gettext ("D               - decrease playing speed\n"));
   waddstr (scherm, gettext ("f               - find the currently playing item and place the cursor there\n"));
   waddstr (scherm, gettext ("g               - go to page number (if any)\n"));
   waddstr (scherm, gettext ("h or ?          - give this help\n"));
   waddstr (scherm, gettext ("j               - just play current item\n"));
   waddstr (scherm, gettext ("l               - switch to next level\n"));
   waddstr (scherm, gettext ("L               - switch to previous level\n"));
   waddstr (scherm, gettext ("n               - search forwards\n"));
   waddstr (scherm, gettext ("N               - search backwards\n"));
   waddstr (scherm, gettext ("o               - select next output sound device\n"));
   waddstr (scherm, gettext ("p               - place a bookmark\n"));
   waddstr (scherm, gettext ("q               - quit daisy-player and place a bookmark\n"));
   waddstr (scherm, gettext ("s               - stop playing\n"));
   waddstr (scherm, gettext ("U               - increase playing speed\n"));
   waddstr (scherm, gettext ("\nPress any key to leave help..."));
   nodelay (scherm, FALSE);
   wgetch (scherm);
   nodelay (scherm, TRUE);
   view_page (cur_page);
   wmove (scherm, y, x);
} // help

void previous_item ()
{
   if (current == 0)
   {
      beep ();
      return;
   } // if
   while (daisy[--current].level > level);
   cur_page = daisy[current].page;
   view_page (cur_page);
   wmove (scherm, daisy[current].y, daisy[current].x);
} // previous_item

int next_item ()
{
   if (current == max_items)
   {
      beep ();
      return -1;
   } // if
   while (daisy[++current].level > level)
   {
      if (current >= max_items)
      {
         beep ();
         previous_item ();
         return -1;
      } // if
   } // while
   cur_page = daisy[current].page;
   view_page (cur_page);
   wmove (scherm, daisy[current].y, daisy[current].x);
   return 0;
} // next_item

void play_datacd (int smil_file_fd)
{
   if (playing_page == daisy[current].page)
   {
      time_t cur_time, addition;
      int played_time, remain_time;

      wattron (scherm, A_BOLD);                 
      if (*current_book_page)
         mvwprintw (scherm, daisy[playing].y, 59, "%3s", current_book_page);
      cur_time = time (NULL);
      addition = (time_t) (atof (clip_begin) - atof (daisy[playing].begin));
      played_time = cur_time - start_time + addition;
      mvwprintw (scherm, daisy[playing].y, 68, "%02d:%02d", \
                 played_time / 60, played_time % 60);
      remain_time =  daisy[playing].end - played_time;
      mvwprintw (scherm, daisy[playing].y, 74, "%02d:%02d", \
                 remain_time / 60, remain_time % 60);
      wattroff (scherm, A_BOLD);
      wmove (scherm, daisy[current].y, daisy[current].x);
      wrefresh (scherm);
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

// next item
   if (++current > max_items)
      exit (0);
   playing = current;
   wmove (scherm, daisy[current].y, daisy[current].x);
   wrefresh (scherm);
   cur_page = playing_page = daisy[playing].page;
   view_page (cur_page);
   if (just_this_item != -1)
   {
      if (level >= daisy[playing].level)
      {
         just_this_item = -1;
         view_page (cur_page);
         playing = -1;
         return;
      } // if
   } // if
   start_playing (daisy[current].smil_file, daisy[current].anchor);
} // play_datacd

void skip_left ()
{
   char last_clip[255];

   if (playing == -1)
   {
      beep ();
      return;
   } // if
   {
      if (smil_file_fd == -1)
         return;
      if (strcmp (clip_begin, daisy[playing].begin) == 0)
      {
// go to previous item
         if (playing == 0)
         {
            beep ();
            return;
         } // if
         kill_player (SIGTERM);
         close (smil_file_fd);
         current = playing;
         while (daisy[--current].level > level);
         playing = current;
         if ((smil_file_fd = open (daisy[current].smil_file, O_RDONLY)) == -1)
         {
            endwin ();
            playmp3 ("/usr/local/share/daisy-player/error.mp3", NULL, NULL, sound_dev, 0);
            errx (1, gettext ("Something is wrong with %s"), daisy[current].smil_file);
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
         if (playing_page != daisy[current].page)
            return;
         wattron (scherm, A_BOLD);
         mvwprintw (scherm, playing_y, 74, "%02d:%02d\n", \
                   (bytes_read / FACTOR) / 60, (bytes_read / FACTOR) % 60);
         wmove (scherm, daisy[current].y, daisy[current].x);
         wrefresh (scherm);
         return;
      } // if
      kill_player (SIGTERM);
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
      if (playing_page != daisy[current].page)
         return;
      wattron (scherm, A_BOLD);
      mvwprintw (scherm, playing_y, 74, "%02d:%02d\n", \
                  (bytes_read / FACTOR) / 60, (bytes_read / FACTOR) % 60);
      wmove (scherm, daisy[current].y, daisy[current].x);
      wrefresh (scherm);
   } // if
} // skip_left

void skip_right ()
{
   if (1)
   {
      kill_player (SIGTERM);
      if (playing_page != daisy[current].page)
         return;
      wattron (scherm, A_BOLD);
      mvwprintw (scherm, playing_y, 74, "%02d:%02d\n", \
                 (bytes_read / FACTOR) / 60, (bytes_read / FACTOR) % 60);
      wmove (scherm, daisy[current].y, daisy[current].x);
      wrefresh (scherm);
   } // if
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
   view_page (cur_page);
   wmove (scherm, daisy[current].y, daisy[current].x);
} // change_level

void read_rc ()
{
   FILE *r;
   char line[255], *p;
   struct passwd *pw = getpwuid (geteuid ());

   chdir (pw->pw_dir);
   strcpy (sound_dev, "hw:0");
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
               strcpy (sound_dev, p);
               sound_dev[4] = 0;
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
               speed = atof (p);
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
   fputs ("# default: sound_dev=hw:0\n", w);
   fprintf (w, "sound_dev=%s\n", sound_dev);
   fputs ("#\n", w);
   fputs ("# At wich speed should the DTB be read?\n", w);
   fputs ("# default: speed=1\n", w);
   fprintf (w, "speed=%f\n", speed);
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
} // quit_daisy_player

void search (int start, char mode)
{
   int c, flag = 0;

   if (playing != -1)
      kill_player (SIGSTOP);
   if (mode == '/')
   {
      mvwaddstr (titlewin, 1, 0, "----------------------------------------");
      waddstr (titlewin, "----------------------------------------");
      mvwprintw (titlewin, 1, 0, gettext ("What do you search?                           "));
      echo ();
      wmove (titlewin, 1, 20);
      wgetnstr (titlewin, search_str, 25);
      noecho ();
   } // if
   if (mode == '/' || mode == 'n')
      for (c = start; c <= max_items; c++)
      {
         if (strcasestr (daisy[c].label, search_str) != NULL)
         {
            flag = 1;
            break;
         } // if
      } // for
   else
      for (c = start; c >= 0; c--)
      {
         if (strcasestr (daisy[c].label, search_str) != NULL)
         {
            flag = 1;
            break;
         } // if
      } // for
   if (flag)
   {
      current = c;
      cur_page = daisy[current].page;
   }
   else
      beep ();
   view_page (cur_page);
   if (playing != -1)
      kill_player (SIGCONT);
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
   char *p, filename[255], anchor[255];

   kill_player (SIGTERM);
   mvwaddstr (titlewin, 1, 0, "----------------------------------------");
   waddstr (titlewin, "----------------------------------------");
   mvwprintw (titlewin, 1, 0, gettext ("Go to page number:        "));
   echo ();
   wmove (titlewin, 1, 19);
   wgetnstr (titlewin, page_number, 5);
   noecho ();
   view_page (cur_page);
   if (! *page_number)
   {
      beep ();
      return;
   } // if
   if ((fd = open (NCC_HTML, O_RDONLY)) == -1)
   {
      endwin ();
      playmp3 ("/usr/local/share/daisy-player/error.mp3", NULL, NULL, sound_dev, 0);
      errx (1, gettext ("Something is wrong with the %s file"), NCC_HTML);
   } // if
   do
   {
      if (get_element_or_label (fd) == EOF)
      {
         beep ();
         return;
      } // if
      strcpy (tag, element + 1);
      get_tag ();
      if (strcasecmp (tag, "span") != 0)
         continue;
      do
      {
         if (get_element_or_label (fd) == EOF)
         {
            beep ();
            return;
         } // if
         strcpy (tag, element + 1);
         get_tag ();
      } while (strcasecmp (tag, "a") != 0);
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
   } while (strcmp (label, page_number) != 0);
   close (fd);

   for (current = 0; current <= max_items; current++)
   {
      if (strcasecmp (daisy[current].smil_file, filename) == 0)
         break;
   } // for
   cur_page = daisy[current].page;
   view_page (cur_page);
   wmove (scherm, daisy[current].y, daisy[current].x);
   playing = current;
   playing_page = daisy[current].page;
   playing_y = daisy[current].y;
   just_this_item = -1;
   start_playing (filename, anchor);
} // go_to_page_number

void select_next_output_device ()
{
   FILE *r;
   int n, bx, by, y;
   char list[10][255], trash[100];

   getyx (scherm, by, bx);
   wclear (scherm);
   wprintw (scherm, "\nSelect an soundcard:\n\n");
   if (! (r = fopen ("/proc/asound/cards", "r")))
   {
      endwin ();
      playmp3 ("/usr/local/share/daisy-player/error.mp3", NULL, NULL, sound_dev, 0);
      errx (1, "%s", gettext ("Cannot read /proc/asound/cards"));
   } // if
   for (n = 0; n < 10; n++)
   {
      *list[n] = 0;
      fgets (list[n], 100, r);
      fgets (trash, 100, r);
      if (! *list[n])
         break;
      wprintw (scherm, "   %s", list[n]);
   } // for
   fclose (r);
   y = 3;
   nodelay (scherm, FALSE);
   for (;;)
   {
      wmove (scherm, y, 2);
      switch (wgetch (scherm))
      {
      case 13:
         sprintf (sound_dev, "hw:%i", y - 3);
         view_page (cur_page);
         nodelay (scherm, TRUE);
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
         view_page (cur_page);
         nodelay (scherm, TRUE);
         return;
      } // switch
   } // for
} // select_next_output_device

void browse ()
{
   char str[255];

   current = 0;
   just_this_item = -1;
   playing = -1;
   playing_page = 1;
   playing_y = -1;
   cur_page = 0;
   smil_file_fd = -1;
   view_page (cur_page);
   wmove (scherm, daisy[0].y, daisy[0].x);
   nodelay (scherm, TRUE);
   get_bookmark ();
   for (;;)
   {
      signal (SIGCHLD, player_ended);
      switch (wgetch (scherm))
      {
      case 13:
         just_this_item = -1;
         view_page (cur_page);
         if (discinfo)
         {
            sprintf (str, "%s -m %s -d %s", prog_name,
                   dirname (daisy[current].smil_file), sound_dev);
            system (str);
            view_page (cur_page);
            break;
         } // if
         playing = current;
         playing_page = daisy[current].page;
         playing_y = daisy[current].y;
         kill_player (SIGTERM);
         start_playing (daisy[current].smil_file, daisy[current].anchor);
         break;
      case '/':
         search (current + 1, '/');
         break;
      case ' ':
         if (playing != -1)
            playing = -1;
         else
            playing = current;
         if (smil_file_fd == -1)
            break;
         else
         {
            if (playing != -1)
               kill_player (SIGCONT);
            else
               kill_player (SIGSTOP);
         } // if
         break;
      case 'd':
         if (playing != -1)
            kill_player (SIGSTOP);
         store_to_disk ();
         if (playing != -1)
            kill_player (SIGCONT);
         break;
      case 'e':
         if (! discinfo && multi)
         {
            beep ();
            break;
         } // if
         quit_daisy_player ();
         system ("udisks --unmount /dev/cdrom > /dev/null");
         cdio_eject_media_drive ("/dev/cdrom");
         exit (0);
      case 'f':
         if (playing == -1)
         {
            beep ();
            break;
         } // if
         for (current = 0; current != playing; current++);
         cur_page = playing_page = daisy[current].page;
         playing_y = daisy[current].y;
         view_page (cur_page);
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
         if (just_this_item == -1)
         {
            if (playing != -1)
               just_this_item = daisy[playing].level;
            else
               just_this_item = daisy[current].level;
         }
         else
            just_this_item = -1;
         if (just_this_item != -1)
            if (playing == -1)
               mvwprintw (scherm, daisy[current].y, 0, "J");
            else
               mvwprintw (scherm, daisy[playing].y, 0, "J");
         else  
            mvwprintw (scherm, daisy[playing].y, 0, " ");
         wrefresh (scherm);
         if (playing != -1)
            break;
         playing = current;
         playing_page = daisy[current].page;
         playing_y = daisy[current].y;
         kill_player (SIGTERM);
         start_playing (daisy[current].smil_file, daisy[current].anchor);
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
         view_page (cur_page);
         wmove (scherm, daisy[current].y, daisy[current].x);
         kill_player (SIGTERM);
         break;
      case KEY_DOWN:
         next_item ();
         break;
      case KEY_UP:
         previous_item ();
         break;
      case KEY_RIGHT:
         skip_right ();
         break;
      case KEY_LEFT:
         skip_left ();
         break;
      case KEY_NPAGE:
         if (cur_page == daisy[max_items].page)
         {
            beep ();
            break;
         } // if
         cur_page++;
         current = -1;
         while (daisy[++current].page != cur_page);
         next_item ();
         view_page (cur_page);
         wmove (scherm, daisy[current].y, daisy[current].x);
         break;
      case KEY_PPAGE:
         if (cur_page == 0)
         {
            beep ();
            break;
         } // if
         cur_page--;
         current = -1;
         while (daisy[++current].page != cur_page);
         previous_item ();
         view_page (cur_page);
         wmove (scherm, daisy[current].y, daisy[current].x);
         break;
      case ERR:
         break;
      case 'U':
         speed += 0.1;
         kill_player (SIGTERM);
         replay_playing_clip ();
         break;
      case 'D':
         speed -= 0.1;
         kill_player (SIGTERM);
         replay_playing_clip ();
         break;
      case KEY_HOME:
         speed = 1;
         kill_player (SIGTERM);
         replay_playing_clip ();
         break;
      default:
         beep ();
         break;
      } // switch

      if (playing != -1)
         play_datacd (smil_file_fd);
      usleep (5000);
   } // for
} // browse

void usage (char *argv)
{
   fprintf (stderr, gettext ("Daisy-player - Version %s\n"), VERSION);
   puts ("(C)2003-2011 J. Lemmens");
   fprintf (stderr, gettext ("\nUsage: %s [directory with a Daisy-structure] [-d ALSA sound device]\n"), basename (argv));
   playmp3 ("/usr/local/share/daisy-player/error.mp3", NULL, NULL, sound_dev, 0);
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
      playmp3 ("/usr/local/share/daisy-player/error.mp3", NULL, NULL, sound_dev, 0);
      puts (gettext ("\nCannot read /proc/mounts."));
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

   strcpy (prog_name, basename (argv[0]));
   daysy_player_pid = getpid ();
   speed = 1;
   atexit (quit_daisy_player);
   read_rc ();
   setlocale (LC_ALL, getenv ("LANG"));
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
         if (system ("udisks --mount /dev/cdrom > /dev/null") == -1)
         {
            endwin ();
            playmp3 ("/usr/local/share/daisy-player/error.mp3", NULL, NULL, sound_dev, 0);
            puts (gettext ("\nCannot use udisks command."));
            _exit (1);
         } // if

         // try again to mount one
         if (! get_mount_point ())
         {
            endwin ();
            playmp3 ("/usr/local/share/daisy-player/error.mp3", NULL, NULL, sound_dev, 0);
            puts (gettext ("\nNo daisy-cd in drive."));
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
   scherm = newwin (23, 80, 2, 0);
   getmaxyx (scherm, max_y, max_x);
   max_y--;
   keypad (scherm, TRUE);
   meta (scherm,       TRUE);
   nonl ();
   noecho ();
   player_pid = -2;
   if (chdir (daisy_mp) == -1)
   {
      endwin ();
      playmp3 ("/usr/local/share/daisy-player/error.mp3", NULL, NULL, sound_dev, 0);
      errx (1, gettext ("No such directory: %s"), daisy_mp);
   } // if
   strcpy (NCC_HTML, realname ("ncc.html"));
   if ((r = open (NCC_HTML, O_RDONLY)) == -1)
   {
      endwin ();
      puts ("");
      playmp3 ("/usr/local/share/daisy-player/error.mp3", NULL, NULL, sound_dev, 0);
      err (1, gettext ("Cannot find a daisy structure in %s\n%s"), daisy_mp, NCC_HTML);
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
         playmp3 ("/usr/local/share/daisy-player/error.mp3", NULL, NULL, sound_dev, 0);
         err (1, gettext (CANNOT_FIND_A_DAISY_STRUCTURE), NCC_HTML);
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
   remake_structure (level);
   *search_str = 0;
   browse ();
   return 0;
} // main
