/*
 * pactl-set-volume - test the pactl program
 *
 * (C)2018 - Jos Lemmens
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int pactl (char *, char *);

int main (int argc, char *argv[])
{

   if (argc != 3)
   {
      printf ("Usage: %s <pulseaudio_device> <volume>\n", *argv);
      return -1;
   } // if
printf ("argv[0] %s\n", argv[0]);
printf ("argv[1] %s\n", argv[1]);
printf ("argv[2] %s\n", argv[2]);
printf ("argv[3] %s\n", argv[3]);
   pactl (argv[1], argv[2]);
} // main
