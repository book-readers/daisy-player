man_MANS = daisy-player.1

html_doc = ../doc/daisy-player.html

$(html_doc): $(man_MANS)
	$(AM_V_GEN) \
	man2html $(man_MANS) | tail -n +3 > $@

daisy-player.1: ${top_srcdir}/doc/$(PACKAGE).txt
	$(AM_V_GEN) \
	which txt2man > /dev/null && txt2man -t "$(PACKAGE)" -s 1 -v "Linux Reference Manual" -r $(VERSION) $< > $@

EXTRA_DIST = $(html_doc)
