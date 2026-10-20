#include <cctype>
#include <cstdlib>
#include <utility>
#include "paragraph_markup.h"

namespace {

struct Token {
  enum Kind { TEXT, TAG, BREAK } kind;
  std::string s;
};

bool is_break(char c) { return c == '\n' || c == '\r' || c == '\f'; }

// Split the text into text runs, tags and line breaks. A tag ends at the
// first '>' outside of quotes, as in balance_markup_per_line().
std::vector<Token> tokenize(const char *text)
{
  std::vector<Token> toks;
  const char *p = text;
  while (*p)
    {
      if (is_break(*p))
        {
          std::string br(1, *p++);
          if (br[0] == '\r' && *p == '\n')
            br += *p++;
          toks.push_back({Token::BREAK, br});
        }
      else if (*p == '<')
        {
          const char *q = p + 1;
          char quote = 0;
          while (*q && (quote || *q != '>'))
            {
              if (quote)
                {
                  if (*q == quote)
                    quote = 0;
                }
              else if (*q == '"' || *q == '\'')
                quote = *q;
              q++;
            }
          if (*q)
            q++;
          toks.push_back({Token::TAG, std::string(p, q - p)});
          p = q;
        }
      else
        {
          const char *q = p;
          while (*q && *q != '<' && !is_break(*q))
            q++;
          toks.push_back({Token::TEXT, std::string(p, q - p)});
          p = q;
        }
    }
  return toks;
}

// The element name of a tag, e.g. "p" for "<p a='b'>" and "</p>".
std::string tag_name(const std::string &tag)
{
  size_t b = tag.size() > 1 && tag[1] == '/' ? 2 : 1;
  size_t e = b;
  while (e < tag.size() && !isspace((unsigned char)tag[e])
         && tag[e] != '>' && tag[e] != '/')
    e++;
  return tag.substr(b, e - b);
}

bool is_close_tag(const std::string &tag) { return tag.size() > 1 && tag[1] == '/'; }

bool is_empty_tag(const std::string &tag)
{
  return tag.size() > 2 && tag[tag.size() - 2] == '/';
}

bool parse_number(const std::string &v, double &out)
{
  const char *s = v.c_str();
  char *end;
  out = strtod(s, &end);
  return end != s && *end == '\0';
}

using Attributes = std::vector<std::pair<std::string, std::string>>;

// Parse name="value" pairs of a tag, starting after the element name.
Attributes parse_attributes(const std::string &tag, size_t i, const char *element,
                            std::vector<std::string> *warnings)
{
  auto warn = [&](const std::string &m) {
    if (warnings) warnings->push_back(std::string("<") + element + ">: " + m);
  };
  Attributes attrs;
  while (i < tag.size())
    {
      while (i < tag.size() && (isspace((unsigned char)tag[i]) || tag[i] == '/'))
        i++;
      if (i >= tag.size() || tag[i] == '>')
        break;
      size_t b = i;
      while (i < tag.size() && !isspace((unsigned char)tag[i]) && tag[i] != '=' && tag[i] != '>')
        i++;
      std::string name = tag.substr(b, i - b);
      if (i >= tag.size() || tag[i] != '=' || i + 1 >= tag.size()
          || (tag[i + 1] != '"' && tag[i + 1] != '\''))
        {
          warn("attribute '" + name + "' needs a quoted value");
          break;
        }
      char quote = tag[i + 1];
      size_t vb = i + 2;
      size_t ve = tag.find(quote, vb);
      if (ve == std::string::npos)
        {
          warn("unterminated value for attribute '" + name + "'");
          break;
        }
      attrs.emplace_back(name, tag.substr(vb, ve - vb));
      i = ve + 1;
    }
  return attrs;
}

// Parse the attributes of a <p ...> tag into props.
void parse_p_attributes(const std::string &tag, ParaProps &props,
                        std::vector<std::string> *warnings)
{
  auto warn = [&](const std::string &m) { if (warnings) warnings->push_back("<p>: " + m); };
  for (auto &nv : parse_attributes(tag, 2, "p", warnings))
    {
      const std::string &name = nv.first, &value = nv.second;
      if (name == "indent" || name == "left")
        {
          double d;
          if (!parse_number(value, d))
            {
              warn("bad number '" + value + "' for attribute '" + name + "'");
              continue;
            }
          (name == "indent" ? props.indent : props.left) = d;
        }
      else if (name == "align")
        {
          if (value == "left") props.align = ParaProps::ALIGN_LEFT;
          else if (value == "center") props.align = ParaProps::ALIGN_CENTER;
          else if (value == "right") props.align = ParaProps::ALIGN_RIGHT;
          else if (value == "justify") props.align = ParaProps::ALIGN_JUSTIFY;
          else
            warn("bad align '" + value + "', expected left, center, right or justify");
        }
      else
        warn("unknown attribute '" + name + "'");
    }
}

// Parse the attributes of an <img .../> tag.
ImageSpec parse_img_attributes(const std::string &tag,
                               std::vector<std::string> *warnings)
{
  auto warn = [&](const std::string &m) { if (warnings) warnings->push_back("<img>: " + m); };
  ImageSpec img;
  for (auto &nv : parse_attributes(tag, 4, "img", warnings))
    {
      const std::string &name = nv.first, &value = nv.second;
      if (name == "src")
        img.src = value;
      else if (name == "width" || name == "height")
        {
          double d;
          if (!parse_number(value, d) || d <= 0)
            {
              warn("bad size '" + value + "' for attribute '" + name + "'");
              continue;
            }
          (name == "width" ? img.width : img.height) = d;
        }
      else
        warn("unknown attribute '" + name + "'");
    }
  if (img.src.empty())
    warn("missing src");
  return img;
}

}  // namespace

std::string
extract_paragraph_props(const char *text,
                        std::vector<ParaProps> &props,
                        std::vector<std::string> *warnings,
                        std::vector<std::vector<ImageSpec>> *images)
{
  auto warn = [&](const std::string &m) { if (warnings) warnings->push_back(m); };
  std::vector<Token> toks = tokenize(text);
  std::string out;

  ParaProps active;     // From the open <p>, if any
  bool p_open = false;
  size_t p_depth = 0;   // Depth of the other open elements at the <p>
  std::vector<std::string> stack;
  ParaProps line = active;
  std::vector<ImageSpec> line_images;

  for (size_t t = 0; t < toks.size(); t++)
    {
      const Token &tok = toks[t];
      if (tok.kind == Token::TEXT)
        {
          out += tok.s;
          continue;
        }
      if (tok.kind == Token::BREAK)
        {
          props.push_back(line);
          if (images)
            images->push_back(std::move(line_images));
          line_images.clear();
          out += tok.s;
          if (p_open)
            {
              // Stay open only if a </p> follows before the next <p>
              bool closes = false;
              for (size_t u = t + 1; u < toks.size(); u++)
                {
                  if (toks[u].kind != Token::TAG || tag_name(toks[u].s) != "p")
                    continue;
                  closes = is_close_tag(toks[u].s);
                  break;
                }
              if (!closes)
                {
                  p_open = false;
                  active = ParaProps();
                }
            }
          line = active;
          continue;
        }

      const std::string name = tag_name(tok.s);
      if (name == "img")
        {
          if (is_close_tag(tok.s))
            warn("</img> without <img>");
          else
            {
              // A placeholder character; the image is attached to it later
              out += OBJECT_REPLACEMENT;
              ImageSpec img = parse_img_attributes(tok.s, warnings);
              if (images)
                line_images.push_back(img);
            }
          continue;
        }
      if (name != "p")
        {
          out += tok.s;
          if (tok.s.size() < 3 || tok.s[1] == '!' || tok.s[1] == '?')
            ;
          else if (is_close_tag(tok.s))
            {
              if (!stack.empty())
                stack.pop_back();
            }
          else if (!is_empty_tag(tok.s))
            stack.push_back(name);
          continue;
        }

      if (is_close_tag(tok.s))
        {
          if (!p_open)
            warn("</p> without matching <p>");
          else if (stack.size() != p_depth)
            warn("</p> is not nested properly with the other elements");
          p_open = false;
          active = ParaProps();
        }
      else
        {
          active = ParaProps();
          active.present = true;
          parse_p_attributes(tok.s, active, warnings);
          p_open = true;
          p_depth = stack.size();
          line = active;
        }
    }
  props.push_back(line);
  if (images)
    images->push_back(std::move(line_images));
  return out;
}
