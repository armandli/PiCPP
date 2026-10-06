#ifndef JSON_WRITE_H
#define JSON_WRITE_H

#include <string>

#include <json/value.h>

namespace pi::json {

// Serialize v to compact JSON.
// String values are escaped per RFC 8259: \", \\, \/, \b, \f, \n, \r, \t,
// and \uXXXX for control characters U+0000–U+001F.
// Code points above U+007F are written as UTF-8 (not escaped).
// Doubles use std::to_chars for shortest round-trip representation.
std::string to_json(const Value& v);

}  // namespace pi::json

#endif  // JSON_WRITE_H
