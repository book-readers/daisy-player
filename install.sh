#!/bin/bash

clear
if [ `whoami` != "root" ]; then
  echo "To install daisy-player this script needs to be executed whith root privillages."
  exit
fi

if [ $# == 1 ]; then
   PREFIX="$1"
else
   PREFIX="/usr/local/"
fi

# Compile from source
make clean
make

# Install daisy-player
install -D daisy-player  ${PREFIX}/bin/daisy-player

# generate manpage
txt2man -p daisy-player.txt > daisy-player.1
man2html daisy-player.1 > daisy-player.html.temp
tail -n +3 daisy-player.html.temp > daisy-player.html
rm -f daisy-player.html.temp
install -D daisy-player.1 ${PREFIX}/share/man/man1/daisy-player.1

# store .mp3 and other files
install -d ${PREFIX}/share/daisy-player/
cp -r COPYING Changelog License Readme TODO daisy-player.desktop daisy-player.html daisy-player.menu daisy-player.txt icons/ ${PREFIX}/share/daisy-player/

# create .mo files

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

update-language
update-locale
