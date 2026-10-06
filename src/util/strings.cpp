#include <util/strings.h>

namespace pi::util {

std::vector<std::string_view> split(std::string_view sv, char delim) {
    // TODO(M1): scan sv, emit a view for each token between occurrences of delim.
    (void)sv; (void)delim;
    return {};
}

std::vector<std::string_view> split_lines(std::string_view sv) {
    // TODO(M1): scan sv for \n and \r\n boundaries; strip the endings from each view.
    // A trailing newline should NOT produce a trailing empty element.
    (void)sv;
    return {};
}

std::string join(const std::vector<std::string_view>& parts, std::string_view sep) {
    // TODO(M1): concatenate parts with sep between consecutive elements.
    (void)parts; (void)sep;
    return {};
}

std::string join(const std::vector<std::string>& parts, std::string_view sep) {
    // TODO(M1): concatenate parts with sep between consecutive elements.
    (void)parts; (void)sep;
    return {};
}

std::string_view trim(std::string_view sv) {
    // TODO(M1): strip leading and trailing ASCII whitespace (\t \n \r \f \v space).
    (void)sv;
    return {};
}

bool starts_with(std::string_view sv, std::string_view prefix) {
    // TODO(M1): return true iff sv begins with prefix.
    (void)sv; (void)prefix;
    return false;
}

bool ends_with(std::string_view sv, std::string_view suffix) {
    // TODO(M1): return true iff sv ends with suffix.
    (void)sv; (void)suffix;
    return false;
}

std::string replace_all(std::string_view s, std::string_view from, std::string_view to) {
    // TODO(M1): scan s for non-overlapping occurrences of from and replace each with to.
    // Return s unchanged if from is empty or not found.
    (void)from; (void)to;
    return std::string(s);
}

}  // namespace pi::util
