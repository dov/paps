// Test that balance_markup_per_line makes multi-line Pango markup valid
// on every individual line.
#include <clocale>
#include <cstdio>
#include <string>
#include <vector>
#include <pango/pango.h>
#include "markup_balance.h"

static int failures = 0;

static std::vector<std::string> split_lines(const std::string &s)
{
  std::vector<std::string> lines;
  std::string cur;
  for (size_t i = 0; i < s.size(); i++) {
    char c = s[i];
    if (c == '\r' && i + 1 < s.size() && s[i + 1] == '\n')
      continue;  // \r\n is a single line break
    if (c == '\n' || c == '\r' || c == '\f') {
      lines.push_back(cur);
      cur.clear();
    } else
      cur += c;
  }
  lines.push_back(cur);
  return lines;
}

static bool line_parses(const std::string &line, std::string &plain)
{
  GError *err = nullptr;
  char *text = nullptr;
  bool ok = pango_parse_markup(line.c_str(), -1, 0, nullptr, &text, nullptr, &err);
  if (ok) plain = text;
  g_free(text);
  if (err) g_error_free(err);
  return ok;
}

// Every line of the balanced output must parse, and the concatenated
// plain text must equal `expected_plain` (lines joined by '\n').
static void check_valid(const char *name, const char *input, const char *expected_plain)
{
  std::string out = balance_markup_per_line(input);
  std::string joined;
  bool first = true;
  for (auto &line : split_lines(out)) {
    std::string plain;
    if (!line_parses(line, plain)) {
      fprintf(stderr, "FAIL %s: line does not parse: '%s'\n", name, line.c_str());
      failures++;
      return;
    }
    if (!first) joined += "\n";
    joined += plain;
    first = false;
  }
  if (joined != expected_plain) {
    fprintf(stderr, "FAIL %s: expected plain '%s', got '%s'\n", name, expected_plain, joined.c_str());
    failures++;
  }
}

static void check_exact(const char *name, const char *input, const char *expected)
{
  std::string out = balance_markup_per_line(input);
  if (out != expected) {
    fprintf(stderr, "FAIL %s: expected '%s', got '%s'\n", name, expected, out.c_str());
    failures++;
  }
}

static void check_wrap(const char *name, const char *input, size_t cols, const char *expected)
{
  std::string out = wrap_markup_to_columns(input, cols);
  if (out != expected) {
    fprintf(stderr, "FAIL %s: expected '%s', got '%s'\n", name, expected, out.c_str());
    failures++;
  }
}

// Wrap, then balance, then require every line to parse and be at most
// `cols` visible characters wide.
static void check_wrap_valid(const char *name, const char *input, size_t cols,
                             const char *expected_plain)
{
  std::string out = balance_markup_per_line(wrap_markup_to_columns(input, cols).c_str());
  std::string joined;
  bool first = true;
  for (auto &line : split_lines(out)) {
    std::string plain;
    if (!line_parses(line, plain)) {
      fprintf(stderr, "FAIL %s: line does not parse: '%s'\n", name, line.c_str());
      failures++;
      return;
    }
    if (g_utf8_strlen(plain.c_str(), -1) > (glong)cols) {
      fprintf(stderr, "FAIL %s: line too wide: '%s'\n", name, plain.c_str());
      failures++;
    }
    if (!first) joined += "\n";
    joined += plain;
    first = false;
  }
  if (joined != expected_plain) {
    fprintf(stderr, "FAIL %s: expected plain '%s', got '%s'\n", name, expected_plain, joined.c_str());
    failures++;
  }
}

int main()
{
  setlocale(LC_ALL, "C.UTF-8");

  // --- CPI clipping: tags take no columns ---
  check_wrap("wrap_plain", "abcdef\n", 3, "abc\ndef\n");
  check_wrap("wrap_fits", "abc\n", 3, "abc\n");
  check_wrap("wrap_tags_free", "<b>abc</b>def\n", 3, "<b>abc</b>\ndef\n");
  check_wrap("wrap_tag_attr_gt", "<span a=\"x>y\">abcd</span>\n", 2,
             "<span a=\"x>y\">ab\ncd</span>\n");
  check_wrap("wrap_entity_one_col", "&amp;&lt;&gt;&amp;\n", 2, "&amp;&lt;\n&gt;&amp;\n");
  check_wrap("wrap_resets_per_line", "ab\ncd\n", 2, "ab\ncd\n");
  check_wrap("wrap_crlf", "ab\r\ncd", 2, "ab\r\ncd");
  check_wrap("wrap_wide_chars", "漢字漢字", 4, "漢字\n漢字");
  check_wrap("wrap_utf8", "שלום עולם", 4, "שלום\n עול\nם");
  check_wrap("wrap_zero_cols_progress", "abc", 0, "a\nb\nc");
  check_wrap("wrap_unterminated_tag", "ab<b", 1, "a\nb<b");

  // The case that produced a literal "</b>" and a Pango warning
  check_wrap_valid("wrap_long_bold",
                   "<b>aaaaaaaaaa</b>\n", 4, "aaaa\naaaa\naa\n");
  // Wrap inside nested elements: they must be re-opened on each line
  check_wrap_valid("wrap_nested",
                   "<b><i>abcdefg</i></b>\n", 3, "abc\ndef\ng\n");
  check_wrap_valid("wrap_span_crossing",
                   "<span foreground=\"#006633\">abcde\nfgh</span>\n", 2,
                   "ab\ncd\ne\nfg\nh\n");
  check_wrap_valid("wrap_entities",
                   "<b>&amp;&amp;&amp;</b>", 2, "&&\n&");

  // The issue #62 case
  check_valid("issue62",
              "<span foreground=\"#006633\">something\n</span>\n<span foreground=\"#aa6633\">something else</span>\n",
              "something\n\nsomething else\n");
  check_exact("issue62_exact",
              "<span foreground=\"#006633\">a\nb</span>\n",
              "<span foreground=\"#006633\">a</span>\n<span foreground=\"#006633\">b</span>\n");

  // No markup crossing lines: output unchanged
  check_exact("unchanged", "<b>a</b>\nplain\n", "<b>a</b>\nplain\n");
  check_exact("empty", "", "");

  // Nested elements across several lines, closed in reverse order
  check_exact("nested",
              "<b><i>x\ny\nz</i></b>\n",
              "<b><i>x</i></b>\n<b><i>y</i></b>\n<b><i>z</i></b>\n");
  check_valid("nested_partial_close",
              "<b>a<i>b\nc</i>d\ne</b>\n", "ab\ncd\ne\n");

  // Blank lines inside an element
  check_valid("blank_lines", "<b>a\n\n\nb</b>", "a\n\n\nb");

  // '>' inside a quoted attribute value must not end the tag
  check_valid("gt_in_attr_dq",
              "<span font_desc=\"a>b\">x\ny</span>", "x\ny");
  check_valid("gt_in_attr_sq",
              "<span font_desc='a>b'>x\ny</span>", "x\ny");
  // A quote of the other kind inside a quoted value
  check_valid("mixed_quotes",
              "<span font_desc=\"it's\">x\ny</span>", "x\ny");

  // Self-closing elements must not be treated as opened
  check_exact("selfclose", "<span/>a\nb\n", "<span/>a\nb\n");
  check_exact("selfclose_space", "<span />a\nb\n", "<span />a\nb\n");
  check_valid("selfclose_inside",
              "<b>a<span/>\nb</b>", "a\nb");

  // Comments and processing instructions are not elements
  check_exact("comment", "<!-- hi -->a\nb\n", "<!-- hi -->a\nb\n");
  check_exact("pi", "<?xml version=\"1.0\"?>a\nb\n", "<?xml version=\"1.0\"?>a\nb\n");

  // Line break flavours
  check_valid("crlf", "<b>a\r\nb</b>\r\n", "a\nb\n");
  check_exact("crlf_exact", "<b>a\r\nb</b>", "<b>a</b>\r\n<b>b</b>");
  check_valid("formfeed", "<b>a\fb</b>", "a\nb");
  check_exact("cr_only", "<b>a\rb</b>", "<b>a</b>\r<b>b</b>");

  // Entities and escaped markup characters
  check_valid("entities", "<b>a &lt;b&gt; &amp;\nc</b>", "a <b> &\nc");

  // UTF-8 text, including multi-byte characters next to tags
  check_valid("utf8", "<b>שלום\nעולם</b>", "שלום\nעולם");

  // Element closed on the same line it is opened, then another opened
  check_valid("sequence",
              "<b>a</b><i>b\nc</i><u>d\ne</u>", "ab\ncd\ne");

  // Many levels deep
  check_valid("deep",
              "<b><i><u><s>a\nb\nc\nd</s></u></i></b>", "a\nb\nc\nd");

  // Malformed input must not crash or hang; Pango reports the error later.
  // Unterminated tag:
  balance_markup_per_line("<b>a\n<span");
  balance_markup_per_line("<");
  balance_markup_per_line("<b");
  balance_markup_per_line("<span a=\"unterminated>x\ny");
  // Stray closing tag with nothing open:
  balance_markup_per_line("</b>a\nb");
  // More closes than opens
  balance_markup_per_line("<b>a</b></b>\nb");
  // Unclosed element at end of input
  check_exact("unclosed_eof", "<b>a", "<b>a");

  if (failures) {
    fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }
  return 0;
}
