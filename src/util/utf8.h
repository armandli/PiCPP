#ifndef UTIL_UTF8_H
#define UTIL_UTF8_H

#include <cstddef>
#include <optional>
#include <string_view>

namespace pi::util {

// Decode the next UTF-8 code point starting at s[pos].
// On success: fills cp, advances pos by 1–4, returns true.
// On error (invalid or truncated sequence): returns std::nullopt, pos unchanged.
std::optional<char32_t> decode_utf8(std::string_view s, std::size_t& pos);

// Return true iff s is entirely valid UTF-8
// (no overlong encodings, no surrogates, no code points above U+10FFFF).
bool is_valid_utf8(std::string_view s);

// Count the number of Unicode code points in s.
// Stops at the first invalid byte and counts only the valid prefix.
std::size_t count_code_points(std::string_view s);

// Return the number of terminal display cells required to render s.
// ASCII printable: 1 cell. East-Asian wide (W/F): 2 cells.
// Non-printable / control / combining (M/Mn): 0 cells.
std::size_t display_width(std::string_view s);

}  // namespace pi::util

#endif  // UTIL_UTF8_H
