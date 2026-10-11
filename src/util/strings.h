#ifndef UTIL_STRINGS_H
#define UTIL_STRINGS_H

#include <string>
#include <string_view>
#include <vector>

#include <util/aliases.h>

namespace pi::util {

// Split sv on every occurrence of delim. Empty tokens are kept.
vec<strv> split(strv sv, char delim);

// Split sv into lines, handling \r\n and \n.
// Returned views do not include the line-ending characters.
// A trailing newline does not produce a trailing empty element.
vec<strv> split_lines(strv sv);

// Join parts with sep between consecutive elements.
str join(const vec<strv>& parts, strv sep);
str join(const vec<str>& parts, strv sep);

// Strip leading and trailing ASCII whitespace (\t \n \r \f \v space).
strv trim(strv sv);

// Return true iff sv starts/ends with the given prefix/suffix.
bool starts_with(strv sv, strv prefix);
bool ends_with(strv sv, strv suffix);

// Return a copy of s with every non-overlapping occurrence of from replaced by to.
// If from is empty, returns s unchanged.
str replace_all(strv text, strv from, strv to);

}  // namespace pi::util

#endif  // UTIL_STRINGS_H
