man_MANS = daisy-player.1

all: daisy-player

daisy-player:
	if which txt2man > /dev/null; then \
	  txt2man -p ${srcdir}/../doc/$@.txt > ${srcdir}/$@.1; \
	  if file ${srcdir}/$@.1 > /dev/null; then \
	    if which man2html > /dev/null; then \
	      man2html ${srcdir}/$@.1.1 | tail -n +3 > ${srcdir}/../doc/$@.html; \
	    fi \
	  fi \
	fi
