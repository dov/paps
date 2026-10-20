// Test extract_paragraph_props: stripping <p> elements and the per-line
// properties they produce.
#include <cstdio>
#include <string>
#include <vector>
#include "paragraph_markup.h"

static int failures = 0;

static void expect(bool ok, const char *name, const char *what)
{
  if (!ok)
    {
      printf("FAIL %s: %s\n", name, what);
      failures++;
    }
}

int main()
{
  {
    std::vector<ParaProps> p;
    std::vector<std::string> w;
    std::string out = extract_paragraph_props(
        "<p indent=\"-24\" left=\"24\" align=\"justify\">hello <b>x</b></p>\nplain\n", p, &w);
    expect(out == "hello <b>x</b>\nplain\n", "attrs", "text not stripped");
    expect(p.size() == 3, "attrs", "line count");
    expect(p[0].present && p[0].indent == -24 && p[0].left == 24
           && p[0].align == ParaProps::ALIGN_JUSTIFY, "attrs", "values");
    expect(!p[1].present, "attrs", "second line must have no props");
    expect(w.empty(), "attrs", "unexpected warning");
  }
  {
    // Unclosed <p> ends at the end of the line
    std::vector<ParaProps> p;
    std::string out = extract_paragraph_props("<p align=\"right\">a\nb\n", p);
    expect(out == "a\nb\n", "unclosed", "text");
    expect(p[0].present && p[0].align == ParaProps::ALIGN_RIGHT, "unclosed", "line 1");
    expect(!p[1].present, "unclosed", "must not leak into line 2");
  }
  {
    // </p> on a later line makes the <p> span the lines in between
    std::vector<ParaProps> p;
    std::string out = extract_paragraph_props("<p left=\"10\">a\nb\nc</p>\nd\n", p);
    expect(out == "a\nb\nc\nd\n", "multiline", "text");
    expect(p[0].present && p[1].present && p[2].present, "multiline", "spanned lines");
    expect(!p[3].present, "multiline", "reset after </p>");
    expect(p[1].left == 10, "multiline", "value kept");
  }
  {
    // A new <p> before any </p> ends the previous one at the line end
    std::vector<ParaProps> p;
    extract_paragraph_props("<p left=\"1\">a\n<p left=\"2\">b</p>\nc\n", p);
    expect(p[0].present && p[0].left == 1, "restart", "line 1");
    expect(p[1].present && p[1].left == 2, "restart", "line 2");
    expect(!p[2].present, "restart", "line 3");
  }
  {
    // Elements may span paragraphs
    std::vector<ParaProps> p;
    std::vector<std::string> w;
    std::string out = extract_paragraph_props(
        "<span foreground=\"red\"><p left=\"5\">a</p>\n<p left=\"6\">b</p></span>\n", p, &w);
    expect(out == "<span foreground=\"red\">a\nb</span>\n", "span", "text");
    expect(w.empty(), "span", "unexpected warning");
  }
  {
    std::vector<ParaProps> p;
    std::vector<std::string> w;
    extract_paragraph_props("<p><b>a</p></b>\n", p, &w);
    expect(w.size() == 1, "misnested", "expected a warning");
  }
  {
    std::vector<ParaProps> p;
    std::vector<std::string> w;
    extract_paragraph_props("<p align=\"middle\" foo=\"1\" left=\"x\">a</p>\n</p>\n", p, &w);
    expect(w.size() == 4, "bad attributes", "expected 4 warnings");
  }
  {
    // \r\n counts as one line break, like when splitting paragraphs
    std::vector<ParaProps> p;
    extract_paragraph_props("<p left=\"3\">a</p>\r\nb\r\n", p);
    expect(p.size() == 3 && p[0].present && !p[1].present, "crlf", "line mapping");
  }
  {
    // A '>' inside a quoted value does not end the tag
    std::vector<ParaProps> p;
    std::string out = extract_paragraph_props("<span font=\"a>b\">x</span>\n", p);
    expect(out == "<span font=\"a>b\">x</span>\n", "quoted", "text");
  }
  if (!failures)
    printf("all paragraph markup tests passed\n");
  return failures ? 1 : 0;
}
