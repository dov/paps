#!/bin/sh
mkdir -p config m4

echo "no" | glib-gettextize --force --copy
intltoolize --copy --force --automake

autoreconf --force --install -I config -I m4
./configure $*

