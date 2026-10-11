#ifndef UTIL_UTF8_H
#define UTIL_UTF8_H

#include <cstddef>
#include <optional>
#include <string_view>

#include <util/aliases.h>

namespace pi::util {

// One decoded code point and the byte offset just past it.
struct DecodedCodePoint {
  char32_t code_point;
  s::size_t next;  // position of the byte after this code point's sequence
  bool operator==(const DecodedCodePoint&) const = default;
};

// Decode the UTF-8 code point starting at s[pos].
// On success returns the code point and the position after it (pos + 1..4).
// Returns std::nullopt if pos >= text.size() or the sequence is invalid
// (bad lead/continuation byte, truncated, overlong, surrogate, > U+10FFFF).
opt<DecodedCodePoint> decode_utf8(strv text, s::size_t pos);

// Return true iff s is entirely valid UTF-8
// (no overlong encodings, no surrogates, no code points above U+10FFFF).
bool is_valid_utf8(strv text);

// Count the number of Unicode code points in s.
// Stops at the first invalid byte and counts only the valid prefix.
s::size_t count_code_points(strv text);

// Return the number of terminal display cells required to render s.
// ASCII printable: 1 cell. East-Asian wide (W/F): 2 cells.
// Non-printable / control / combining (M/Mn): 0 cells.
// Emoji sequences (ZWJ-joined, skin-tone modified, flag pairs, VS16) count as one 2-cell glyph.
s::size_t display_width(strv text);

}  // namespace pi::util

#endif  // UTIL_UTF8_H
