/* header file for daisy-player
 *  Copyright (C)2019 J. Lemmens
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

#include <string.h>
#include <libgen.h>
#include <stdio.h>
#include <unistd.h>
#include <strings.h>
#define __STDC_WANT_IEC_60559_BFP_EXT__
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <ncursesw/curses.h>
#include <fcntl.h>
#include <dirent.h>
#include <pwd.h>
#include <locale.h>
#include <libintl.h>
#include <sox.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <libxml/xmlreader.h>
#include <libxml/xmlwriter.h>
#include <libxml/HTMLparser.h>
#include <cdio/cdio.h>
#include <cdda.h>
#include <paranoia.h>
#include <cdio/disc.h>
#include <magic.h>
#include <fnmatch.h>
#include <sys/select.h>
#include <grp.h>
#include <time.h>

#undef PACKAGE
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_URL
#undef PACKAGE_VERSION
#undef VERSION
#include "config.h"

#define MAX_CMD 512
#define MAX_STR 256
#define MAX_TAG 1024
#define DAISY_PLAYER

typedef struct Daisy
{
   int playorder, x, y, screen;
   float begin, duration;
   char *smil_file, *smil_anchor;
   char *xml_file, *xml_anchor;
   char *clips_file, *clips_anchor, *orig_smil;
   char class[MAX_STR], label[100];
   char first_id[MAX_STR], last_id[MAX_STR];
   int level, page_number;
   char *daisy_mp; // discinfo
   char *filename; // Audio-CD
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
        *id,
        *idref,
        media_type[MAX_STR],
        name[MAX_STR],
        ncc_depth[MAX_STR],
        ncc_maxPageNormal[MAX_STR],
        ncc_totalTime[MAX_STR],
        number[MAX_STR],
        playorder[MAX_STR],
        smilref[MAX_STR],
        *src,
        toc[MAX_STR],
        value[MAX_STR];
} my_attribute_t;

typedef struct Misc
{
   int discinfo, playing, just_this_item, current_page_number;
   int current, max_y, max_x, total_items, level, displaying, ignore_bookmark;
   int items_in_opf, items_in_ncx;
   int depth, total_pages, verbose;
   int pipefd[2], tmp_wav_fd, has_audio_tag;
   int pause_resume_playing, mounted_by_daisy_player;
   int ncx_failed, opf_failed;
   char *pause_resume_id, *prev_id, *current_id;
   char *audio_id;
   float speed, total_time, clip_begin, clip_end;
   long min_vol, max_vol, volume;
   htmlDocPtr doc;
   xmlTextReaderPtr reader;
   pid_t player_pid, cdda_pid, main_pid;
   time_t seconds;
   char ncc_html[MAX_STR], ncc_totalTime[MAX_STR], ocr_language[5];
   char daisy_version[MAX_STR], daisy_title[MAX_STR], daisy_language[MAX_STR];
   char *daisy_mp, *tmp_dir;
   char tag[MAX_TAG], *label;
   int label_len;
   char bookmark_title[MAX_STR], search_str[30];
   char cd_dev[MAX_STR], pulseaudio_device[5];
   char cddb_flag, opf_name[MAX_STR], ncx_name[MAX_STR];
   char *current_audio_file, tmp_wav[MAX_STR + 1], mcn[MAX_STR];
   char xmlversion[MAX_STR + 1];
   char cmd[MAX_CMD + 1], str[MAX_STR + 1];
   time_t elapsed_seconds;
   WINDOW *screenwin, *titlewin;
   cdrom_paranoia_t *par;
   cdrom_drive_t *drv;
   CdIo_t *p_cdio;
   int cd_type;
   lsn_t lsn_cursor, pause_resume_lsn_cursor;
   int term_signaled;
   int use_OPF, use_NCX; // for testing
} misc_t;

extern void get_toc_audiocd (misc_t *, daisy_t *);
extern  daisy_t *get_number_of_tracks (misc_t *);
extern pid_t play_track (misc_t *, char *, char *, lsn_t);
extern void playfile (misc_t *, char *, char *, char *, char *, char *);
extern char *get_mcn (misc_t *);
extern void quit_daisy_player (misc_t *, my_attribute_t *, daisy_t *);
extern void view_screen (misc_t *, daisy_t *);
extern void pause_resume (misc_t *, my_attribute_t *, daisy_t *);
extern void open_clips_file (misc_t *, my_attribute_t *, char *, char *);
extern void open_xml_file (misc_t *, my_attribute_t *,
                           daisy_t *, char *, char *);
extern void get_next_clips (misc_t *, my_attribute_t *, daisy_t *);
extern void free_all (misc_t *, my_attribute_t *, daisy_t *);
extern void select_next_output_device (misc_t *, daisy_t *);
extern void go_to_page_number (misc_t *, my_attribute_t *, daisy_t *);
extern void make_tmp_dir (misc_t *);
extern daisy_t *create_daisy_struct (misc_t *, my_attribute_t *, daisy_t *);
extern void skip_right (misc_t *, daisy_t *);
extern void player_ended ();
extern char *convert_URL_name (misc_t *, char *);
extern void get_realpath_name (char *, char *, char *);
extern void failure (misc_t *, char *, int);
extern int get_tag_or_label (misc_t *, my_attribute_t *, xmlTextReaderPtr);
extern void get_clips (misc_t *, my_attribute_t *);
extern void fill_daisy_struct_2 (misc_t *, my_attribute_t *, daisy_t *);
extern void fill_item (misc_t *, daisy_t *);
extern int get_page_number_2 (misc_t *, my_attribute_t *,
                              daisy_t *daisy, char *);
extern void read_daisy_3 (misc_t *, my_attribute_t *, daisy_t *);
extern void fill_page_numbers (misc_t *, daisy_t *, my_attribute_t *);
extern int get_page_number_3 (misc_t *, my_attribute_t *);
extern int madplay (char *, char *, char *, char *);
extern void kill_player (misc_t *);
extern void remove_tmp_dir (misc_t *);
extern void init_paranoia (misc_t *);
extern void parse_page_number (misc_t *, my_attribute_t *, xmlTextReaderPtr);
extern char *get_dir_content (misc_t *, char *, char *);
extern void get_cddb_info (misc_t *, daisy_t *);
extern void find_index_names (misc_t *);
extern int handle_ncc_html (misc_t *, my_attribute_t *, daisy_t *);
extern int namefilter (const struct dirent *);
extern int get_meta_attributes (xmlTextReaderPtr, xmlTextWriterPtr);
extern void create_ncc_html (misc_t *);
extern void get_attributes (misc_t *, my_attribute_t *, xmlTextReaderPtr);
extern char *pactl (char *, char *, char *);
extern void parse_smil_2 (misc_t *, my_attribute_t *, daisy_t *);
extern void get_label_opf (misc_t *, my_attribute_t *, daisy_t *, int);
extern void parse_manifest (misc_t *, my_attribute_t *, daisy_t *,
                            int, char *);
extern void get_label_2 (misc_t *, daisy_t *, int, int);
extern void fill_smil_anchor_ncx (misc_t *, my_attribute_t *, daisy_t *);
extern void parse_content_ncx (misc_t *, my_attribute_t *, daisy_t *);
extern void put_bookmark (misc_t *);
extern float read_time (char *);
extern void parse_clips_opf (misc_t *, my_attribute_t *, daisy_t *, int);
extern void parse_smil_opf (misc_t *, my_attribute_t *, daisy_t *, int);
extern void fill_smil_anchor_opf (misc_t *, my_attribute_t *, daisy_t *);
extern void parse_opf (misc_t *, my_attribute_t *, daisy_t *);
extern void parse_ncx (misc_t *, my_attribute_t *, daisy_t *);
extern void get_bookmark (misc_t *, my_attribute_t *, daisy_t *);
extern void view_page (misc_t *, daisy_t *);
extern void view_time (misc_t *, daisy_t *);
extern void start_playing (misc_t *, daisy_t *);
extern void write_wav (misc_t *, my_attribute_t *, daisy_t *, char *);
extern void store_to_disk (misc_t *, my_attribute_t *, daisy_t *);
extern void help (misc_t *, my_attribute_t *, daisy_t *);
extern void previous_item (misc_t *, daisy_t *);
extern void next_item (misc_t *, daisy_t *);
extern void calculate_times_3 (misc_t *, my_attribute_t *, daisy_t *);
extern void load_xml (misc_t *, my_attribute_t *);
extern void save_xml (misc_t *);
extern void search (misc_t *, my_attribute_t *, daisy_t *, int, char);
extern void change_level (misc_t *, my_attribute_t *, daisy_t *, char);
extern void go_to_time (misc_t *, daisy_t *, my_attribute_t *);
extern void skip_left (misc_t *, my_attribute_t *, daisy_t *);
extern void browse (misc_t *, my_attribute_t *, daisy_t *, char *);
extern void usage (int);
extern char *get_mount_point (misc_t *);
extern void handle_discinfo (misc_t *, my_attribute_t *, daisy_t *, char *);
extern void reset_term_signal_handlers_after_fork (void);
extern void parse_spine (misc_t *, my_attribute_t *, daisy_t *);
