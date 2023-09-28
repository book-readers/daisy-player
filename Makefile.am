AUTOMAKE_OPTIONS = foreign

SUBDIRS = doc po src tests

docdir = $(datadir)/doc/@PACKAGE@

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = ChangeLog \
             icons INSTALL \
             src/daisy.h \
             src/Makefile.am \
             src/madplay/audio.h \
             src/madplay/CHANGES \
             src/madplay/config.h.in \
             src/madplay/configure \
             src/madplay/configure.ac \
             src/madplay/COPYING \
             src/madplay/COPYRIGHT \
             src/madplay/crc.h \
             src/madplay/CREDITS \
             src/madplay/filter.h \
             src/madplay/gettext.h \
             src/madplay/global.h \
             src/madplay/INSTALL \
             src/madplay/m4/Makefile.am \
             src/madplay/m4/Makefile.in \
             src/madplay/madtest.c \
             src/madplay/Makefile.am \
             src/madplay/Makefile.in \
             src/madplay/player.h \
             src/madplay/README \
             src/madplay/resample.h \
             src/madplay/rgain.h \
             src/madplay/tag.h \
             src/madplay/TODO \
             src/madplay/VERSION \
             src/madplay/version.h \
             src/pactl/AGPL \
             src/pactl/configure \
             src/pactl/configure.ac \
             src/pactl/GPL \
             src/pactl/LGPL src/pactl/LICENSE \
             src/pactl/Makefile.am src/pactl/Makefile.in \
             src/pactl/NEWS \
             src/pactl/README \
             src/pactl/m4/Makefile.in \
             src/pactl/m4/Makefile.am \
             src/pactl/src/core-error.h \
             src/pactl/src/core-util.h \
             src/pactl/src/i18n.h \
             src/pactl/src/macro.h \
             src/pactl/src/Makefile.am \
             src/pactl/src/Makefile.in \
             src/pactl/src/pactl.c \
             src/pactl/src/socket.h \
             src/pactl/src/strbuf.h \
             src/pactl/src/strlist.h \
             src/pactl/todo

DISTCLEANFILES = \
	src/madplay/config.log \
	src/madplay/.deps/madtest.Po \
	src/madplay/Makefile \
	src/madplay/config.h \
	src/madplay/stamp-h1 \
	src/madplay/config.status \
	src/madplay/m4/Makefile \
	src/pactl/config.log \
	src/pactl/Makefile \
	src/pactl/config.status \
	src/pactl/m4/Makefile \
	src/pactl/src/Makefile

dist_doc_DATA = README.adoc

check-gettext:
	@if test x$(USE_NLS) != "xyes" ; then \
	  echo "Missing gettext. Rerun configure and check for" \
	  "'checking whether to use NLS... yes'!" ; exit 1; fi

update-po: check-gettext
	@find $(srcdir)/src/ -name "*.cpp" -print | sort > $(srcdir)/po/POTFILES.in.2 ; \
	if diff $(srcdir)/po/POTFILES.in $(srcdir)/po/POTFILES.in.2 >/dev/null 2>&1 ; then \
	   rm -f $(srcdir)/po/POTFILES.in.2 ; \
	else \
	   mv $(srcdir)/po/POTFILES.in.2 $(srcdir)/po/POTFILES.in ; \
	fi
	cd po && $(MAKE) $(AM_MAKEFLAGS) update-po

update-gmo: check-gettext
	cd po && $(MAKE) $(AM_MAKEFLAGS) update-gmo

force-update-gmo: check-gettext
	touch po/*.po
	cd po && $(MAKE) $(AM_MAKEFLAGS) update-gmo

force-update-gmo-%: check-gettext
	@language=`echo $@ | sed s/force-update-gmo-//` ; \
	if test ! -f po/$$language.po ; then \
	   echo "file po/$$language.po does not exist" ; exit 1 \
	fi ; \
	touch po/$$language.po ; \
	cd po && $(MAKE) $(AM_MAKEFLAGS) update-gmo
	
checkinstall:
	@echo
	@echo "checkinstall..."
	@echo
	make distdir
	cd $(PACKAGE)-$(VERSION)
	checkinstall -D --fstrans --pkgname "$(PACKAGE)" \
	--pkgversion $(VERSION) --pkggroup audio \
	--maintainer "Jos Lemmens \<jos\@jlemmens.nl\>" \
	--provides "daisy-player" \
	--strip --stripso \
	--review-control

.PHONY:
	hcheck-gettext update-po update-gmo force-update-gmo
