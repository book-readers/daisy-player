#!/bin/sh

set -xue

export LC_ALL=C
export TERM=dumb

test -d "$abs_srcdir"

if test "$1" = '--generate'; then
  generate=true
  shift
  actual="$2"
else
  generate=false
  trap 'rm -f "$actual"' EXIT INT QUIT TERM
  actual=$(mktemp)
fi

expected="$2"
input="${expected%.*}"

# try and make abs_srcdir safe to use as a sed expression
sed_safe_abs_srcdir=$(printf %s "${abs_srcdir}" | sed 's%[].\[*]%\\\0%g')

# run daisy-player -D
"$1" -D "$input" | sed "
# strip the header
1,5d
# remove the source dir part of the path
s%${sed_safe_abs_srcdir}%%g
# sanitize path
s%/[.]/%%
" > "$actual"

# compare the result with the expectations
$generate || diff -u "$expected" "$actual"
