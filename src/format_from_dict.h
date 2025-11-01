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
#ifndef FORMAT_FROM_DICT_H
#define FORMAT_FROM_DICT_H

#include <string>
#include <map>
#include <variant>
#include <fmt/chrono.h>

// Embed the time to disambiguate on systems where time_t==int 
struct paps_time_t {
  std::time_t time;
};

using scalar_t = std::variant<int, std::string, double, paps_time_t>;
using dict_t = std::map<std::string, scalar_t>;

// Take a python like format string and a dictionary and format
// it according to the format string.
std::string format_from_dict(const std::string& str,
                             dict_t dict);

#endif /* FORMAT_FROM_DICT */
