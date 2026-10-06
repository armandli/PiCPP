#ifndef UTIL_STRINGS_H
#define UTIL_STRINGS_H

#include <string>
#include <string_view>
#include <vector>

namespace pi::util {

// Split sv on every occurrence of delim. Empty tokens are kept.
std::vector<std::string_view> split(std::string_view sv, char delim);

// Split sv into lines, handling \r\n and \n.
// Returned views do not include the line-ending characters.
// A trailing newline does not produce a trailing empty element.
std::vector<std::string_view> split_lines(std::string_view sv);

// Join parts with sep between consecutive elements.
std::string join(const std::vector<std::string_view>& parts, std::string_view sep);
std::string join(const std::vector<std::string>& parts, std::string_view sep);

// Strip leading and trailing ASCII whitespace (\t \n \r \f \v space).
std::string_view trim(std::string_view sv);

// Return true iff sv starts/ends with the given prefix/suffix.
bool starts_with(std::string_view sv, std::string_view prefix);
bool ends_with(std::string_view sv, std::string_view suffix);

// Return a copy of s with every non-overlapping occurrence of from replaced by to.
// If from is empty, returns s unchanged.
std::string replace_all(std::string_view s, std::string_view from, std::string_view to);

}  // namespace pi::util

#endif  // UTIL_STRINGS_H
