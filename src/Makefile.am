AUTOMAKE_OPTIONS = foreign
AM_CFLAGS = -D LOCALEDIR=\"$(prefix)/share/locale\"  -Wall -Wextra \
            -Wunused-function -Wuninitialized -Winit-self -fno-common \
            -g -I /usr/include/libxml2/ -O3 \
            --param=ssp-buffer-size=4 -Wformat -Werror=format-security \
            -DPROGRAMNAME_LOCALEDIR=\"${PROGRAMNAME_LOCALEDIR}\"
AM_LDFLAGS = -lncursesw -lsox -lxml2 -lcdio -lcdio_cdda -lcdio_paranoia \
             -lmagic -lasound -lmad

bin_PROGRAMS = daisy-player

ldadd =                 $(depend) @LTLIBINTL@

DEPENDENCIES =          $(depend) \
LDADD =			$(ldadd)

CLEANFILES =		$(EXTRA_PROGRAMS)

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

.c.s:
	$(COMPILE) -S $<

again:
	$(MAKE) clean
	$(MAKE)            

.PHONY: profile again

daisy_player_SOURCES = daisy.h audiocd.c paranoia.c common.c daisy2.02.c \
                       daisy3.c daisy-player.c \
                       madplay/global.h madplay/madplay.c madplay/audio.c \
                       madplay/version.h madplay/audio.h \
                       madplay/crc.c madplay/version.c madplay/tag.c \
                       madplay/rgain.h madplay/player.c \
                       madplay/tag.h madplay/audio_wave.c madplay/resample.c \
                       madplay/crc.h madplay/gettext.h madplay/resample.h \
                       madplay/rgain.c \
                       madplay/filter.c madplay/player.h \
                       madplay/filter.h madplay/audio_cdda.c

localedir = $(datadir)/locale
