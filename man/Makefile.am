man_MANS = daisy-player.1

all: daisy-player

daisy-player:
	@which txt2man > /dev/null && txt2man -p ${srcdir}/../doc/$@.txt > ${srcdir}/$@.1
	@file ${srcdir}/$@.1 && \
	   man2html ${srcdir}/$@.1 | tail -n +3 > ${srcdir}/../doc/$@.html

daisy-player.1:
