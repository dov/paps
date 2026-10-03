#pragma once
#include <cstddef>
#include <string>

// Make Pango markup that spans several lines valid on each line by closing
// open elements at every line break and reopening them on the next line.
std::string balance_markup_per_line(const char *text);

// Insert line breaks so that no line has more than `cols` visible columns.
// Tags take no columns and an entity (&amp;) takes one. Apply this before
// balance_markup_per_line() so that elements are re-opened on the new lines.
std::string wrap_markup_to_columns(const char *text, size_t cols);
