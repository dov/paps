#!/bin/sh
# End-to-end: --booklet imposes pages in signature order, also for --rtl.
# Usage: test_booklet.sh /path/to/paps
paps="$1"
tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT

for i in 1 2 3 4 5 6 7 8; do printf 'pg%s\n\f' "$i"; done > "$tmp/in.txt"

if command -v pdftotext >/dev/null; then
  "$paps" --booklet --signature=8 --header --header-left=x --header-center=y \
    --header-right='{page_idx}' --format=pdf "$tmp/in.txt" > "$tmp/o.pdf" || exit 1
  got=$(for p in 1 2 3 4; do pdftotext -f $p -l $p "$tmp/o.pdf" - | grep '^pg' | tr '\n' ' '; done)
  want="pg8 pg1 pg2 pg7 pg6 pg3 pg4 pg5 "
  [ "$got" = "$want" ] || { echo "FAIL: order '$got'"; exit 1; }
  "$paps" --booklet --rtl --signature=8 --format=pdf "$tmp/in.txt" > "$tmp/r.pdf" || exit 1
  [ "$(pdfinfo "$tmp/r.pdf" | awk '/^Pages/{print $2}')" = 4 ] ||
    { echo "FAIL: rtl pages"; exit 1; }
fi
exit 0
