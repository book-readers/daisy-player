AUTOMAKE_OPTIONS = foreign

AM_CFLAGS = -I . -D LOCALEDIR=\"$(prefix)/share/locale\"  -Wall -Wextra \
            -g -I /usr/include/libxml2/ -O3 \
            --param=ssp-buffer-size=4 -Wformat -Werror=format-security \
            -DPROGRAMNAME_LOCALEDIR=\"${PROGRAMNAME_LOCALEDIR}\"

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
                       daisy3.c daisy-player.c playfile.c

localedir = $(datadir)/locale
