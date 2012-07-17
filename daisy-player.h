/* header file for daisy-player and eBook-speaker
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

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h> 
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <ncursesw/curses.h>
#include <fcntl.h>
#include <libgen.h>
#include <dirent.h>
#include <pwd.h>
#include <locale.h>
#include <libintl.h>
#include <stringprep.h>
#include <sox.h>
#include <errno.h>
#include <time.h>
#include <libxml/xmlreader.h>
#include <sys/mount.h>

typedef struct daisy
{
   int playorder, x, y, screen;
   float begin, duration;
   char smil_file[255], anchor[255], label[255], class[100];
   int level, page_number;
   char daisy_mp[100]; // discinfo
} daisy_t;

typedef struct my_attribute
{
   char class[100],
        clip_begin[100],
        clip_end[100],
        content[100],
        dc_format[100],
        dc_title[100],
        dtb_depth[100],
        dtb_totalPageCount[100],
        href[255],
        http_equiv[100],
        id[100],
        idref[100],
        media_type[100],
        name[100],
        ncc_depth[100],
        ncc_maxPageNormal[100],
        ncc_totalTime[100],
        playorder[100],
        src[255],
        smilref[255],
        value[100];
} my_attribute_t;

void playfile (char *, char *);
float read_time (char *);
void get_clips ();
void put_bookmark ();
void get_bookmark ();
void get_tag ();
void get_page_number ();
void view_screen ();
void get_label (int, int);
void player_ended ();
void play_now ();
void open_text_file (char *, char *);
void pause_resume ();
void write_wav (char *);
void help ();
void previous_item ();
void next_item ();
void skip_left ();
void skip_right ();
void change_level (char);
void read_rc ();
void save_rc ();
void quit_eBook_reader ();
void search (int , char);
void kill_player ();
void go_to_page_number ();
void select_next_output_device ();
void browse ();
void usage (char *);
char *sort_by_playorder ();
void read_out_eBook (const char *);
const char *read_eBook (char *);
void get_eBook_struct (int);
void read_daisy_3 (int, char *);
void parse_smil ();
void start_element (void *, const char *, const char **);
void end_element (void *, const char *);
