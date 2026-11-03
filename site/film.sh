#!/bin/sh
# Rebuilds the hero's frames from the real renderer. Run after anything drawn on the panel changes.
set -e
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
c++ -std=c++20 -I test/stubs -I include -I lib/TermBuffer -I lib/TermRenderer -I lib/TermFont site/film.cpp \
  lib/TermRenderer/TermRenderer.cpp lib/TermBuffer/TermBuffer.cpp -o "$tmp/film"
"$tmp/film" "$tmp"
uvx -q --with pillow python site/film.py "$tmp"
rm -r "$tmp"
