AUTOMAKE_OPTIONS = foreign

AM_CPPFLAGS = $(CDIO_CFLAGS) \
	$(CDIO_CDDA_CFLAGS) \
	$(CDIO_PARANOIA_CFLAGS) \
	$(MAGIC_CFLAGS) \
	$(NCURSESW_CFLAGS) \
	$(SOX_CFLAGS) \
	$(XML2_CFLAGS) \
	$(MAD_CFLAGS) \
	$(PULSE_CFLAGS) \
	$(ID3TAG_CFLAGS) \
	$(ASOUND_CFLAGS)

AM_CFLAGS = -I . -D LOCALEDIR=\"$(prefix)/share/locale\"  -Wall -Wextra \
            -Wunused -Wunused-variable -Wunused-function \
            -Wmissing-declarations \
            -g -O3 \
            -Wuninitialized -Winit-self \
            -DPROGRAMNAME_LOCALEDIR=\"${PROGRAMNAME_LOCALEDIR}\"

#AM_LDFLAGS = -static # jos

LIBS += $(CDIO_LIBS) \
	$(CDIO_CDDA_LIBS) \
	$(CDIO_PARANOIA_LIBS) \
	$(MAGIC_LIBS) \
	$(NCURSESW_LIBS) \
	$(SOX_LIBS) \
	$(XML2_LIBS) \
	$(MAD_LIBS) \
	$(PULSE_LIBS) \
	$(ID3TAG_LIBS) \
	$(ASOUND_LIBS)

bin_PROGRAMS = daisy-player

ldadd = $(depend) @LTLIBINTL@

DEPENDENCIES = $(depend)

LDADD =	$(ldadd)

CLEANFILES = $(EXTRA_PROGRAMS)

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

.c.s:
	(cd madplay && $(MAKE))
	(cd pactl/src && $(MAKE))
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
                       alsa_ctl.c \
                       audiocd.c paranoia.c common.c daisy2.02.c \
                       daisy3.c daisy-player.c playfile.c \
                       ncx.c opf.c

localedir = $(datadir)/locale
