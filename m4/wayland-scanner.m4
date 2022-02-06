AC_DEFUN([WAYLAND_SCANNER_RULES], [
    wayland__prefix=${prefix}
    wayland__exec_prefix=${exec_prefix}

    prefix=/usr
    exec_prefix=${prefix}

    AC_PATH_PROG([wayland_scanner], [wayland-scanner], [/bin/false],
		 [${exec_prefix}/bin$PATH_SEPARATOR$PATH])
    AC_SUBST_FILE([wayland_scanner_rules])
    AC_SUBST([wayland_protocoldir], [$1])
    wayland_scanner_rules=${prefix}/share/aclocal/wayland-scanner.mk

    prefix=${wayland__prefix}
    exec_prefix=${wayland__exec_prefix}
])
