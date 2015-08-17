#!/bin/sh
mkdir -p config m4
autoreconf --force --install -I config -I m4
./configure $*

