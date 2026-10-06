#include <util/utf8.h>

namespace pi::util {

std::optional<char32_t> decode_utf8(std::string_view s, std::size_t& pos) {
    // TODO(M1): read 1–4 bytes at s[pos], validate the sequence, set pos += byte_count.
    // Return std::nullopt and leave pos unchanged on any error.
    (void)s; (void)pos;
    return std::nullopt;
}

bool is_valid_utf8(std::string_view s) {
    // TODO(M1): iterate with decode_utf8; return false on the first error.
    (void)s;
    return false;
}

std::size_t count_code_points(std::string_view s) {
    // TODO(M1): iterate with decode_utf8; count successful decodes.
    (void)s;
    return 0;
}

std::size_t display_width(std::string_view s) {
    // TODO(M1): iterate code points; add 2 for East-Asian wide (W/F category),
    // 1 for normal printable, 0 for non-printable / combining / control chars.
    (void)s;
    return 0;
}

}  // namespace pi::util
