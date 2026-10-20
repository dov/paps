#pragma once
#include <string>
#include <vector>

// Paragraph properties set with the paps markup element
//   <p indent="-24" left="24" align="justify">...</p>
// Lengths are in points. `left` is the margin of all lines; `indent` is the
// extra offset of the first line, so indent="-24" left="24" gives a hanging
// indent that starts at the column edge.
struct ParaProps {
  enum Align { ALIGN_DEFAULT, ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT, ALIGN_JUSTIFY };

  bool present = false;  // A <p> element applies to this paragraph
  double indent = 0;
  double left = 0;
  Align align = ALIGN_DEFAULT;
};

// Remove the <p> and </p> elements from `text`, which is paps markup with
// line breaks as paragraph separators. `props` receives one entry per line,
// in the order the lines are split into paragraphs.
//
// A <p> applies to the line it is on and ends at </p> or at the end of the
// line, whichever comes first. If a </p> follows on a later line (before the
// next <p>), the <p> applies to all the lines in between.
//
// Problems (bad attributes, misnested elements) are appended to `warnings`
// when it is not null; the text is still processed.
std::string extract_paragraph_props(const char *text,
                                    std::vector<ParaProps> &props,
                                    std::vector<std::string> *warnings = nullptr);
