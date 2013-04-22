#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([daisy-player], [8.2], [jos@jlemmens.nl])
AM_INIT_AUTOMAKE([foreign])

AC_CONFIG_SRCDIR([src/daisy-player.c])
AC_CONFIG_HEADERS([daisy.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
AC_CHECK_LIB([cdio], [main])
# FIXME: Replace `main' with a function in `-lcdio_cdda':
AC_CHECK_LIB([cdio_cdda], [main])
# FIXME: Replace `main' with a function in `-lcdio_paranoia':
AC_CHECK_LIB([cdio_paranoia], [main])
# FIXME: Replace `main' with a function in `-lncursesw':
AC_CHECK_LIB([ncursesw], [main])
# FIXME: Replace `main' with a function in `-lsox':
AC_CHECK_LIB([sox], [main])
# FIXME: Replace `main' with a function in `-lxml2':
AC_CHECK_LIB([xml2], [main])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h locale.h stdlib.h string.h strings.h sys/ioctl.h sys/mount.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT16_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([atexit mkdir select setlocale strcasecmp strchr strdup strerror strrchr strstr])

AC_CONFIG_FILES([Makefile doc/Makefile man/Makefile src/Makefile po/Makefile])
AC_OUTPUT

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.18.1])

# extract gettext strings from source
xgettext ${srcdir}/src/*.c -o ${srcdir}/po/daisy-player.pot

# create .mo files                

# af for afrikaans
msgfmt -c ${srcdir}/po/af.po -o ${srcdir}/po/af.mo

# de for german
msgfmt -c ${srcdir}/po/de.po -o ${srcdir}/po/de.mo

# es for spanish
msgfmt -c ${srcdir}/po/es.po -o ${srcdir}/po/es.mo

# fr for french
msgfmt -c ${srcdir}/po/fr.po -o ${srcdir}/po/fr.mo

# hu for hungarian
msgfmt -c ${srcdir}/po/hu.po -o ${srcdir}/po/hu.mo

# nl for dutch
msgfmt -c ${srcdir}/po/nl.po -o ${srcdir}/po/nl.mo

# nb for norwegian
msgfmt -c ${srcdir}/po/nb.po -o ${srcdir}/po/nb.mo
