# About

paps is a command line program for converting Unicode text encoded in UTF-8
to postscript and pdf by using pango.

# Example

Here is the output from processing the file misc/small-hello.utf8:

![Example image](misc/small-hello.png)

## History

paps was written around 2005 to enable printing of plain text unicode UTF-8 files. It is named by the use of the excellent Pango library (from which it took its first two characters) and outputed postscript (ps, the last two characters). When the initial version was written, there was no simple way of translating the output of pango to postscript outlines, and therefore Ι wrote my own font encoding library, by making use of the fact that postscript is a full programming language. This worked well when sending the resulting postscript to the printer, but it had some serious disadvantages:

- When converting to pdf you got huge files (two orders of magnitude larger!) since in contrast to postscript, pdf is not a programming language, and thus it encoded a path for each glyph.
- You couldn't select and copy and paste from the resulting postscript file, since there was no underlying text, only graphic paths.

In the early 2010's the library cairo and its accompanying pangocairo library finally created an easy way of converting pango output to postscript in an idiomatic way. But it took me several years until I finally rewrote paps to make use of them. 

But I finally did so and released the resulting version on github in 2015 as version 0.7.0 . 

But even so the new cairo based version was not picked up! E.g. Fedora still supplies the old 0.6.8 version without cairo. Perhaps because I did not update the old SourceForge page.

# Usage

Run `paps --help` for getting help.

