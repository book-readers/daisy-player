dnl madplay - MPEG audio decoder and player
dnl Copyright (C) 2000-2004 Robert Leslie
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2 of the License, or
dnl (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software
dnl Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
dnl
AC_REVISION([$Id: configure.ac,v 1.17 2004/02/23 21:34:53 rob Exp $])dnl

dnl Process this file with autoconf to produce a configure script.

AC_INIT([MPEG audio decoder and player],[0.15.2b],[rob@mars.org],[madtest])
AC_PREREQ([2.69])

AM_INIT_AUTOMAKE([subdir-objects])
AC_CONFIG_FILES([Makefile
                 m4/Makefile])

dnl System type.

AC_CANONICAL_HOST

dnl Checks for programs.

AC_PROG_CC

dnl Support for libtool.

dnl Compiler options.

dnl Checks for header files.

AC_CONFIG_HEADERS([config.h])
AC_HEADER_STDC
dnl Checks for typedefs, structures, and compiler characteristics.

AC_C_CONST
AC_C_INLINE
AC_C_BIGENDIAN

dnl Checks for library functions.

AC_FUNC_VPRINTF
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MMAP
AC_CHECK_FUNCS(madvise localeconv)
AC_REPLACE_FUNCS(strcasecmp strncasecmp)
AC_SEARCH_LIBS(pow, m)
AC_SEARCH_LIBS(log10, m)
AC_MSG_CHECKING([whether to use mmap])
AC_CACHE_SAVE

dnl madplay options...

AC_MSG_CHECKING([whether to enable profiling])
AC_MSG_RESULT(${enable_profiling-no})

AC_MSG_CHECKING([whether to enable debugging])
AC_MSG_RESULT(${enable_experimental-no})

dnl Create output files.

AC_TYPE_SSIZE_T
AC_PROG_YACC
AC_PROG_LN_S
AC_CHECK_FUNCS([__argz_count])
AC_CHECK_FUNCS([__argz_next])
AC_CHECK_FUNCS([__argz_stringify])
AC_CHECK_FUNCS([memmove])
AC_CHECK_FUNCS([munmap])
AC_CHECK_FUNCS([strcasecmp])
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([nl_langinfo])
AC_CHECK_FUNCS([pow])
AC_CHECK_FUNCS([strchr])
AC_CHECK_FUNCS([strcspn])
AC_CHECK_FUNCS([strncasecmp])
AC_CHECK_FUNCS([strrchr])
AC_CHECK_FUNCS([strstr])
AC_CHECK_FUNCS([strtol])
AC_CHECK_HEADERS([float.h])
AC_CHECK_HEADERS([fcntl.h])
AC_CHECK_HEADERS([termios.h])
AC_CHECK_HEADERS([langinfo.h])
AC_CHECK_HEADERS([libintl.h])
AC_CHECK_HEADERS([locale.h])
AC_CHECK_HEADERS([malloc.h]) 
AC_CHECK_HEADERS([stdio_ext.h])
AC_CHECK_HEADERS([wchar.h])
AC_CHECK_HEADER_STDBOOL
AC_CHECK_HEADER(id3tag.h, [], [
        AC_MSG_ERROR([id3tag.h was not found
*** You must first install libid3tag before you can build this package.
*** If libid3tag is already installed, you may need to use the CPPFLAGS
*** environment variable to specify its installed location, e.g. -I<dir>.])
])
AC_CHECK_TYPES([ptrdiff_t])
AC_CHECK_FUNCS([getcwd])
AC_CHECK_FUNCS([mempcpy])
AC_CHECK_FUNCS([stpcpy])
AC_CHECK_FUNCS([strtoul])
AC_CHECK_HEADERS([argz.h])
AC_CHECK_HEADERS([limits.h])
AC_CHECK_HEADERS([stddef.h])
AC_CHECK_HEADERS([sys/param.h])
AC_FUNC_ALLOCA
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRTOD
AC_PROG_CXX

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

AC_MSG_CHECKING([whether to enable profiling])
AC_ARG_ENABLE(profiling, AS_HELP_STRING([--enable-profiling],[generate profiling code]),
[
    case "$enableval" in
	yes) profile="-pg" ;;
    esac
])
AC_MSG_RESULT(${enable_profiling-no})

AC_MSG_CHECKING([whether to enable debugging])
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
AC_MSG_RESULT(${enable_experimental-no})

dnl Create output files.

test -n "$arch"     && CFLAGS="$CFLAGS $arch"
test -n "$debug"    && CFLAGS="$CFLAGS $debug"
test -n "$optimize" && CFLAGS="$CFLAGS $optimize"
test -n "$profile"  && CFLAGS="$CFLAGS $profile" LDFLAGS="$LDFLAGS $profile"
AC_DEFINE([AUDIO_DEFAULT], [], [audio_wav])
AC_OUTPUT
