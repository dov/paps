#!/bin/sh
# End-to-end: the <p> element positions lines.
# Usage: test_paragraph_p.sh /path/to/paps
paps="$1"
tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT

long="alpha beta gamma delta epsilon zeta eta theta iota kappa lambda mu nu xi omicron pi rho sigma tau"
printf '<p indent="-24" left="24" align="justify">%s</p>\nplain\n<p align="right">r</p>\n' \
  "$long" > "$tmp/in.txt"

"$paps" --markup --format=pdf --font="Monospace 10" "$tmp/in.txt" > "$tmp/out.pdf" 2> "$tmp/err"
rc=$?
if [ $rc -ne 0 ] || [ -s "$tmp/err" ]; then
  echo "FAIL: rc=$rc"; cat "$tmp/err"; exit 1
fi

command -v pdftotext >/dev/null 2>&1 || exit 77
pdftotext -bbox "$tmp/out.pdf" "$tmp/out.html" || exit 1

# x position of the first word of each text line
xmin() { grep -o "xMin=\"[0-9.]*\"[^>]*>$1<" "$tmp/out.html" | head -1 | sed 's/xMin="\([0-9.]*\)".*/\1/'; }
x_first=$(xmin alpha)
x_plain=$(xmin plain)
x_right=$(xmin r)
[ -n "$x_first" ] && [ -n "$x_plain" ] && [ -n "$x_right" ] || { echo "FAIL: words not found"; exit 1; }

# The hanging line starts 24pt left of where the continuation lines start,
# and the plain line starts at the left margin like the hanging first line.
awk -v a="$x_first" -v b="$x_plain" -v r="$x_right" \
  'BEGIN { d = a - b; if (d < 0) d = -d; exit !(d < 0.5 && r > b + 100) }' \
  || { echo "FAIL: first=$x_first plain=$x_plain right=$x_right"; exit 1; }
# Continuation lines are 24pt right of the first line
x_cont=$(grep -o 'xMin="[0-9.]*" yMin="[0-9.]*"' "$tmp/out.html" | sed 's/xMin="\([0-9.]*\)" yMin="\([0-9.]*\)"/\2 \1/' \
  | sort -n -k1,1 -k2,2n | awk '!($1 in m) {m[$1]=$2; n++; if (n==2) {print $2; exit}}')
awk -v c="$x_cont" -v b="$x_plain" 'BEGIN { d = c - b - 24; if (d < 0) d = -d; exit !(d < 0.5) }' \
  || { echo "FAIL: continuation at $x_cont, expected $(awk -v b=$x_plain 'BEGIN{print b+24}')"; exit 1; }
if grep -q '[<>]' "$tmp/out.html" && grep -q '&lt;p' "$tmp/out.html"; then
  echo "FAIL: literal <p> in output"; exit 1
fi
exit 0
