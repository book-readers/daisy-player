AM_CFLAGS = -D LOCALEDIR=\"$(prefix)/share/locale\"  -Wall -Wextra \
            -Wunused -g -I /usr/include/libxml2/ -O2 -fstack-protector \
            --param=ssp-buffer-size=4 -Wformat -Werror=format-security \
            -Wuninitialized \
            -DPROGRAMNAME_LOCALEDIR=\"${PROGRAMNAME_LOCALEDIR}\"
AM_LDFLAGS = -lncursesw -lsox -lxml2 -lcdio -lcdio_cdda -lcdio_paranoia \
             -lmagic
bin_PROGRAMS = daisy-player
daisy_player_SOURCES = daisy3.c audiocd.c daisy-player.c paranoia.c daisy.h
localedir = $(datadir)/locale
