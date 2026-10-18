#include <json/parse.h>

#include <simdjson.h>

#include <util/aliases.h>

namespace pi::json {

// Recursive helper: simdjson DOM element → owned Value.
[[maybe_unused]] static Value convert(simdjson::dom::element elem) {
  switch (elem.type()){
  break; case simdjson::dom::element_type::ARRAY: {
    vec<Value> ret;
    for (simdjson::dom::element e : elem.get_array().value()){
      ret.push_back(convert(e));
    }
    return Value(ret);
  }
  break; case simdjson::dom::element_type::OBJECT: {
    vec<s::pair<str, Value>> entries;
    for (const auto [k, v] : elem.get_object().value()){
      entries.emplace_back(k, convert(v));
    }
    return Value(Object{entries});
  }
  break; case simdjson::dom::element_type::INT64:
    return Value(elem.get_int64().value());
  break; case simdjson::dom::element_type::UINT64:
    return Value(static_cast<int64_t>(elem.get_uint64().value())); //TODO: correct?
  break; case simdjson::dom::element_type::DOUBLE:
    return Value(elem.get_double().value());
  break; case simdjson::dom::element_type::STRING:
    return Value(elem.get_string().value()); //TODO: or get_c_str() ?
  break; case simdjson::dom::element_type::BOOL:
    return Value(elem.get_bool().value());
  break; case simdjson::dom::element_type::NULL_VALUE:
    return Value(nullptr);
  break; default: assert(false);
  }
}

s::expected<Value, ParseError> parse(strv input) {
  simdjson::dom::parser parser;
  auto result = parser.parse(simdjson::padded_string(input));
  if (result.error()) return s::unexpected(ParseError{simdjson::error_message(result.error()), 0});
  return convert(result.value());
}

}  // namespace pi::json
