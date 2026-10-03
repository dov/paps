#include <cctype>
#include <cwchar>
#include <string>
#include <utility>
#include <vector>
#include "markup_balance.h"

/* Pango markup is parsed one line at a time, so an element that spans
 * several lines would be unbalanced on each of them. Close every open
 * element at the end of each line and reopen it at the start of the next.
 */
std::string
balance_markup_per_line(const char *text)
{
  std::string out;
  std::vector<std::pair<std::string,std::string>> open; // name, full open tag
  const char *p = text;

  while (*p)
    {
      if (*p == '\n' || *p == '\r' || *p == '\f')
        {
          // close in reverse order before the line break
          for (auto it = open.rbegin(); it != open.rend(); ++it)
            out += "</" + it->first + ">";
          // keep \r\n together
          out += *p++;
          if (p[-1] == '\r' && *p == '\n')
            out += *p++;
          for (auto &o : open)
            out += o.second;
          continue;
        }
      if (*p != '<')
        {
          out += *p++;
          continue;
        }

      // Find the end of the tag, honoring quoted attribute values
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
      if (!*q)
        {
          out += p;  // unterminated tag, let pango complain
          break;
        }
      std::string tag(p, q + 1 - p);
      p = q + 1;
      out += tag;

      if (tag.size() < 3 || tag[1] == '!' || tag[1] == '?')
        continue;
      if (tag[1] == '/')
        {
          if (!open.empty())
            open.pop_back();
        }
      else if (tag[tag.size() - 2] != '/')
        {
          size_t e = 1;
          while (e < tag.size() && !isspace((unsigned char)tag[e]) && tag[e] != '>')
            e++;
          open.emplace_back(tag.substr(1, e - 1), tag);
        }
    }
  return out;
}

// Display width of a unicode code point; non-printable counts as 0.
static int
cp_width(unsigned cp)
{
  int w = wcwidth((wchar_t)cp);
  return w < 0 ? 0 : w;
}

std::string
wrap_markup_to_columns(const char *text, size_t cols)
{
  std::string out;
  size_t width = 0;
  const unsigned char *p = (const unsigned char *)text;

  while (*p)
    {
      if (*p == '\n' || *p == '\r' || *p == '\f')
        {
          out += *p++;
          width = 0;
          continue;
        }
      if (*p == '<')
        {
          // Tags take no columns. Honor quoted attribute values.
          const unsigned char *q = p + 1;
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
          out.append((const char *)p, q - p);
          p = q;
          continue;
        }

      // One visible character: an entity, or a UTF-8 sequence
      const unsigned char *q = p;
      unsigned cp = *p;
      if (*p == '&')
        {
          const unsigned char *e = p + 1;
          while (*e && *e != ';' && *e != '<' && *e != '&' && *e > ' ')
            e++;
          if (*e == ';')
            {
              q = e + 1;
              cp = 'x';  // entities are a single column
            }
          else
            q = p + 1;
        }
      else
        {
          int n = cp < 0x80 ? 1 : cp >= 0xf0 ? 4 : cp >= 0xe0 ? 3 : 2;
          cp = n == 1 ? cp : cp & (0xff >> (n + 1));
          q = p + 1;
          for (int i = 1; i < n && (*q & 0xc0) == 0x80; i++)
            cp = (cp << 6) | (*q++ & 0x3f);
        }

      int w = cp_width(cp);
      if (width > 0 && width + w > cols)
        {
          out += '\n';
          width = 0;
        }
      width += w;
      out.append((const char *)p, q - p);
      p = q;
    }
  return out;
}
