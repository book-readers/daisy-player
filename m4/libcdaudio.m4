# Configure paths for libcdaudio                -*- Autoconf -*-
#
# Derived from glib.m4 (Owen Taylor 97-11-3)
#

dnl AM_PATH_LIBCDAUDIO([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND ]]])
dnl Test for libcdaudio, and define LIBCDAUDIO_CFLAGS, LIBCDAUDIO_LIBS and
dnl LIBCDAUDIO_LDADD
dnl
AC_DEFUN([AM_PATH_LIBCDAUDIO],
[dnl 
dnl Get the cflags and libraries from the libcdaudio-config script
dnl
AC_ARG_WITH(libcdaudio-prefix,
            AS_HELP_STRING([--with-libcdaudio-prefix=PFX],
                           [Prefix where libcdaudio is installed (optional)]),
            [libcdaudio_config_prefix="$withval"], [libcdaudio_config_prefix=""])
AC_ARG_WITH(libcdaudio-exec-prefix,
            AS_HELP_STRING([--with-libcdaudio-exec-prefix=PFX],
                           [Exec prefix where libcdaudio is installed (optional)]),
            [libcdaudio_config_exec_prefix="$withval"],
            [libcdaudio_config_exec_prefix=""])
AC_ARG_ENABLE(libcdaudiotest,
              AS_HELP_STRING([--disable-libcdaudiotest],
	                     [Do not try to compile and run a test libcdaudio program]),,
	      [enable_libcdaudiotest=yes])

  if test "x$libcdaudio_config_exec_prefix" != x ; then
     libcdaudio_config_args="$libcdaudio_config_args --exec-prefix=$libcdaudio_config_exec_prefix"
     if test "x${LIBCDAUDIO_CONFIG+set}" != xset ; then
        LIBCDAUDIO_CONFIG=$libcdaudio_config_exec_prefix/bin/libcdaudio-config
     fi
  fi
  if test x$libcdaudio_config_prefix != x ; then
     libcdaudio_config_args="$libcdaudio_config_args --prefix=$libcdaudio_config_prefix"
     if test "x${LIBCDAUDIO_CONFIG+set}" != xset ; then
        LIBCDAUDIO_CONFIG=$libcdaudio_config_prefix/bin/libcdaudio-config
     fi
  fi

  AC_PATH_PROG(LIBCDAUDIO_CONFIG, libcdaudio-config, no)
  min_libcdaudio_version=ifelse([$1], ,0.99.0,$1)
  AC_MSG_CHECKING([for libcdaudio - version >= $min_libcdaudio_version])
  no_libcdaudio=""
  if test "$LIBCDAUDIO_CONFIG" = "no" ; then
    no_libcdaudio=yes
  else
    LIBCDAUDIO_CFLAGS=`$LIBCDAUDIO_CONFIG $libcdaudio_config_args --cflags`
    LIBCDAUDIO_LIBS=`$LIBCDAUDIO_CONFIG $libcdaudio_config_args --libs`
    LIBCDAUDIO_LDADD=`$LIBCDAUDIO_CONFIG $libcdaudio_config_args --ldadd`
    libcdaudio_config_major_version=`$LIBCDAUDIO_CONFIG $libcdaudio_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\).*/\1/'`
    libcdaudio_config_minor_version=`$LIBCDAUDIO_CONFIG $libcdaudio_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\).*/\2/'`
    libcdaudio_config_micro_version=`$LIBCDAUDIO_CONFIG $libcdaudio_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\).*/\3/'`
    if test "x$enable_libcdaudiotest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $LIBCDAUDIO_CFLAGS $LIBCDAUDIO_LDADD"
      LIBS="$LIBCDAUDIO_LIBS $LIBS"
dnl
dnl Now check if the installed libcdaudio is sufficiently new. (Also sanity
dnl checks the results of libcdaudio-config to some extent
dnl
      rm -f conf.cdaudiotest
      AC_TRY_RUN([
#include <cdaudio.h>
#include <stdio.h>
#include <stdlib.h>

char* my_strdup (char *str)
{
  char *new_str;

  if (str) {
    new_str = malloc ((strlen (str) + 1) * sizeof(char));
    strcpy (new_str, str);
  } else
    new_str = NULL;

  return new_str;
}

int main()
{
  int major,minor,micro;
  int libcdaudio_major_version,libcdaudio_minor_version,libcdaudio_micro_version;
  char *tmp_version;

  system ("touch conf.cdaudiotest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = my_strdup("$min_libcdaudio_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_libcdaudio_version");
     exit(1);
   }

  libcdaudio_major_version=(cdaudio_getversion()>>16)&255;
  libcdaudio_minor_version=(cdaudio_getversion()>> 8)&255;
  libcdaudio_micro_version=(cdaudio_getversion()    )&255;

  if ((libcdaudio_major_version != $libcdaudio_config_major_version) ||
      (libcdaudio_minor_version != $libcdaudio_config_minor_version) ||
      (libcdaudio_micro_version != $libcdaudio_config_micro_version))
    {
      printf("\n*** 'libcdaudio-config --version' returned %d.%d.%d, but libcdaudio (%d.%d.%d)\n", 
             $libcdaudio_config_major_version, $libcdaudio_config_minor_version, $libcdaudio_config_micro_version,
             libcdaudio_major_version, libcdaudio_minor_version, libcdaudio_micro_version);
      printf ("*** was found! If libcdaudio-config was correct, then it is best\n");
      printf ("*** to remove the old version of libcdaudio. You may also be able to fix the error\n");
      printf("*** by modifying your LD_LIBRARY_PATH enviroment variable, or by editing\n");
      printf("*** /etc/ld.so.conf. Make sure you have run ldconfig if that is\n");
      printf("*** required on your system.\n");
      printf("*** If libcdaudio-config was wrong, set the environment variable LIBCDAUDIO_CONFIG\n");
      printf("*** to point to the correct copy of libcdaudio-config, and remove the file config.cache\n");
      printf("*** before re-running configure\n");
    } 
  else if ((libcdaudio_major_version != LIBCDAUDIO_VERSION_MAJOR) ||
	   (libcdaudio_minor_version != LIBCDAUDIO_VERSION_MINOR) ||
           (libcdaudio_micro_version != LIBCDAUDIO_VERSION_MICRO))
    {
      printf("*** libcdaudio header files (version %d.%d.%d) do not match\n",
	     LIBCDAUDIO_VERSION_MAJOR, LIBCDAUDIO_VERSION_MINOR, LIBCDAUDIO_VERSION_MICRO);
      printf("*** library (version %d.%d.%d)\n",
	     libcdaudio_major_version, libcdaudio_minor_version, libcdaudio_micro_version);
    }
  else
    {
      if ((libcdaudio_major_version > major) ||
        ((libcdaudio_major_version == major) && (libcdaudio_minor_version > minor)) ||
        ((libcdaudio_major_version == major) && (libcdaudio_minor_version == minor) && (libcdaudio_micro_version >= micro)))
      {
        return 0;
       }
     else
      {
        printf("\n*** An old version of libcdaudio (%d.%d.%d) was found.\n",
               libcdaudio_major_version, libcdaudio_minor_version, libcdaudio_micro_version);
        printf("*** You need a version of libcdaudio newer than %d.%d.%d.\n",
	       major, minor, micro);
        printf("***\n");
        printf("*** If you have already installed a sufficiently new version, this error\n");
        printf("*** probably means that the wrong copy of the libcdaudio-config shell script is\n");
        printf("*** being found. The easiest way to fix this is to remove the old version\n");
        printf("*** of libcdaudio, but you can also set the LIBCDAUDIO_CONFIG environment to point to the\n");
        printf("*** correct copy of libcdaudio-config. (In this case, you will have to\n");
        printf("*** modify your LD_LIBRARY_PATH enviroment variable, or edit /etc/ld.so.conf\n");
        printf("*** so that the correct libraries are found at run-time))\n");
      }
    }
  return 1;
}
],, no_libcdaudio=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_libcdaudio" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$LIBCDAUDIO_CONFIG" = "no" ; then
       echo "*** The libcdaudio-config script installed by libcdaudio could not be found"
       echo "*** If libcdaudio was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the LIBCDAUDIO_CONFIG environment variable to the"
       echo "*** full path to libcdaudio-config."
     else
       if test -f conf.cdaudiotest ; then
        :
       else
          echo "*** Could not run libcdaudio test program, checking why..."
          CFLAGS="$CFLAGS $LIBCDAUDIO_CFLAGS"
          LIBS="$LIBS $LIBCDAUDIO_LIBS"
          AC_TRY_LINK([
#include <cdaudio.h>
#include <stdio.h>
],      [ return (cdaudio_getversion()!=0); ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding libcdaudio or finding the wrong"
          echo "*** version of libcdaudio. If it is not finding libcdaudio, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location. Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means libcdaudio was incorrectly installed"
          echo "*** or that you have moved libcdaudio since it was installed. In the latter case, you"
          echo "*** may want to edit the libcdaudio-config script: $LIBCDAUDIO_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     LIBCDAUDIO_CFLAGS=""
     LIBCDAUDIO_LIBS=""
     LIBCDAUDIO_LDADD=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(LIBCDAUDIO_CFLAGS)
  AC_SUBST(LIBCDAUDIO_LIBS)
  AC_SUBST(LIBCDAUDIO_LDADD)
  rm -f conf.cdaudiotest
])
