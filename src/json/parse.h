#ifndef JSON_PARSE_H
#define JSON_PARSE_H

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

#include <json/value.h>

namespace pi::json {

// Error returned by parse() on malformed input.
struct ParseError {
    std::string message;
    std::size_t offset = 0;  // byte offset into the input where the error was detected
};

// Parse JSON text using simdjson DOM and convert the result into an owned Value.
// Returns std::unexpected<ParseError> if the input is not well-formed JSON.
std::expected<Value, ParseError> parse(std::string_view input);

}  // namespace pi::json

#endif  // JSON_PARSE_H
