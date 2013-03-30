#!/bin/bash
# create .mo files

$PREFIX=/local/bin/

# de for german
install -d ${PREFIX}/share/locale/de/LC_MESSAGES
msgfmt -c po/daisy-player.de.po -o ${PREFIX}/share/locale/de/LC_MESSAGES/daisy-player.mo

# es for spanish
install -d ${PREFIX}/share/locale/es/LC_MESSAGES
msgfmt -c po/daisy-player.es.po -o ${PREFIX}/share/locale/es/LC_MESSAGES/daisy-player.mo

# fr for french
install -d ${PREFIX}/share/locale/fr/LC_MESSAGES
msgfmt -c po/daisy-player.fr.po -o ${PREFIX}/share/locale/fr/LC_MESSAGES/daisy-player.mo

# hu for hungarian
install -d ${PREFIX}/share/locale/hu/LC_MESSAGES
msgfmt -c po/daisy-player.hu.po -o ${PREFIX}/share/locale/hu/LC_MESSAGES/daisy-player.mo

# nl for dutch
install -d ${PREFIX}/share/locale/nl/LC_MESSAGES
msgfmt -c po/daisy-player.nl.po -o ${PREFIX}/share/locale/nl/LC_MESSAGES/daisy-player.mo

# nb for norwegian
install -d ${PREFIX}/share/locale/nb/LC_MESSAGES
msgfmt -c po/daisy-player.nb.po -o ${PREFIX}/share/locale/nb/LC_MESSAGES/daisy-player.mo

# af for afrikaans
install -d ${PREFIX}/share/locale/af/LC_MESSAGES
msgfmt -c po/daisy-player.af.po -o ${PREFIX}/share/locale/af/LC_MESSAGES/daisy-player.mo

xgettext daisy-player.c daisy3.c audiocd.c -o daisy-player.pot

update-language
update-locale
