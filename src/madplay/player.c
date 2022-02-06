/*
 * madplay - MPEG audio decoder and player
 * Copyright (C) 2000-2004 Robert Leslie
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * $Id: player.c,v 1.69 2004/02/23 21:34:53 rob Exp $
 */

#include <errno.h>
# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include "global.h"

# include <stdio.h>
# include <stdarg.h>
# include <stdlib.h>

# ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
# endif

# include <sys/stat.h>

# ifdef HAVE_FCNTL_H
#  include <fcntl.h>
# endif

# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# endif

# include <string.h>

# ifdef HAVE_ERRNO_H
#  include <errno.h>
# endif

# include <time.h>
# include <locale.h>
# include <math.h>

# ifdef HAVE_TERMIOS_H
#  include <termios.h>
# endif

# ifdef _WIN32
#  include <windows.h>
# endif

# include <signal.h>

# ifdef HAVE_ASSERT_H
#  include <assert.h>
# endif

# if defined(HAVE_MMAP)
#  include <sys/mman.h>
# endif

# if !defined(O_BINARY)
#  define O_BINARY  0
# endif

# include <mad.h>
# include <id3tag.h>

# include "gettext.h"

# include "player.h"
# include "audio.h"
# include "resample.h"
# include "filter.h"
# include "tag.h"
# include "rgain.h"

# define MPEG_BUFSZ	40000	/* 2.5 s at 128 kbps; 1 s at 320 kbps */
# define FREQ_TOLERANCE	2	/* percent sampling frequency tolerance */

# define TTY_DEVICE	"/dev/tty"

# define KEY_CTRL(key)	((key) & 0x1f)

enum {
  KEY_PAUSE    = 'p',
  KEY_STOP     = 's',
  KEY_FORWARD  = 'f',
  KEY_BACK     = 'b',
  KEY_TIME     = 't',
  KEY_QUIT     = 'q',
  KEY_INFO     = 'i',
  KEY_GAINDECR = '-',
  KEY_GAININCR = '+',
  KEY_GAINZERO = '_',
  KEY_GAININFO = '='
};

/*
 * NAME:	player_init()
 * DESCRIPTION:	initialize player structure
 */
void player_init(struct player *player)
{
  player->verbosity = 0;

  player->options = 0;
  player->repeat  = 1;
  player->control = PLAYER_CONTROL_DEFAULT;
  player->global_start = mad_timer_zero;
  player->global_stop  = mad_timer_zero;

  player->fade_in      = mad_timer_zero;
  player->fade_out     = mad_timer_zero;
  player->gap          = mad_timer_zero;

  player->input.path   = 0;
  player->input.fd     = -1;
# if defined(HAVE_MMAP)
  player->input.fdm    = 0;
# endif
  player->input.data   = 0;
  player->input.length = 0;
  player->input.eof    = 0;

  tag_init(&player->input.tag);

  player->output.mode          = AUDIO_MODE_DITHER;
  player->output.voladj_db     = 0;
  player->output.attamp_db     = 0;
  player->output.gain          = MAD_F_ONE;
  player->output.replay_gain   = 0;
  player->output.filters       = 0;
  player->output.channels_in   = 0;
  player->output.channels_out  = 0;
  player->output.select        = PLAYER_CHANNEL_DEFAULT;
  player->output.speed_in      = 0;
  player->output.speed_out     = 0;
  player->output.speed_request = 0;
  player->output.precision_in  = 0;
  player->output.precision_out = 0;
  player->output.path          = 0;
  player->output.command       = 0;
  /* player->output.resample */
  player->output.resampled     = 0;
  player->stats.show                  = STATS_SHOW_OVERALL;
  player->stats.label                 = 0;
  player->stats.total_bytes           = 0;
  player->stats.total_time            = mad_timer_zero;
  player->stats.global_timer          = mad_timer_zero;
  player->stats.absolute_timer        = mad_timer_zero;
  player->stats.play_timer            = mad_timer_zero;
  player->stats.global_framecount     = 0;
  player->stats.absolute_framecount   = 0;
  player->stats.play_framecount       = 0;
  player->stats.error_frame           = -1;
  player->stats.mute_frame            = 0;
  player->stats.vbr                   = 0;
  player->stats.bitrate               = 0;
  player->stats.vbr_frames            = 0;
  player->stats.vbr_rate              = 0;
  player->stats.nsecs                 = 0;
  player->stats.audio.clipped_samples = 0;
  player->stats.audio.peak_clipping   = 0;
  player->stats.audio.peak_sample     = 0;
}

/*
 * NAME:	player_finish()
 * DESCRIPTION:	destroy a player structure
 */
void player_finish(struct player *player)
{
  if (player->output.filters)
    filter_free(player->output.filters);

  if (player->output.resampled) {
    resample_finish(&player->output.resample[0]);
    resample_finish(&player->output.resample[1]);

    free(player->output.resampled);
    player->output.resampled = 0;
  }
}

/*
 * NAME:	error()
 * DESCRIPTION:	show an error using proper interaction with message()
 */
static
void error(char const *id, char const *format, ...)
{
  int err;
  va_list args;

  err = errno;

  if (id)
    fprintf(stderr, "%s: ", id);

  va_start(args, format);

  if (*format == ':') {
    if (format[1] == 0) {
      format = va_arg(args, char const *);
      errno = err;
      perror(format);
    }
    else {
      errno = err;
      perror(format + 1);
    }
  }
  else {
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
  }

  va_end(args);
}

# if defined(HAVE_MMAP)
/*
 * NAME:	map_file()
 * DESCRIPTION:	map the contents of a file into memory
 */
static
void *map_file(int fd, unsigned long length)
{
  void *fdm;

  fdm = mmap(0, length, PROT_READ, MAP_SHARED, fd, 0);
  if (fdm == MAP_FAILED)
    return 0;

# if defined(HAVE_MADVISE)
  madvise(fdm, length, MADV_SEQUENTIAL);
# endif

  return fdm;
}

/*
 * NAME:	unmap_file()
 * DESCRIPTION:	undo a file mapping
 */
static
int unmap_file(void *fdm, unsigned long length)
{
  if (munmap(fdm, length) == -1)
    return -1;

  return 0;
}

/*
 * NAME:	decode->input_mmap()
 * DESCRIPTION:	(re)fill decoder input buffer from a memory map
 */
static
enum mad_flow decode_input_mmap(void *data, struct mad_stream *stream)
{
  struct player *player = data;
  struct input *input = &player->input;

  if (input->eof)
    return MAD_FLOW_STOP;

  if (stream->next_frame) {
    struct stat stat;
    unsigned long posn, left;

    if (fstat(input->fd, &stat) == -1)
      return MAD_FLOW_BREAK;

    posn = stream->next_frame - input->fdm;

    /* check for file size change and update map */

    if (stat.st_size > input->length) {
      if (unmap_file(input->fdm, input->length) == -1) {
	input->fdm  = 0;
	input->data = 0;
	return MAD_FLOW_BREAK;
      }

      player->stats.total_bytes += stat.st_size - input->length;

      input->length = stat.st_size;

      input->fdm = map_file(input->fd, input->length);
      if (input->fdm == 0) {
	input->data = 0;
	return MAD_FLOW_BREAK;
      }

      mad_stream_buffer(stream, input->fdm + posn, input->length - posn);

      return MAD_FLOW_CONTINUE;
    }

    /* end of memory map; append MAD_BUFFER_GUARD zero bytes */

    left = input->length - posn;

    input->data = malloc(left + MAD_BUFFER_GUARD);
    if (input->data == 0)
      return MAD_FLOW_BREAK;

    input->eof = 1;

    memcpy(input->data, input->fdm + posn, left);
    memset(input->data + left, 0, MAD_BUFFER_GUARD);

    mad_stream_buffer(stream, input->data, left + MAD_BUFFER_GUARD);

    return MAD_FLOW_CONTINUE;
  }

  /* first call */

  mad_stream_buffer(stream, input->fdm, input->length);

  return MAD_FLOW_CONTINUE;
}
# endif

/*
 * NAME:	decode->input_read()
 * DESCRIPTION:	(re)fill decoder input buffer by reading a file descriptor
 */
static
enum mad_flow decode_input_read(void *data, struct mad_stream *stream)
{
  struct player *player = data;
  struct input *input = &player->input;
  int len;

  if (input->eof)
    return MAD_FLOW_STOP;

  if (stream->next_frame) {
    memmove(input->data, stream->next_frame,
	    input->length = &input->data[input->length] - stream->next_frame);
  }

  do {
    len = read(input->fd, input->data + input->length,
	       MPEG_BUFSZ - input->length);
  }
  while (len == -1 && errno == EINTR);

  if (len == -1) {
    error("input", ":read");
    return MAD_FLOW_BREAK;
  }
  else if (len == 0) {
    input->eof = 1;

    assert(MPEG_BUFSZ - input->length >= MAD_BUFFER_GUARD);

    while (len < MAD_BUFFER_GUARD)
      input->data[input->length + len++] = 0;
  }

  mad_stream_buffer(stream, input->data, input->length += len);

  return MAD_FLOW_CONTINUE;
}

/*
 * NAME:	decode->header()
 * DESCRIPTION:	decide whether to continue decoding based on header
 */
static
enum mad_flow decode_header(void *data, struct mad_header const *header)
{
  struct player *player = data;

  if ((player->options & PLAYER_OPTION_TIMED) &&
      mad_timer_compare(player->stats.global_timer, player->global_stop) > 0)
    return MAD_FLOW_STOP;

  /* accounting (except first frame) */

  if (player->stats.absolute_framecount) {
    ++player->stats.absolute_framecount;
    mad_timer_add(&player->stats.absolute_timer, header->duration);

    ++player->stats.global_framecount;
    mad_timer_add(&player->stats.global_timer, header->duration);

    if ((player->options & PLAYER_OPTION_SKIP) &&
	mad_timer_compare(player->stats.global_timer,
			  player->global_start) < 0)
      return MAD_FLOW_IGNORE;
  }

  return MAD_FLOW_CONTINUE;
}

/*
 * NAME:	set_gain()
 * DESCRIPTION:	modify player gain information
 */
static
double set_gain(struct player *player, double db)
{
  db = player->output.voladj_db + player->output.attamp_db;
  if (db > DB_MAX || db < DB_MIN) {
    db = (db > DB_MAX) ? DB_MAX : DB_MIN;
    player->output.attamp_db = db - player->output.voladj_db;
  }

  player->output.gain = db ? mad_f_tofixed(pow(10, db / 20)) : MAD_F_ONE;

  return db;
}

/*
 * NAME:	use_rgain()
 * DESCRIPTION:	select and employ a Replay Gain volume adjustment
 */
static
void use_rgain(struct player *player, struct rgain *list)
{
  struct rgain *rgain = &list[0];

  if ((player->output.replay_gain & PLAYER_RGAIN_AUDIOPHILE) &&
      list[1].name == RGAIN_NAME_AUDIOPHILE &&
      list[1].originator != RGAIN_ORIGINATOR_UNSPECIFIED)
    rgain = &list[1];

  if (RGAIN_VALID(rgain)) {

    player->output.replay_gain |= PLAYER_RGAIN_SET;
  }
}

/*
 * NAME:	decode->filter()
 * DESCRIPTION:	perform filtering on decoded frame
 */
static
enum mad_flow decode_filter(void *data, struct mad_stream const *stream,
			    struct mad_frame *frame)
{
  struct player *player = data;

  /* first frame accounting */

  if (player->stats.absolute_framecount == 0) {
    if (player->input.tag.flags == 0 &&
	tag_parse(&player->input.tag, stream) == 0) {
      struct tag *tag = &player->input.tag;
      unsigned int frame_size;

      if ((tag->flags & TAG_LAME) &&
          (player->output.replay_gain & PLAYER_RGAIN_ENABLED) &&
	  ! (player->output.replay_gain & PLAYER_RGAIN_SET))
        use_rgain(player, tag->lame.replay_gain);
      if ((tag->flags & TAG_XING) &&
	  (tag->xing.flags & TAG_XING_FRAMES)) {
	player->stats.total_time = frame->header.duration;
	mad_timer_multiply(&player->stats.total_time, tag->xing.frames);
      }

      /* total stream byte size adjustment */

      frame_size = stream->next_frame - stream->this_frame;

      if (player->stats.total_bytes == 0) {
	if ((tag->flags & TAG_XING) && (tag->xing.flags & TAG_XING_BYTES) &&
	    tag->xing.bytes > frame_size)
	  player->stats.total_bytes = tag->xing.bytes - frame_size;
      }
      else if (player->stats.total_bytes >=
            (unsigned long) (stream->next_frame - stream->this_frame))
  	player->stats.total_bytes -= frame_size;

      return (player->options & PLAYER_OPTION_SHOWTAGSONLY) ?
	MAD_FLOW_STOP : MAD_FLOW_IGNORE;
    }
    else if (player->options & PLAYER_OPTION_SHOWTAGSONLY)
      return MAD_FLOW_STOP;

    ++player->stats.absolute_framecount;
    mad_timer_add(&player->stats.absolute_timer, frame->header.duration);

    ++player->stats.global_framecount;
    mad_timer_add(&player->stats.global_timer, frame->header.duration);

    if ((player->options & PLAYER_OPTION_SKIP) &&
	mad_timer_compare(player->stats.global_timer,
			  player->global_start) < 0)
      return MAD_FLOW_IGNORE;
  }

  /* run the filter chain */

  return filter_run(player->output.filters, frame);
}


/*
 * NAME:	decode->output()
 * DESCRIPTION: configure audio module and output decoded samples
 */
static
enum mad_flow decode_output(void *data, struct mad_header const *header,
			    struct mad_pcm *pcm)
{
  struct player *player = data;
  struct output *output = &player->output;
  mad_fixed_t const *ch1, *ch2;
  unsigned int nchannels;
  union audio_control control;

  ch1 = pcm->samples[0];
  ch2 = pcm->samples[1];

  switch (nchannels = pcm->channels) {
  case 1:
    ch2 = 0;
    if (output->select == PLAYER_CHANNEL_STEREO) {
      ch2 = ch1;
      nchannels = 2;
    }
    break;

  case 2:
    switch (output->select) {
    case PLAYER_CHANNEL_RIGHT:
      ch1 = ch2;
      /* fall through */

    case PLAYER_CHANNEL_LEFT:
      ch2 = 0;
      nchannels = 1;
      /* fall through */

    case PLAYER_CHANNEL_STEREO:
      break;

    default:
      if (header->mode == MAD_MODE_DUAL_CHANNEL) {
	if (output->select == PLAYER_CHANNEL_DEFAULT) {
	  if (player->verbosity >= -1) {
	    error("output",
		  _("no channel selected for dual channel; using first"));
	  }

	  output->select = -PLAYER_CHANNEL_LEFT;
	}

	ch2 = 0;
	nchannels = 1;
      }
    }
  }

  if (output->channels_in != nchannels ||
      output->speed_in != pcm->samplerate) {
    unsigned int speed_request;

    if (player->verbosity >= 1 &&
	pcm->samplerate != header->samplerate) {
      error("output", _("decoded sample frequency %u Hz"),
	    pcm->samplerate);
    }

    speed_request = output->speed_request ?
      output->speed_request : pcm->samplerate;

    audio_control_init(&control, AUDIO_COMMAND_CONFIG);

    control.config.channels  = nchannels;
    control.config.speed     = speed_request;
    control.config.precision = output->precision_in;

    if (output->command(&control) == -1) {
      error("output", audio_error);
      return MAD_FLOW_BREAK;
    }

    output->channels_in  = nchannels;
    output->speed_in     = pcm->samplerate;

    output->channels_out  = control.config.channels;
    output->speed_out     = control.config.speed;
    output->precision_out = control.config.precision;

    if (player->verbosity >= -1 &&
	output->channels_in != output->channels_out) {
      if (output->channels_in == 1)
	error("output", _("mono output not available; forcing stereo"));
      else {
	error("output", _("stereo output not available; using first channel "
			  "(use -m to mix)"));
      }
    }

    if (player->verbosity >= -1 &&
	output->precision_in &&
	output->precision_in != output->precision_out) {
      error("output", _("bit depth %u not available; using %u"),
	    output->precision_in, output->precision_out);
    }

    if (player->verbosity >= -1 &&
	speed_request != output->speed_out) {
      error("output", _("sample frequency %u Hz not available; using %u Hz"),
	    speed_request, output->speed_out);
    }

    /* check whether resampling is necessary */
    if (abs(output->speed_out - output->speed_in) <
	(long) FREQ_TOLERANCE * output->speed_in / 100) {
      if (output->resampled) {
	resample_finish(&output->resample[0]);
	resample_finish(&output->resample[1]);

	free(output->resampled);
	output->resampled = 0;
      }
    }
    else {
      if (output->resampled) {
	resample_finish(&output->resample[0]);
	resample_finish(&output->resample[1]);
      }
      else {
	output->resampled = malloc(sizeof(*output->resampled));
	if (output->resampled == 0) {
	  error("output",
		_("not enough memory to allocate resampling buffer"));

	  output->speed_in = 0;
	  return MAD_FLOW_BREAK;
	}
      }

      if (resample_init(&output->resample[0],
			output->speed_in, output->speed_out) == -1 ||
	  resample_init(&output->resample[1],
			output->speed_in, output->speed_out) == -1) {
	error("output", _("cannot resample %u Hz to %u Hz"),
	      output->speed_in, output->speed_out);

	free(output->resampled);
	output->resampled = 0;

	output->speed_in = 0;
	return MAD_FLOW_BREAK;
      }
      else if (player->verbosity >= -1) {
	error("output", _("resampling %u Hz to %u Hz"),
	      output->speed_in, output->speed_out);
      }
    }
  }

  audio_control_init(&control, AUDIO_COMMAND_PLAY);

  if (output->channels_in != output->channels_out)
    ch2 = (output->channels_out == 2) ? ch1 : 0;

  if (output->resampled) {
    control.play.nsamples = resample_block(&output->resample[0],
					   pcm->length, ch1,
					   (*output->resampled)[0]);
    control.play.samples[0] = (*output->resampled)[0];

    if (ch2 == ch1)
      control.play.samples[1] = control.play.samples[0];
    else if (ch2) {
      resample_block(&output->resample[1], pcm->length, ch2,
		     (*output->resampled)[1]);
      control.play.samples[1] = (*output->resampled)[1];
    }
    else
      control.play.samples[1] = 0;
  }
  else {
    control.play.nsamples   = pcm->length;
    control.play.samples[0] = ch1;
    control.play.samples[1] = ch2;
  }

  control.play.mode  = output->mode;
  control.play.stats = &player->stats.audio;

  if (output->command(&control) == -1) {
    error("output", audio_error);
    return MAD_FLOW_BREAK;
  }

  ++player->stats.play_framecount;
  mad_timer_add(&player->stats.play_timer, header->duration);

  return MAD_FLOW_CONTINUE;
}

/*
 * NAME:	decode->error()
 * DESCRIPTION:	handle a decoding error
 */
static
enum mad_flow decode_error(void *data, struct mad_stream *stream,
			   struct mad_frame *frame)
{
  struct player *player = data;

  switch (stream->error) {
  case MAD_ERROR_BADDATAPTR:
    return MAD_FLOW_CONTINUE;

    /* fall through */

  default:
    if (player->verbosity >= -1 &&
	!(player->options & PLAYER_OPTION_SHOWTAGSONLY) &&
	((stream->error == MAD_ERROR_LOSTSYNC && !player->input.eof)
	 || stream->sync) &&
	player->stats.global_framecount != player->stats.error_frame) {
      error("error", _("frame %lu: %s"),
	    player->stats.absolute_framecount, mad_stream_errorstr(stream));
      player->stats.error_frame = player->stats.global_framecount;
    }
  }

  if (stream->error == MAD_ERROR_BADCRC) {
    if (player->stats.global_framecount == player->stats.mute_frame)
      mad_frame_mute(frame);

    player->stats.mute_frame = player->stats.global_framecount + 1;

    return MAD_FLOW_IGNORE;
  }

  return MAD_FLOW_CONTINUE;
}

/*
 * NAME:	decode()
 * DESCRIPTION:	decode and output audio for an open file
 */
static
int decode(struct player *player)
{
  struct stat stat;
  struct mad_decoder decoder;
  int options, result;

  if (fstat(player->input.fd, &stat) == -1) {
    error("decode", ":fstat");
    return -1;
  }

  if (S_ISREG(stat.st_mode))
    player->stats.total_bytes = stat.st_size;

  tag_init(&player->input.tag);

  /* prepare input buffers */

# if defined(HAVE_MMAP)
  if (S_ISREG(stat.st_mode) && stat.st_size > 0) {
    player->input.length = stat.st_size;

    player->input.fdm = map_file(player->input.fd, player->input.length);
    if (player->input.fdm == 0 && player->verbosity >= 0)
      error("decode", ":mmap");

    player->input.data = player->input.fdm;
  }
# endif

  if (player->input.data == 0) {
    player->input.data = malloc(MPEG_BUFSZ);
    if (player->input.data == 0) {
      error("decode", _("not enough memory to allocate input buffer"));
      return -1;
    }

    player->input.length = 0;
  }

  player->input.eof = 0;

  /* reset statistics */
  player->stats.absolute_timer        = mad_timer_zero;
  player->stats.play_timer            = mad_timer_zero;
  player->stats.absolute_framecount   = 0;
  player->stats.play_framecount       = 0;
  player->stats.error_frame           = -1;
  player->stats.vbr                   = 0;
  player->stats.bitrate               = 0;
  player->stats.vbr_frames            = 0;
  player->stats.vbr_rate              = 0;
  player->stats.audio.clipped_samples = 0;
  player->stats.audio.peak_clipping   = 0;
  player->stats.audio.peak_sample     = 0;

  mad_decoder_init(&decoder, player,
# if defined(HAVE_MMAP)
		   player->input.fdm ? decode_input_mmap :
# endif
		   decode_input_read,
		   decode_header, decode_filter,
		   player->output.command ? decode_output : 0,
		   decode_error, 0);

  options = 0;
  if (player->options & PLAYER_OPTION_DOWNSAMPLE)
    options |= MAD_OPTION_HALFSAMPLERATE;
  if (player->options & PLAYER_OPTION_IGNORECRC)
    options |= MAD_OPTION_IGNORECRC;

  mad_decoder_options(&decoder, options);

  result = mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);

  mad_decoder_finish(&decoder);

# if defined(HAVE_MMAP)
  if (player->input.fdm) {
    if (unmap_file(player->input.fdm, player->input.length) == -1) {
      error("decode", ":munmap");
      result = -1;
    }

    player->input.fdm = 0;

    if (!player->input.eof)
      player->input.data = 0;
  }
# endif

  if (player->input.data) {
    free(player->input.data);
    player->input.data = 0;
  }

  tag_finish(&player->input.tag);

  return result;
}

/*
 * NAME:	play_one()
 * DESCRIPTION:	open and play a single file
 */
static
int play_one(struct player *player)
{
  char const *file = player->input.path;
  int result;

  player->input.fd   = open(file, O_RDONLY | O_BINARY);
  if (player->input.fd == -1)
  {
    error(0, ":", file);
    return -1;
  } // if

  /* reset file information */

  player->stats.total_bytes = 0;
  player->stats.total_time  = mad_timer_zero;
  player->output.replay_gain &= ~PLAYER_RGAIN_SET;
  result = decode (player);

  if (player->input.fd != STDIN_FILENO &&
      close(player->input.fd) == -1 && result == 0)
  {
    error(0, ":", player->input.path);
    result = -1;
  }

  return result;
}

/*
 * NAME:	addfilter()
 * DESCRIPTION:	insert a filter at the beginning of the filter chain
 */
static
int addfilter(struct player *player, filter_func_t *func, void *data)
{
  struct filter *filter;

  filter = filter_new(func, data, player->output.filters);
  if (filter == 0)
    return -1;

  player->output.filters = filter;

  return 0;
}

/*
 * NAME:	setup_filters()
 * DESCRIPTION:	create output filters
 */
static
int setup_filters(struct player *player)
{
  /* filters must be added in reverse order */

# if defined(EXPERIMENTAL)
  if ((player->options & PLAYER_OPTION_EXTERNALMIX) &&
      addfilter(player, mixer_filter, stdout) == -1)
    return -1;

  if ((player->options & PLAYER_OPTION_EXPERIMENTAL) &&
      addfilter(player, experimental_filter, 0) == -1)
    return -1;
# endif

  if ((player->options & PLAYER_OPTION_FADEIN) &&
      addfilter(player, fadein_filter, player) == -1)
    return -1;

  addfilter(player, gain_filter, &player->output.gain);

  return 0;
}

/*
 * NAME:	silence()
 * DESCRIPTION:	output silence for a period of time
 */
static
int silence(struct player *player, mad_timer_t duration)
{
  union audio_control control;
  unsigned int nchannels, speed, nsamples;
  mad_fixed_t *samples;
  mad_timer_t unit;
  int result = 0;

  audio_control_init(&control, AUDIO_COMMAND_CONFIG);
  control.config.channels = 2;
  control.config.speed    = 44100;

  if (player->output.command(&control) == -1) {
    error("audio", audio_error);
    return -1;
  }

  nchannels = control.config.channels;
  speed     = control.config.speed;
  nsamples  = speed > MAX_NSAMPLES ? MAX_NSAMPLES : speed;

  player->output.channels_in  = nchannels;
  player->output.channels_out = nchannels;
  player->output.speed_in     = speed;
  player->output.speed_out    = speed;

  samples = calloc(nsamples, sizeof(mad_fixed_t));
  if (samples == 0) {
    error("silence", _("not enough memory to allocate sample buffer"));
    return -1;
  }

  audio_control_init(&control, AUDIO_COMMAND_PLAY);
  control.play.nsamples   = nsamples;
  control.play.samples[0] = samples;
  control.play.samples[1] = (nchannels == 2) ? samples : 0;
  control.play.mode       = player->output.mode;
  control.play.stats      = &player->stats.audio;

  mad_timer_set(&unit, 0, nsamples, speed);

  for (mad_timer_negate(&duration);
       mad_timer_sign(duration) < 0;
       mad_timer_add(&duration, unit)) {
    if (mad_timer_compare(unit, mad_timer_abs(duration)) > 0) {
      unit = mad_timer_abs(duration);
      control.play.nsamples = mad_timer_fraction(unit, speed);
    }

    if (player->output.command(&control) == -1) {
      error("audio", audio_error);
      goto fail;
    }

    mad_timer_add(&player->stats.global_timer, unit);
  }

  if (0) {
  fail:
    result = -1;
  }

  free(samples);

  return result;
}

/*
 * NAME:	player->run()
 * DESCRIPTION:	begin playback
 */
int player_run (struct player *player)
{
  int result = 0;
  union audio_control control;

  /* set up filters */

  if (setup_filters(player) == -1) {
    error("filter", _("not enough memory to allocate filters"));
    goto fail;
  }

  set_gain(player, 0);

  /* initialize audio */

  if (player->output.command) {
    audio_control_init(&control, AUDIO_COMMAND_INIT);
    control.init.path = player->output.path;

    if (player->output.command(&control) == -1) {
      error("audio", audio_error, control.init.path);
      goto fail;
    }

    if ((player->options & PLAYER_OPTION_SKIP) &&
	mad_timer_sign(player->global_start) < 0) {
      player->stats.global_timer = player->global_start;

      if (silence(player, mad_timer_abs(player->global_start)) == -1)
	result = -1;
    }
  }

  if (result == 0)
    result = play_one(player);

  /* drain and close audio */

  if (player->output.command) {
    audio_control_init(&control, AUDIO_COMMAND_FINISH);

    if (player->output.command(&control) == -1) {
      error("audio", audio_error);
      goto fail;
    }
  }

  if (0) {
  fail:
    result = -1;
  }
  return result;
}
