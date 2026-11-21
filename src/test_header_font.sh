#!/bin/sh
# End-to-end: --header-font changes the header/footer font, header and footer
# strings accept Pango markup, and default parts are escaped.
# Usage: test_header_font.sh /path/to/paps
paps="$1"
tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT

printf 'body\n' > "$tmp/a&b.txt"

# Options must be accepted, and markup (incl. a filename with '&') must not
# produce Pango warnings.
"$paps" --header --footer --header-font="Serif Italic 20" \
  --footer-center='<b>bold</b> &amp; <span font_desc="Sans 8">small</span>' \
  --format=pdf "$tmp/a&b.txt" > "$tmp/o.pdf" 2> "$tmp/err" ||
  { echo "FAIL: paps failed"; cat "$tmp/err"; exit 1; }
[ -s "$tmp/err" ] && { echo "FAIL: warnings"; cat "$tmp/err"; exit 1; }

if command -v pdffonts >/dev/null && command -v pdftotext >/dev/null; then
  "$paps" --header --format=pdf "$tmp/a&b.txt" > "$tmp/d.pdf" || exit 1
  fonts() { pdffonts "$1" | awk 'NR>2{print $1}' | sed 's/^[A-Z]*+//' | sort | tr '\n' ' '; }
  [ "$(fonts "$tmp/d.pdf")" != "$(fonts "$tmp/o.pdf")" ] ||
    { echo "FAIL: --header-font had no effect"; exit 1; }
  txt=$(pdftotext "$tmp/o.pdf" - | tr '\n' ' ')
  case "$txt" in *bold*'&'*small*) ;; *) echo "FAIL: footer markup text: $txt"; exit 1;; esac
  pdftotext "$tmp/o.pdf" - | grep -q 'a&b.txt' ||
    { echo "FAIL: escaped filename"; exit 1; }
fi
exit 0
