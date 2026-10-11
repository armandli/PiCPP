#include <json/parse.h>

#include <simdjson.h>

#include <util/aliases.h>

namespace pi::json {

// Recursive helper: simdjson DOM element → owned Value.
[[maybe_unused]] static Value convert(simdjson::dom::element elem) {
    // TODO(M1): switch on elem.type() and recursively build a Value.
    // simdjson types: null, bool, int64, uint64, double, string, array, object.
    // Use simdjson::dom::array and simdjson::dom::object iterators for recursion.
    (void)elem;
    return {};
}

s::expected<Value, ParseError> parse(strv input) {
    // TODO(M1):
    //   simdjson::dom::parser parser;
    //   auto result = parser.parse(simdjson::padded_string(input));
    //   if (result.error()) return s::unexpected(ParseError{...});
    //   return convert(result.value());
    (void)input;
    return s::unexpected(ParseError{"not implemented", 0});
}

}  // namespace pi::json
