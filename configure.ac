#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([daisy-player], [13.0], [jos@jlemmens.nl], [], [www.jlemmens.nl])
AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_SRCDIR([config.h.in])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_MKDIR_P

# locate pkg-config
PKG_PROG_PKG_CONFIG

# Checks for libraries.
PKG_CHECK_MODULES([CDIO], [libcdio])
PKG_CHECK_MODULES([CDIO_CDDA], [libcdio_cdda])
PKG_CHECK_MODULES([CDIO_PARANOIA], [libcdio_paranoia])
dnl rely on pkgconfig alone once it is largely available
AC_CHECK_LIB([magic], [main],, [PKG_CHECK_MODULES([MAGIC], [libmagic])])
PKG_CHECK_MODULES([NCURSESW], [ncursesw])
PKG_CHECK_MODULES([SOX], [sox])
PKG_CHECK_MODULES([XML2], [libxml-2.0])
PKG_CHECK_MODULES([MAD], [mad])
PKG_CHECK_MODULES([PULSE], [libpulse])
PKG_CHECK_MODULES([ASOUND], [alsa])
AC_CHECK_HEADER(id3tag.h,, [PKG_CHECK_MODULES([ID3TAG], [id3tag])])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h libintl.h locale.h stdlib.h string.h strings.h \
                  sys/ioctl.h unistd.h])
#AC_CHECK_HEADERS([cdio/paranoia/cdda.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT16_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.7])
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([atexit mkdir select setlocale strcasecmp strchr strdup strerror strrchr strstr])
AC_CHECK_FUNCS([isascii])
AC_CHECK_FUNCS([rmdir])
AC_CHECK_FUNCS([memmove])
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([munmap])
AC_CHECK_FUNCS([pow])
AC_CHECK_FUNCS([strncasecmp])
AC_CHECK_FUNCS([strtol])
AC_CHECK_HEADERS([termios.h])
AC_CHECK_HEADERS([wchar.h])
AC_FUNC_MMAP
AC_TYPE_SSIZE_T

AC_CONFIG_FILES([Makefile
                 doc/Makefile
                 po/Makefile.in
                 src/Makefile
                 src/madplay/Makefile
                 src/pactl/Makefile
                 tests/Makefile])

dnl System type.

AC_CANONICAL_HOST

dnl Compiler options.

AM_SILENT_RULES([yes])

dnl Checks for typedefs, structures, and compiler characteristics.

AC_C_CONST
AC_C_BIGENDIAN

dnl Checks for library functions.

AC_CHECK_FUNCS(madvise localeconv)
AC_REPLACE_FUNCS(strcasecmp strncasecmp)
AC_SEARCH_LIBS(pow, m)
AC_SEARCH_LIBS(log10, m)
AC_CACHE_SAVE

dnl Create output files.

AC_CHECK_FUNCS([nl_langinfo])
AC_CHECK_FUNCS([strchr])
AC_CHECK_FUNCS([strcspn])
AC_CHECK_FUNCS([strrchr])
AC_CHECK_FUNCS([strstr])
AC_CHECK_HEADERS([float.h])
AC_CHECK_HEADERS([langinfo.h])
AC_CHECK_HEADERS([libintl.h])
AC_CHECK_HEADERS([locale.h])
AC_CHECK_HEADERS([malloc.h])
AC_CHECK_HEADERS([stdio_ext.h])
AC_CHECK_HEADER_STDBOOL
AC_CHECK_TYPES([ptrdiff_t])
AC_CHECK_FUNCS([getcwd])
AC_CHECK_FUNCS([mempcpy])
AC_CHECK_FUNCS([stpcpy])
AC_CHECK_FUNCS([strtoul])
AC_CHECK_HEADERS([argz.h])
AC_CHECK_HEADERS([limits.h])
AC_CHECK_HEADERS([stddef.h])
AC_CHECK_HEADERS([sys/param.h])

dnl Select an audio system.

depend_audio=""
ldadd_audio=""
AC_SUBST(depend_audio)
AC_SUBST(ldadd_audio)
AC_MSG_CHECKING([for audio support])

if test "$audio" = unknown
then
    AC_EGREP_CPP(waveOutOpen, [
# include <mmsystem.h>
], [audio="win32"; ldadd_audio="$ldadd_audio -lwinmm"])
fi

AC_MSG_RESULT($audio)

AC_ARG_ENABLE(profiling, AS_HELP_STRING([--enable-profiling],[generate profiling code]),
[
    case "$enableval" in
	yes) profile="-pg" ;;
    esac
])

AC_ARG_ENABLE(debugging, AS_HELP_STRING([--enable-debugging],[enable diagnostic debugging support])
AS_HELP_STRING([--disable-debugging],[do not enable debugging and use more optimization]),
[
    case "$enableval" in
	yes)
	    AC_DEFINE(DEBUG, 1,
		[Define to enable diagnostic debugging support.])
	    optimize=""
	    ;;
	no)
	    if test -n "$profile"
	    then
    AC_MSG_ERROR([--enable-profiling and --disable-debugging are incompatible])
	    fi

	    AC_DEFINE(NDEBUG, 1,
		[Define to disable debugging assertions.])
	    debug=""
	    if test "$GCC" = yes
	    then
		optimize="$optimize -fomit-frame-pointer"
	    fi
	    ;;
    esac
])
AC_MSG_RESULT(${enable_debugging-default})

case "$enable_debugging" in
    yes) AC_CHECK_LIB(efence, malloc) ;;
esac

AC_MSG_CHECKING([whether to enable experimental code])
AC_ARG_ENABLE(experimental, AS_HELP_STRING([--enable-experimental],[enable experimental code]),
[
    case "$enableval" in
	yes)
	    AC_DEFINE(EXPERIMENTAL, 1, [Define to enable experimental code.])
	    ;;
    esac
])

test -n "$arch"     && CFLAGS="$CFLAGS $arch"
test -n "$debug"    && CFLAGS="$CFLAGS $debug"
test -n "$optimize" && CFLAGS="$CFLAGS $optimize"
test -n "$profile"  && CFLAGS="$CFLAGS $profile" LDFLAGS="$LDFLAGS $profile"
AC_DEFINE([AUDIO_DEFAULT], [], [audio_wav])
AC_OUTPUT

dnl madplay stuff

echo
echo "Configure madplay..."
echo                      

(cd src/madplay && ../../${srcdir}/src/madplay/configure)

dnl pulseaudio/pactl stuff

echo
echo "Configure pulseaudio/pactl..."
echo

(cd src/pactl && ../../${srcdir}/src/pactl/configure)
