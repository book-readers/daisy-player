#!/bin/sh

prefix=/usr/local

# generate manpage
txt2man -p doc/daisy-player.txt > man/daisy-player.1
man2html man/daisy-player.1 | tail -n +3 > doc/daisy-player.html

# extract gettext strings from source
xgettext src/*.c -o po/daisy-player.pot

# create .mo files

# af for afrikaans
install -d ${prefix}/share/locale/af/LC_MESSAGES
msgfmt -c po/daisy-player.af.po -o ${prefix}/share/locale/af/LC_MESSAGES/daisy-player.mo

# de for german
install -d ${prefix}/share/locale/de/LC_MESSAGES
msgfmt -c po/daisy-player.de.po -o ${prefix}/share/locale/de/LC_MESSAGES/daisy-player.mo

# es for spanish
install -d ${prefix}/share/locale/es/LC_MESSAGES
msgfmt -c po/daisy-player.es.po -o ${prefix}/share/locale/es/LC_MESSAGES/daisy-player.mo

# fr for french
install -d ${prefix}/share/locale/fr/LC_MESSAGES
msgfmt -c po/daisy-player.fr.po -o ${prefix}/share/locale/fr/LC_MESSAGES/daisy-player.mo

# hu for hungarian
install -d ${prefix}/share/locale/hu/LC_MESSAGES
msgfmt -c po/daisy-player.hu.po -o ${prefix}/share/locale/hu/LC_MESSAGES/daisy-player.mo

# nl for dutch
install -d ${prefix}/share/locale/nl/LC_MESSAGES
msgfmt -c po/daisy-player.nl.po -o ${prefix}/share/locale/nl/LC_MESSAGES/daisy-player.mo

# nb for norwegian
install -d ${prefix}/share/locale/nb/LC_MESSAGES
msgfmt -c po/daisy-player.nb.po -o ${prefix}/share/locale/nb/LC_MESSAGES/daisy-player.mo

update-language
update-locale
