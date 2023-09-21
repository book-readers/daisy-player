man_MANS = daisy-player.1

html_doc = ${srcdir}/../doc/daisy-player.html

all: daisy-player $(html_doc)

daisy-player:
#	@which txt2man > /dev/null && txt2man -p ${srcdir}/../doc/$@.txt > ${srcdir}/$@.1
#	@file ${srcdir}/$@.1 && \
#	   man2html ${srcdir}/$@.1 | tail -n +3 > ${srcdir}/../doc/$@.html

#Date: Sun, 13 Nov 2016 16:22:29 +0000
#From: Daniel Shahaf <danielsh@apache.org>
#Subject: Bug#844232: daisy-player: man page title is wrong if build directory
#        isn't named ${package}-${version}
#To: Debian Bug Tracking System <submit@bugs.debian.org>

	@which txt2man > /dev/null && txt2man -t $@ -s 1 -v "Linux Reference Manual" -r $(VERSION) ${srcdir}/../doc/$@.txt > ${srcdir}/$@.1

$(html_doc): $(man_MANS)
	@file $(man_MANS:%=${srcdir}/%) && \
	  man2html $(man_MANS:%=${srcdir}/%) | tail -n +3 > $@

daisy-player.1:

EXTRA_DIST = ${srcdir}/../doc/daisy-player.html
