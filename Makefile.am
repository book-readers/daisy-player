AUTOMAKE_OPTIONS = foreign

SUBDIRS = man doc po src  

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = config.rpath m4/Makefile.in src/gettext.h doc/COMPILE \
             doc/Changelog doc/License doc/Readme doc/daisy-player.* \
             man/daisy-player.1 icons/daisy-player.svg \
             icons/48x48/daisy-player.png icons/72x72/daisy-player.png \
             ltmain.sh \
             src/madplay/ABOUT-NLS src/madplay/aclocal.m4 src/madplay/am \
             src/madplay/ar-lib src/madplay/CHANGES src/madplay/compile \
             src/madplay/config.guess src/madplay/config.h \
             src/madplay/config.h.in src/madplay/config.status \
             src/madplay/config.sub src/madplay/configure \
             src/madplay/configure.ac src/madplay/configure.scan \
             src/madplay/COPYING src/madplay/COPYRIGHT src/madplay/CREDITS \
             src/madplay/depcomp src/madplay/INSTALL src/madplay/install-sh \
             src/madplay/.libs src/madplay/m4 src/madplay/m4/ChangeLog \
             src/madplay/m4/codeset.m4 src/madplay/m4/gettext.m4 \
             src/madplay/m4/glibc21.m4 src/madplay/m4/iconv.m4 \
             src/madplay/m4/intdiv0.m4 src/madplay/m4/intmax.m4 \
             src/madplay/m4/inttypes_h.m4 src/madplay/m4/inttypes.m4 \
             src/madplay/m4/inttypes-pri.m4 src/madplay/m4/isc-posix.m4 \
             src/madplay/m4/lcmessage.m4 src/madplay/m4/lib-ld.m4 \
             src/madplay/m4/lib-link.m4 src/madplay/m4/lib-prefix.m4 \
             src/madplay/m4/longdouble.m4 src/madplay/m4/longlong.m4 \
             src/madplay/m4/Makefile.am src/madplay/m4/Makefile.in \
             src/madplay/m4/nls.m4 src/madplay/m4/po.m4 \
             src/madplay/m4/printf-posix.m4 src/madplay/m4/progtest.m4 \
             src/madplay/m4/signed.m4 src/madplay/m4/size_max.m4 \
             src/madplay/m4/stdint_h.m4 src/madplay/m4/uintmax_t.m4 \
             src/madplay/m4/ulonglong.m4 src/madplay/m4/wchar_t.m4 \
             src/madplay/m4/wint_t.m4 src/madplay/m4/xsize.m4 \
             src/madplay/madtest.c \
             src/madplay/Makefile.am src/madplay/Makefile.in \
             src/madplay/mdate-sh src/madplay/missing \
             src/madplay/mkinstalldirs src/madplay/py-compile \
             src/madplay/README src/madplay/stamp-h1 \
             src/madplay/tap-driver.sh src/madplay/test-driver \
             src/madplay/texinfo.tex src/madplay/TODO src/madplay/VERSION \
             src/madplay/ylwrap

check-gettext:
	@if test x$(USE_NLS) != "xyes" ; then \
	  echo "Missing gettext. Rerun configure and check for" \"'checking whether to use NLS... yes'!" ; exit 1; fi

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

.PHONY: check-gettext update-po update-gmo force-update-gmo
