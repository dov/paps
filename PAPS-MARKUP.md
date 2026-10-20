# paps markup

With `--markup`, paps reads its input as [Pango markup](https://docs.gtk.org/Pango/pango_markup.html):
`<b>`, `<i>`, `<span foreground="red">` and so on. Pango markup cannot set the
indent or alignment of a paragraph and cannot include images, so paps adds two
elements, together called *paps markup*:

| Element | Purpose |
| ------- | ------- |
| [`<p>`](#p-paragraph-layout) | Indent, left margin and alignment of a paragraph |
| [`<img/>`](#img-images)      | An inline PNG image |

Everything else is passed to Pango unchanged, so any valid Pango markup is
also valid paps markup. Lengths are in points (1/72 inch).

The examples below are in [`examples/paps-markup.txt`](examples/paps-markup.txt):

    cd examples
    paps --markup --format=pdf paps-markup.txt > paps-markup.pdf

## Paragraphs and lines

paps treats every line of the input as a paragraph, which is wrapped to the
column width on its own. The `<p>` element applies to those paragraphs.

Pango elements such as `<b>` and `<span>` may continue over line breaks. paps
closes and reopens them at each break, so
`<span foreground="red">one` / `two</span>` is red on both lines.

## `<p>`: paragraph layout

    <p indent="-24" left="24" align="justify">text</p>

| Attribute | Value | Meaning |
| --------- | ----- | ------- |
| `left`    | points, default 0 | Margin of all lines of the paragraph |
| `indent`  | points, default 0 | Extra offset of the first line, relative to `left` |
| `align`   | `left`, `center`, `right` or `justify` | Alignment of the lines. `justify` leaves the last line left aligned. Without `align` the paragraph is aligned as the rest of the text, including the global `--justify`. |

The first line starts at `left` + `indent` and the other lines at `left`.
A negative `indent` is therefore a hanging indent: the example above starts the
first line at the column edge and indents the following lines by 24 points.
The text wraps at the right edge of the column.

### Where a `<p>` ends

A `<p>` ends at its `</p>` or at the end of its line, whichever comes first.
Settings never leak into the next line, so `</p>` is optional for one-line
paragraphs:

    <p align="center">A centered title
    This line is aligned as usual.

If a `</p>` follows on a later line before the next `<p>`, the `<p>` applies to
all the lines in between. Each line is still its own paragraph, so a hanging
indent restarts on every line:

    <p left="36">These three
    lines are all
    indented.</p>

A `<p>` replaces the settings of an earlier open `<p>`; they do not add up.

### Nesting

`<p>` can contain other elements, and can be inside a `<span>` that continues
over several paragraphs:

    <span foreground="blue"><p left="20">one</p>
    <p left="40">two</p></span>

The elements must be properly nested: `<p><b>x</p></b>` gives a warning.

### Right-to-left

With `--rtl`, `left` and `indent` apply on the right side, the start of the
line, and `align="left"` and `align="right"` mean the visual left and right.

## `<img/>`: images

    <img src="logo.png" width="100" height="50"/>

| Attribute | Value | Meaning |
| --------- | ----- | ------- |
| `src`     | file name (required) | A PNG file. A relative name is relative to the input file, or to the current directory when reading from stdin. |
| `width`   | points | Width of the image |
| `height`  | points | Height of the image |

- With only one of `width` and `height` the other follows the aspect ratio.
  With neither, one pixel is one point.
- An image wider than the column, or taller than the column height, is scaled
  down to fit, keeping its aspect ratio.
- The image is a character in the line, with its bottom edge on the baseline.
  It can be in the middle of a text line, which then becomes taller. A line
  with only an image is an image on its own, which can be positioned with
  `<p align="center">`.
- The `<img>` may be written `<img .../>` or `<img ...>`; there is no `</img>`.
- If an image cannot be loaded paps prints a warning and leaves it out.

### Limitations

- Only PNG is supported.
- With `--lpi` or `--geometry` the rows have a fixed height, and an image taller
  than a row overlaps the next rows.
- `--stretch-chars` also stretches images vertically.
- `--cpi` does not take the width of images into account when clipping lines.
- An image is not split over pages. One taller than the column is scaled down,
  but the header and footer height are not subtracted, so with a header it can
  run slightly out of the column.
- A literal U+FFFC (object replacement) character in the input is taken as the
  place of an image.

## Errors

Problems with paps markup are written to standard error and paps continues:
unknown or malformed attributes, a `</p>` without a `<p>`, misnested `<p>`, an
`<img>` without `src`, and image files that cannot be read. Invalid Pango markup
is reported by Pango as usual.
