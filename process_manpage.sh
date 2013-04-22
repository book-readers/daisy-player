#!/bin/sh

txt2man -p ../doc/$1.txt > $1.1
man2html $1.1 | tail -n +3 > ../doc/$1.html
