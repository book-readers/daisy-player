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

#include <stdio.h>
extern int madplay (char *, char *, char *, char *);

int main (int argc, char *argv[])
{
  if (argc != 5)
  {
    printf ("Usage: %s <in_file> <start_time> <duration> <out_file.wav>\n", *argv);
    return -1;
  } // if
  return madplay (argv[1], argv[2], argv[3], argv[4]);
} // main
