#!/bin/bash

if [ `whoami` != "root" ]; then
  echo "To install daisy-player this script needs to be executed whith root privellages."
  exit
fi

# Compile from source
make

# Install daisy-player

cp daisy-player /usr/local/bin/

# generate manpage
mkdir -p /usr/local/share/man/man1
txt2man -p daisy-player.txt > daisy-player.1
man2html daisy-player.1 > daisy-player.html
cp daisy-player.1 /usr/local/share/man/man1

# store .mp3 file
mkdir -p /usr/local/share/daisy-player/
cp error.mp3 /usr/local/share/daisy-player/

# create .mo files
# de for german
test -d /usr/share/locale/de/LC_MESSAGES || mkdir -p /usr/share/locale/de/LC_MESSAGES
msgfmt daisy-player.de.po -o /usr/share/locale/de/LC_MESSAGES/daisy-player.mo

# nl for dutch
test -d /usr/share/locale/nl/LC_MESSAGES || mkdir -p /usr/share/locale/nl/LC_MESSAGES
msgfmt daisy-player.nl.po -o /usr/share/locale/nl/LC_MESSAGES/daisy-player.mo
