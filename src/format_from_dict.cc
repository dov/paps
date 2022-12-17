/* 
 * paps.cc: A postscript printing program using pango.
 *
 * Copyright (C) 2002, 2005, 2022 Dov Grobgeld <dov.grobgeld@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "format_from_dict.h"

using namespace std;
using namespace fmt;

static string scalar_to_string(scalar_t scalar,
                               const string& spec="")
{
  if (holds_alternative<string>(scalar))
  {
    auto val = get<string>(scalar);
    if (!spec.length())
      return val;
    return format(runtime(format("{{:{}}}", spec)), val);
  }
  if (holds_alternative<int>(scalar))
  {
    auto val = get<int>(scalar);
    if (!spec.length())
      return to_string(val);
    return format(runtime(format("{{:{}}}", spec)), val);
  }
  if (holds_alternative<double>(scalar))
  {
    auto val = get<double>(scalar);
    if (!spec.length())
      return to_string(val);
    return format(runtime(format("{{:{}}}", spec)), val);
  }
  if (holds_alternative<time_t>(scalar))
  {
    time_t val = get<time_t>(scalar);
    if (!spec.length())
      return to_string(val);
    return format(runtime(format("{{:{}}}", spec)), fmt::localtime(val));
  }
  throw runtime_error("Unrecognized type!"); // I shouldn't be here!
}

// Take a python like format string and a dictionary and format
// it according to the format string.
string format_from_dict(const string& str,
                        dict_t dict)
{
  string res;

  // Currenly no support for double {{ and double }}
  int pos=0;
  size_t len = str.size();
  while (true) {
    size_t start = str.find("{", pos);
    if (start == string::npos || start == len-1)
      break;
    if (str[start+1]=='{')
    {
      res += str.substr(pos, start+1);
      pos = start+1;
      continue;
    }
    size_t end = str.find("}", start);
    if (end == string::npos)
      throw runtime_error(format("No end brace for start {{ at {}", start));
    if (end < len-1 && str[end+1] == '}')
      throw runtime_error(format("Can't have double }}}} in formatting clause!", start));

    // TBD - fill in
    res += str.substr(pos, start-pos);

    string spec = str.substr(start+1, end-start-1);
    size_t colon_pos = spec.find(":");
    if (colon_pos == string::npos)
    {
      if (dict.count(spec) == 0)
        throw runtime_error(format("Can't find {} in dictionary!", spec));
      res += scalar_to_string(dict[spec]);
    }
    else
    {
      // Split on ':'
      string spec_format = spec.substr(colon_pos+1);
      spec = spec.substr(0, colon_pos);
      res += scalar_to_string(dict[spec], spec_format);
    }

    pos = end+1;
  }

  res += str.substr(pos);
  
  return res;
}

