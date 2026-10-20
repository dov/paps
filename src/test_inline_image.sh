#!/bin/sh
# End-to-end: <img> reserves space in the line and draws the image.
# Usage: test_inline_image.sh /path/to/paps
paps=$(realpath "$1")
tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT

command -v pdftotext >/dev/null 2>&1 || exit 77
command -v base64 >/dev/null 2>&1 || exit 77

# A 1x1 PNG, drawn 50pt wide, relative to the input file
echo 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==' \
  | base64 -d > "$tmp/dot.png"
printf 'before <img src="dot.png" width="50" height="20"/> after\nplain\n' > "$tmp/in.txt"

# Run from another directory to check that names are relative to the input
(cd / && "$paps" --markup --font="Monospace 10" --format=pdf "$tmp/in.txt" > "$tmp/out.pdf" 2> "$tmp/err")
rc=$?
if [ $rc -ne 0 ] || [ -s "$tmp/err" ]; then
  echo "FAIL: rc=$rc"; cat "$tmp/err"; exit 1
fi

if command -v pdfimages >/dev/null 2>&1; then
  n=$(pdfimages -list "$tmp/out.pdf" | tail -n +3 | wc -l)
  [ "$n" -ge 1 ] || { echo "FAIL: no image in the PDF"; exit 1; }
fi

pdftotext -bbox "$tmp/out.pdf" "$tmp/out.html" || exit 1
xmax() { grep -o "xMax=\"[0-9.]*\"[^>]*>$1<" "$tmp/out.html" | head -1 | sed 's/xMax="\([0-9.]*\)".*/\1/'; }
xmin() { grep -o "xMin=\"[0-9.]*\"[^>]*>$1<" "$tmp/out.html" | head -1 | sed 's/xMin="\([0-9.]*\)".*/\1/'; }
before_end=$(xmax before)
after_start=$(xmin after)
[ -n "$before_end" ] && [ -n "$after_start" ] || { echo "FAIL: words not found"; exit 1; }

# The gap holds the 50pt image and the spaces around it
awk -v a="$before_end" -v b="$after_start" 'BEGIN { exit !(b - a > 50 && b - a < 70) }' \
  || { echo "FAIL: gap is $(awk -v a=$before_end -v b=$after_start 'BEGIN{print b-a}')"; exit 1; }

# A missing image warns but does not fail
printf '<img src="nope.png"/>x\n' > "$tmp/missing.txt"
"$paps" --markup --format=pdf "$tmp/missing.txt" > "$tmp/m.pdf" 2> "$tmp/err" || { echo "FAIL: missing image"; exit 1; }
grep -q nope.png "$tmp/err" || { echo "FAIL: no warning for missing image"; exit 1; }
exit 0
