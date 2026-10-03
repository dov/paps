#!/bin/sh
# End-to-end: --markup together with --cpi must not cut tags in half.
# Usage: test_markup_cpi.sh /path/to/paps
paps="$1"
tmp=$(mktemp -d) || exit 1
trap 'rm -rf "$tmp"' EXIT

printf '<b>%s</b>\n<span foreground="#006633">x\ny</span>\n' \
  "$(printf 'a%.0s' $(seq 1 200))" > "$tmp/in.txt"

for fmt in ps pdf; do
  "$paps" --markup --cpi=10 --format=$fmt "$tmp/in.txt" > "$tmp/out.$fmt" 2> "$tmp/err"
  rc=$?
  if [ $rc -ne 0 ] || [ -s "$tmp/err" ]; then
    echo "FAIL ($fmt): rc=$rc"; cat "$tmp/err"; exit 1
  fi
done

if command -v pdftotext >/dev/null 2>&1; then
  if pdftotext "$tmp/out.pdf" - | grep -q '[<>]'; then
    echo "FAIL: literal markup in output"; pdftotext "$tmp/out.pdf" -; exit 1
  fi
fi
exit 0
