
dist_check_SCRIPTS = run1.sh

TEST_EXTENSIONS = .expected
EXPECTED_LOG_COMPILER = sh -x $(srcdir)/run1.sh
AM_EXPECTED_LOG_FLAGS = $(top_builddir)/src/daisy-player$(EXEEXT)
AM_TESTS_ENVIRONMENT = abs_srcdir=$(abs_builddir)/$(srcdir)/

TESTS = \
	Pierre_Rabhi_Vers_la_sobriete_heureuse.expected \
	Six_points_de_lumiere.expected \
	Y9621.expected \
	S07241.expected \
	$(null)
EXTRA_DIST = $(TESTS:.expected=) $(TESTS)
