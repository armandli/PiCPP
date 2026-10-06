#include <json/write.h>

namespace pi::json {

std::string to_json(const Value& v) {
    // TODO(M1): switch on v.kind() and build a JSON string:
    //   Null   → "null"
    //   Bool   → "true" / "false"
    //   Int    → std::to_string(*v.as_int())
    //   Double → std::to_chars with chars_format::general for shortest round-trip
    //   String → '"' + escaped + '"'
    //     escape: \", \\, \b \f \n \r \t, \u00XX for 0x00–0x1F, pass-through UTF-8
    //   Array  → '[' + comma-joined elements + ']'
    //   Object → '{' + comma-joined "key":value pairs + '}'
    (void)v;
    return {};
}

}  // namespace pi::json
