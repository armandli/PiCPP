#include <json/parse.h>

#include <simdjson.h>

namespace pi::json {

// Recursive helper: simdjson DOM element → owned Value.
[[maybe_unused]] static Value convert(simdjson::dom::element elem) {
    // TODO(M1): switch on elem.type() and recursively build a Value.
    // simdjson types: null, bool, int64, uint64, double, string, array, object.
    // Use simdjson::dom::array and simdjson::dom::object iterators for recursion.
    (void)elem;
    return {};
}

std::expected<Value, ParseError> parse(std::string_view input) {
    // TODO(M1):
    //   simdjson::dom::parser parser;
    //   auto result = parser.parse(simdjson::padded_string(input));
    //   if (result.error()) return std::unexpected(ParseError{...});
    //   return convert(result.value());
    (void)input;
    return std::unexpected(ParseError{"not implemented", 0});
}

}  // namespace pi::json
