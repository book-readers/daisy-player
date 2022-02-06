# madtest.c - a small program for testing. (C)2017 J. Lemmens

## madplay - MPEG audio decoder and player
## Copyright (C) 2000-2004 Robert Leslie
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
##
## If you would like to negotiate alternate licensing terms, you may do
## so by contacting the author: Robert Leslie <rob@mars.org>
##
## $Id: Makefile.am,v 1.36 2004/02/23 21:34:53 rob Exp $
##

AUTOMAKE_OPTIONS = foreign

AM_CFLAGS = -D LOCALEDIR=\"$(prefix)/share/locale\"  -Wall -Wextra \
            -Wunused-function -Wuninitialized -Winit-self -fno-common \
            -I /usr/include/libxml2/ -O3 \
            --param=ssp-buffer-size=4 -Wformat -Werror=format-security \
            -DPROGRAMNAME_LOCALEDIR=\"${PROGRAMNAME_LOCALEDIR}\"

DIST_SUBDIRS =	 m4

ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = madtest

EXTRA_DIST = ABOUT-NLS aclocal.m4 am ar-lib autom4te.cache CHANGES \
             compile config.guess config.h config.h.in config.status \
             config.sub configure configure.ac COPYING COPYRIGHT \
             CREDITS depcomp INSTALL install-sh m4 Makefile.am \
             Makefile.in mdate-sh missing mkinstalldirs py-compile \
             README test-driver TODO VERSION

default_audio =		audio.c audio.h audio_wave.c

madtest_SOURCES = global.h madplay.c gettext.h version.c resample.c \
                  filter.c tag.c crc.c rgain.c player.h player.c \
                  version.h resample.h filter.h tag.h crc.h rgain.h \
                  audio_cdda.c $(default_audio) madtest.c

madtest_DEPENDENCIES = $(depend_audio) $(DEPENDENCIES)
madtest_LDADD = $(ldadd_audio) $(LDADD) $(ldadd_libid3tag)

localedir = $(datadir)/locale
AM_CPPFLAGS = -DLOCALEDIR=\"$(localedir)\"

depend_audio = @depend_audio@
ldadd_audio = $(depend_audio) @ldadd_audio@

ldadd_libmad = -lmad

depend = @LIBOBJS@
#ldadd = $(depend) @LTLIBINTL@

DEPENDENCIES = $(depend)
LDADD =	$(ldadd)  $(ldadd_libmad)

CLEANFILES =		$(EXTRA_PROGRAMS)

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

profile: madtest
	gprof madtest | less

.c.s:
	$(COMPILE) -S $< -fwhole-program

again:
	$(MAKE) clean
	$(MAKE)

.PHONY: profile again
