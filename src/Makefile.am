AUTOMAKE_OPTIONS = foreign

AM_CFLAGS = -I . -I /usr/include/cdio/ -I /usr/include/cdio/paranoia/ \
            -I /usr/include/libxml2/ \
            -D LOCALEDIR=\"$(prefix)/share/locale\"  -Wall -Wextra \
            -Wunused -Wunused-variable -Wunused-function \
            -Wmissing-declarations \
            -g -I /usr/include/libxml2/ -O3 \
            -Wuninitialized -Winit-self \
            -DPROGRAMNAME_LOCALEDIR=\"${PROGRAMNAME_LOCALEDIR}\"

#AM_LDFLAGS = -static # jos

bin_PROGRAMS = daisy-player

ldadd = $(depend) @LTLIBINTL@

DEPENDENCIES = $(depend)

LDADD =	$(ldadd)

CLEANFILES = $(EXTRA_PROGRAMS)

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

.c.s:
	(cd madplay && make)
	(cd pactl/src && make)
	$(COMPILE) -S $<

again:
	$(MAKE) clean
	$(MAKE)

.PHONY: profile again

daisy_player_SOURCES = madplay/madplay.c madplay/audio.c \
                       madplay/crc.c madplay/version.c madplay/tag.c \
                       madplay/player.c madplay/audio_wave.c \
                       madplay/resample.c madplay/rgain.c \
                       madplay/filter.c madplay/audio_cdda.c \
                       pactl/src/core-util.c \
                       pactl/src/strbuf.c \
                       pactl/src/pactl.c \
                       audiocd.c paranoia.c common.c daisy2.02.c \
                       daisy3.c daisy-player.c playfile.c \
                       ncx.c opf.c

localedir = $(datadir)/locale
