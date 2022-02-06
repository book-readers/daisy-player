AM_CFLAGS = -D LOCALEDIR=\"$(prefix)/share/locale\"  -Wall -Wextra \
            -Wuninitialized -Winit-self -fno-common \
            -g -I /usr/include/libxml2/ -O2 -fstack-protector \
            --param=ssp-buffer-size=4 -Wformat -Werror=format-security \
            -DPROGRAMNAME_LOCALEDIR=\"${PROGRAMNAME_LOCALEDIR}\"
AM_LDFLAGS = -lncursesw -lsox -lxml2 -lcdio -lcdio_cdda -lcdio_paranoia \
             -lmagic
bin_PROGRAMS = daisy-player
daisy_player_SOURCES = daisy.h audiocd.c paranoia.c \
                       common.c daisy2.02.c daisy3.c daisy-player.c
localedir = $(datadir)/locale
