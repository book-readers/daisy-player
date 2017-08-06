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
 * $Id: madplay.c,v 1.86 2004/02/23 21:34:53 rob Exp $
 */

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include "global.h"

/* include this first to avoid conflicts with MinGW __argc et al. */
# include "getopt.h"

# include <locale.h>
# include <stdio.h>
# include <stdarg.h>
# include <stdlib.h>
# include <string.h>

# ifdef HAVE_ASSERT_H
#  include <assert.h>
# endif

# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# endif

# ifdef HAVE_FCNTL_H
#  include <fcntl.h>
# endif

# include <ctype.h>
# include <mad.h>

# include "gettext.h"

# include "version.h"
# include "audio.h"
# include "player.h"

# define FADE_DEFAULT	"0:05"

# if defined(EXPERIMENTAL)
static int external_mix;
static int experimental;
# endif

char const *argv0;


/*
 * NAME:	verror()
 * DESCRIPTION:	print error message with program title prefix
 */
static
void verror(char const *message, va_list args)
{
  fprintf(stderr, "%s: ", argv0);
  vfprintf(stderr, message, args);
  fputc('\n', stderr);
}

/*
 * NAME:	warn()
 * DESCRIPTION:	print warning message
 */
static
void warn(char const *message, ...)
{
  va_list args;

  va_start(args, message);
  verror(message, args);
  va_end(args);
}

/*
 * NAME:	die()
 * DESCRIPTION:	exit with failure status after printing message
 */
static
void die(char const *message, ...)
{
  va_list args;

  va_start(args, message);
  verror(message, args);
  va_end(args);

  exit(1);
}

/*
 * NAME:	parse_time()
 * DESCRIPTION:	parse a time specification string
 */
static int parse_time (mad_timer_t *timer, char const *str)
{
  mad_timer_t time, accum = mad_timer_zero;
  signed long decimal;
  unsigned long seconds, fraction, fracpart;
  int minus;

  while (isspace((unsigned char) *str))
    ++str;

  do {
    seconds = fraction = fracpart = 0;

    switch (*str) {
    case '-':
      ++str;
      minus = 1;
      break;

    case '+':
      ++str;
    default:
      minus = 0;
    }

    do {
      decimal = strtol(str, (char **) &str, 10);
      if (decimal < 0)
	return -1;

      seconds += decimal;

      if (*str == ':') {
	seconds *= 60;
	++str;
      }
    }
    while (*str >= '0' && *str <= '9');

    if (*str == '.') 
    {
      char const *ptr;

      decimal = strtol(++str, (char **) &ptr, 10);
      if (decimal < 0)
	return -1;

      fraction = decimal;

      for (fracpart = 1; str != ptr; ++str)
	fracpart *= 10;
    }
    else if (*str == '/') {
      ++str;

      decimal = strtol(str, (char **) &str, 10);
      if (decimal < 0)
	return -1;

      fraction = seconds;
      fracpart = decimal;

      seconds  = 0;
    }

    mad_timer_set(&time, seconds, fraction, fracpart);
    if (minus)
      mad_timer_negate(&time);

    mad_timer_add(&accum, time);
  }
  while (*str == '-' || *str == '+');

  while (isspace((unsigned char) *str))
    ++str;

  if (*str != 0)
    return -1;

  *timer = accum;

  return 0;
}

/*
 * NAME:	get_time()
 * DESCRIPTION:	parse a time value or die
 */
static mad_timer_t get_time (char const *str, int positive, char const *name)
{
  mad_timer_t time;

  if (parse_time (&time, str) == -1)
    die(_("invalid %s specification \"%s\""), name, str);

  if (positive && mad_timer_sign (time) <= 0)
    die(_("%s must be positive"), name);

  return time;                   
}

/*
 * NAME:	main()
 * DESCRIPTION:	program entry point
 */
int madplay (char *in_file, char *begin, char *duration, char *out_file)
{
  struct player player;
  int result = 0;
  int ttyset = 0;

  /* initialize and get options */

  player_init (&player);
  player.verbosity = -2;
  if (! ttyset)
    player.options &= ~PLAYER_OPTION_TTYCONTROL;
  player.global_start = get_time (begin, 0, _("start time"));
  player.options |= PLAYER_OPTION_SKIP;
  player.global_stop = get_time (duration, 1, _("playing time"));
  player.options |= PLAYER_OPTION_TIMED;
  player.input.path = in_file;
  player.output.path = out_file;
  player.output.command = audio_output (&player.output.path);
  if (player.output.command == 0)
    die (_("unknown output format type for \"%s\""), out_file);
  if (!ttyset)
    player.options &= ~PLAYER_OPTION_TTYCONTROL;

  /* main processing */

  if (player.options & PLAYER_OPTION_CROSSFADE)
  {
    if (!(player.options & PLAYER_OPTION_GAP))
      warn(_("cross-fade ignored without gap"));
    else
      if (mad_timer_sign(player.gap) >= 0)
        warn(_("cross-fade ignored without negative gap"));
  }

  if (player.output.replay_gain & PLAYER_RGAIN_ENABLED)
  {
    if (player.options & PLAYER_OPTION_IGNOREVOLADJ)
      warn(_("volume adjustment ignored with Replay Gain enabled"));
    else
      player.options |= PLAYER_OPTION_IGNOREVOLADJ;
  }

  if ((player.options & PLAYER_OPTION_SHOWTAGSONLY) &&
      player.repeat != 1) {
    warn(_("ignoring repeat"));
    player.repeat = 1;
  }

  /* make stop time absolute */
  if (player.options & PLAYER_OPTION_TIMED)
    mad_timer_add (&player.global_stop, player.global_start);
  /* run the player */

  if (player_run (&player) == -1)
    result = 4;

  /* finish up */

  player_finish (&player);

  return result;
} // madplay
