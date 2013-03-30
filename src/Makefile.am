AM_CFLAGS = -D PREFIX=\"/usr/local/\" -Wall -Wextra -Wunused -g -I /usr/include/libxml2/ -O2 -fstack-protector --param=ssp-buffer-size=4 -Wformat -Werror=format-security
AM_LDFLAGS = -lncursesw -lsox -lxml2 -lcdio -lcdio_cdda -lcdio_paranoia
bin_PROGRAMS = daisy-player
daisy_player_SOURCES = daisy3.c audiocd.c daisy-player.c paranoia.c
