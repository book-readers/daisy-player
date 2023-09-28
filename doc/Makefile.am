docdir = $(datadir)/doc/@PACKAGE@
doc_DATA = Changelog COMPILE COPYING daisy-player.description \
           daisy-player.desktop daisy-player.html daisy-player.menu \
           daisy-player.png daisy-player.txt License Makefile.am \
           TODO

dist_html_DATA = daisy-player.html
dist_man_MANS = daisy-player.1

$(dist_html_DATA): $(dist_man_MANS)
	$(AM_V_GEN) \
	man2html $(dist_man_MANS) | tail -n +3 > $@

$(dist_man_MANS): $(PACKAGE).txt
	$(AM_V_GEN) \
	which txt2man > /dev/null && txt2man -t "$(PACKAGE)" -s 1 -v "Linux Reference Manual" -r $(VERSION) $< > $@
