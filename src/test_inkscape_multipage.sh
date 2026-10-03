#!/bin/sh
# End-to-end: --inkscape-multipage places pages side by side with
# inkscape:page entries; it is rejected for non-SVG output.
# Usage: test_inkscape_multipage.sh /path/to/paps
paps="$1"
tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT

seq 1 400 > "$tmp/in.txt"

"$paps" --format=svg --inkscape-multipage --page-gap=10 "$tmp/in.txt" \
  > "$tmp/out.svg" 2> "$tmp/err"
if [ $? -ne 0 ] || [ -s "$tmp/err" ]; then
  echo "FAIL: rc/stderr"; cat "$tmp/err"; exit 1
fi

pages=$(grep -c '<inkscape:page ' "$tmp/out.svg")
groups=$(grep -c '<g id="page[0-9]*" transform=' "$tmp/out.svg")
if [ "$pages" -lt 2 ] || [ "$pages" -ne "$groups" ]; then
  echo "FAIL: pages=$pages groups=$groups"; exit 1
fi
if grep -q '<pageSet>\|<page>' "$tmp/out.svg"; then
  echo "FAIL: cairo pageSet left in output"; exit 1
fi
# The second page starts after the first page width plus the gap.
if ! grep '<inkscape:page ' "$tmp/out.svg" | sed -n '1,2p' |
     sed 's/.* x="\([0-9.]*\)" y="0" width="\([0-9.]*\)".*/\1 \2/' |
     awk 'NR==1{w=$2} NR==2{x=$1} END{d=x-w-10; exit (d<0.01 && d>-0.01) ? 0 : 1}'; then
  echo "FAIL: page gap"; exit 1
fi
if command -v xmllint >/dev/null 2>&1; then
  xmllint --noout "$tmp/out.svg" || { echo "FAIL: malformed xml"; exit 1; }
fi

if "$paps" --format=pdf --inkscape-multipage "$tmp/in.txt" >/dev/null 2>&1; then
  echo "FAIL: --inkscape-multipage accepted for pdf"; exit 1
fi
exit 0
