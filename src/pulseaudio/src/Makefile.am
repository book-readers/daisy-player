AUTOMAKE_OPTIONS = foreign

AM_CFLAGS = -D PA_BINARY=\"./\" -D LOCALEDIR=\"$(prefix)/share/locale\" \
            -D PA_DEFAULT_CONFIG_DIR=pulseconfdir -D TCP_NODELAY=1 \
            -D PA_MACHINE_ID=\"\" -Wall -Wextra -D RLIMIT_NOFILE=\"\" \
            -D PA_MACHINE_ID_FALLBACK=\"\" -D PA_BUILDDIR=\"\" \
            -D PA_SINK_CLIENT_FLAGS_MASK=0xFFFFFF \
            -D PA_SINK_SHARE_VOLUME_WITH_MASTER=0x1000000U \
            -D PA_SOURCE_CLIENT_FLAGS_MASK=0xFFFFFF \
            -Wall -Wunused-function -Winit-self -fno-common -O3 \
            --param=ssp-buffer-size=4 -Wformat -Werror=format-security \
            -DPROGRAMNAME_LOCALEDIR=\"${PROGRAMNAME_LOCALEDIR}\" \
            -lm -pthread -g

bin_PROGRAMS = pactl-set-volume

DEPENDENCIES = $(depend)

LDADD = $(ldadd)

CLEANFILES = $(EXTRA_PROGRAMS)

libtool:
	$(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

pactl_set_volume_SOURCES = strbuf.c core-util.c pactl.c pactl-set-volume.c

pactl_set_volume_DEPENDENCIES = $(DEPENDENCIES)
                        
depend = @LIBOBJS@

profile:
	pactl-set-volume

.c.s:
	$(COMPILE) -S $<

again:
	$(MAKE) clean
	$(MAKE)

.PHONY:
	profile again
