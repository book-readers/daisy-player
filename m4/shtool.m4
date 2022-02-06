##
##  shtool.m4 -- GNU shtool Autoconf macros
##  Copyright (c) 1999-2008 Ralf S. Engelschall <rse@engelschall.com>
##
##  This file is part of shtool and free software; you can redistribute
##  it and/or modify it under the terms of the GNU General Public
##  License as published by the Free Software Foundation; either version
##  2 of the License, or (at your option) any later version.
##
##  This file is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
##  General Public License for more details.
##
##  You should have received a copy of the GNU General Public License
##  along with this program; if not, write to the Free Software
##  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
##  USA, or contact Ralf S. Engelschall <rse@engelschall.com>.
##

##
##  the standard macro for finding GNU shtool:
##  AC_PROG_SHTOOL
##
AC_DEFUN([AC_PROG_SHTOOL],[dnl
AC_PREREQ(2.13)dnl
AC_MSG_CHECKING([for GNU shtool])
AC_CACHE_VAL(ac_cv_shtool,[dnl
#   canonicalize and split path string
ac_paths="`echo .:$ac_aux_dir:$PATH |\
           sed -e 's%/*:%:%g' -e 's%/*$%%' \
               -e 's/^:/.:/' -e 's/::/:.:/g' -e 's/:$/:./' \
               -e 's/:/ /g'`"
#   iterate over $PATH but prefer CWD
ac_cv_shtool=""
for ac_path in $ac_paths; do
    if test -f "$ac_path/shtool" && test ! -d "$ac_path/shtool"; then
        ac_cv_shtool="$ac_path/shtool"
        break
    fi
done
#   check for existance
if test ".$ac_cv_shtool" = .; then
    AC_MSG_ERROR([no shtool found in .:$PATH])
fi
#   check deeper
ac_rc=`($ac_cv_shtool --version) </dev/null 2>/dev/null | grep 'GNU shtool'`
if test ".$ac_rc" = .; then
    ac_cv_shtool="${CONFIG_SHELL-/bin/sh} $ac_cv_shtool"
    ac_rc=`($ac_cv_shtool --version) </dev/null 2>/dev/null | grep 'GNU shtool'`
    if test ".$ac_rc" = .; then
        AC_MSG_ERROR([$ac_cv_shtool seems not to be GNU shtool])
    fi
fi
])dnl
AC_MSG_RESULT([$ac_cv_shtool])
SHTOOL="$ac_cv_shtool"
AC_SUBST(SHTOOL)
])

