/* header file for daisy-player
 *  Copyright (C)2015 J. Lemmens
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
#include <string.h>
#include <unistd.h>
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
#include <sox.h>
#include <errno.h>
#include <time.h>
#include <sys/ioctl.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlwriter.h>
#include <libxml/HTMLparser.h>
#include <cdio/cdio.h>
#include <cdio/cdda.h>
#include <cdio/paranoia.h>
#include <cdio/disc.h>
#include <magic.h>

#undef PACKAGE
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_URL
#undef PACKAGE_VERSION
#undef VERSION
#include "config.h"

#define MAX_PHRASE_LEN 2000
#define MAX_CMD 512
#define MAX_STR 256
#define MAX_TAG 1024
#define htmlParserOptions HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | \
        HTML_PARSE_NOWARNING | HTML_PARSE_NOBLANKS

typedef struct Daisy
{
   int playorder, x, y, screen;
   float begin, duration;
   char smil_file[MAX_STR], anchor[MAX_STR], class[MAX_STR];
   char label[MAX_PHRASE_LEN];
   int level, page_number;
   char daisy_mp[MAX_STR]; // discinfo
   char filename[MAX_STR]; // Audio-CD
   lsn_t first_lsn, last_lsn;
} daisy_t;

typedef struct My_attribute
{
   char class[MAX_STR],
        clip_begin[MAX_STR],
        clip_end[MAX_STR],
        content[MAX_STR],
        dc_format[MAX_STR],
        dc_title[MAX_STR],
        dtb_depth[MAX_STR],
        dtb_totalPageCount[MAX_STR],
        href[MAX_STR],
        http_equiv[MAX_STR],
        id[MAX_STR],
        idref[MAX_STR],
        media_type[MAX_STR],
        name[MAX_STR],
        ncc_depth[MAX_STR],
        ncc_maxPageNormal[MAX_STR],
        ncc_totalTime[MAX_STR],
        number[MAX_STR],
        playorder[MAX_STR],
        src[MAX_STR],
        smilref[MAX_STR],
        toc[MAX_STR],
        value[MAX_STR];
} my_attribute_t;

typedef struct Misc
{
   int discinfo, playing, just_this_item, current_page_number;
   int current, max_y, max_x, total_items, level, displaying, ignore_bookmark;
   int phrase_nr, tts_no, depth, total_phrases, total_pages;
   int pipefd[2], tmp_wav_fd;
   int pause_resume_playing;
   float pause_resume_clip_begin;
   float speed, total_time;
   float clip_begin, clip_end, start_time;
   xmlTextReaderPtr reader;
   pid_t daisy_player_pid, player_pid;
   char NCC_HTML[MAX_STR], ncc_totalTime[MAX_STR];
   char daisy_version[MAX_STR], daisy_title[MAX_STR], daisy_language[MAX_STR];
   char daisy_mp[MAX_STR];
   char tag[MAX_TAG];
   char label[MAX_PHRASE_LEN];
   char bookmark_title[MAX_STR];
   char search_str[MAX_STR];
   char *wd, cd_dev[MAX_STR], sound_dev[MAX_STR];
   char cddb_flag, opf_name[MAX_STR], ncx_name[MAX_STR];
   char current_audio_file[MAX_STR], tmp_wav[MAX_STR + 1], mcn[MAX_STR];
   time_t seconds;
   WINDOW *screenwin, *titlewin;
   cdrom_paranoia_t *par;
   cdrom_drive_t *drv;
   CdIo_t *p_cdio;
   int cd_type;
   lsn_t lsn_cursor, pause_resume_lsn_cursor;
} misc_t;

extern void save_rc (misc_t *);
extern char *get_mcn (misc_t *);
extern int get_tag_or_label (misc_t *, my_attribute_t *,
                                    xmlTextReaderPtr);
extern void get_label (misc_t *, daisy_t *, int);
extern void parse_ncx (misc_t *, my_attribute_t *, daisy_t *, char *);
extern void usage ();
extern pid_t play_track (misc_t *, char *, char *, lsn_t);
extern void get_clips (misc_t *, char *, char *);
extern void open_smil_file (misc_t *, my_attribute_t *, char *, char *);
extern void play_now (misc_t *misc, my_attribute_t *my_attribute,
                      daisy_t *daisy);
extern void get_page_number_3 (misc_t *, my_attribute_t *);
extern void skip_left (misc_t *, my_attribute_t *, daisy_t *);
extern void skip_right (misc_t *);
extern void set_drive_speed (misc_t *, int);
extern void quit_daisy_player (misc_t *);
extern void read_daisy_3 (misc_t *, my_attribute_t *, daisy_t *);
void parse_smil_3 (misc_t *, my_attribute_t *, daisy_t *);
extern float read_time (char *);
extern void init_paranoia (misc_t *);
extern void get_toc_audiocd (misc_t *, daisy_t *);
extern void playfile (misc_t *, char *, char *, char *, char *, char *);
extern void view_screen (misc_t *, daisy_t *);
extern daisy_t *create_daisy_struct (misc_t *, my_attribute_t *);
extern daisy_t *get_number_of_tracks (misc_t *);
extern void failure (char *, int);
extern void put_bookmark (misc_t *);
