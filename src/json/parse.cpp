#include <json/parse.h>

#include <cstdint>
#include <cstdlib>

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
  // Integers outside int64 become the nearest double (precision may be lost),
  // matching JSON.parse.  Never cast to int64_t: that wraps to a negative value.
  break; case simdjson::dom::element_type::UINT64:
    return Value(static_cast<double>(elem.get_uint64().value()));
  break; case simdjson::dom::element_type::BIGINT: {
    // Wider than 64 bits; simdjson hands us the raw digits (see number_as_string).
    // strtod rounds to nearest and yields ±HUGE_VAL (infinity) past DBL_MAX.
    str digits(elem.get_bigint().value());
    return Value(s::strtod(digits.c_str(), nullptr));
  }
  break; case simdjson::dom::element_type::DOUBLE:
    return Value(elem.get_double().value());
  break; case simdjson::dom::element_type::STRING:
    return Value(elem.get_string().value());
  break; case simdjson::dom::element_type::BOOL:
    return Value(elem.get_bool().value());
  break; case simdjson::dom::element_type::NULL_VALUE:
    return Value(nullptr);
  break; default: assert(false);
  }
}

// ---------------------------------------------------------------------------
// Error-offset scanner
//
// simdjson's DOM parser reports *what* went wrong but not *where*.  After
// simdjson rejects an input, this scanner re-reads it to find the offset of
// the first byte at which the input stops being a prefix of any valid JSON
// document (input.size() when the input is a valid but truncated prefix).
// It only runs on the error path, so it favours clarity over speed.
//
// Every scan_* helper starts at pos, advances pos past what it accepted, and
// returns the error offset, or nullopt when the token is complete and valid.
// ---------------------------------------------------------------------------

static bool is_ws(char c) {
  return c == ' ' or c == '\t' or c == '\n' or c == '\r';
}

static bool is_digit(char c) {
  return c >= '0' and c <= '9';
}

static bool is_hex(char c) {
  return is_digit(c) or (c >= 'a' and c <= 'f') or (c >= 'A' and c <= 'F');
}

// Four hex digits of a \u escape; pos is at the first digit.
static opt<s::size_t> scan_hex4(strv in, s::size_t& pos, uint32_t& code) {
  code = 0;
  for (int i = 0; i < 4; ++i){
    if (pos == in.size()) return in.size();
    char c = in[pos];
    if (not is_hex(c)) return pos;
    code = code * 16 + (is_digit(c) ? c - '0' : (c | 0x20) - 'a' + 10);
    ++pos;
  }
  return s::nullopt;
}

// One escape sequence; pos is at the byte after the backslash.
static opt<s::size_t> scan_escape(strv in, s::size_t& pos) {
  if (pos == in.size()) return in.size();
  switch (in[pos]){
  break; case '"': case '\\': case '/': case 'b': case 'f': case 'n': case 'r': case 't':
    ++pos;
    return s::nullopt;
  break; case 'u': {
    ++pos;
    s::size_t digits = pos;
    uint32_t code = 0;
    if (auto e = scan_hex4(in, pos, code)) return e;
    // Lone low surrogate: impossible from the second digit ('D' then 'C'..'F').
    if (code >= 0xDC00 and code <= 0xDFFF) return digits + 1;
    if (code < 0xD800 or code > 0xDBFF) return s::nullopt;
    // High surrogate: must be followed by \u + low surrogate (DC00–DFFF).
    for (char want : {'\\', 'u'}){
      if (pos == in.size()) return in.size();
      if (in[pos] != want) return pos;
      ++pos;
    }
    digits = pos;
    if (auto e = scan_hex4(in, pos, code)){
      // Report the first digit that rules out DC00–DFFF, if it comes earlier.
      if (*e > digits     and (in[digits] | 0x20) != 'd') return digits;
      if (*e > digits + 1 and not ((in[digits + 1] | 0x20) >= 'c' and (in[digits + 1] | 0x20) <= 'f')) return digits + 1;
      return e;
    }
    if ((in[digits] | 0x20) != 'd') return digits;
    if (code < 0xDC00 or code > 0xDFFF) return digits + 1;
    return s::nullopt;
  }
  break; default:
    return pos;
  }
}

// One UTF-8 encoded code point of 2–4 bytes; pos is at the lead byte.
// Rejects overlong encodings, surrogates and code points above U+10FFFF.
static opt<s::size_t> scan_utf8_multibyte(strv in, s::size_t& pos) {
  unsigned char b = static_cast<unsigned char>(in[pos]);
  int len = 0;
  unsigned char lo = 0x80, hi = 0xBF;  // allowed range of the second byte
  if      (b >= 0xC2 and b <= 0xDF)                              len = 2;
  else if (b == 0xE0)                                          { len = 3; lo = 0xA0; }
  else if ((b >= 0xE1 and b <= 0xEC) or b == 0xEE or b == 0xEF)  len = 3;
  else if (b == 0xED)                                          { len = 3; hi = 0x9F; }
  else if (b == 0xF0)                                          { len = 4; lo = 0x90; }
  else if (b >= 0xF1 and b <= 0xF3)                              len = 4;
  else if (b == 0xF4)                                          { len = 4; hi = 0x8F; }
  else return pos;
  ++pos;
  for (int i = 1; i < len; ++i){
    if (pos == in.size()) return in.size();
    unsigned char c = static_cast<unsigned char>(in[pos]);
    if (c < lo or c > hi) return pos;
    lo = 0x80;
    hi = 0xBF;
    ++pos;
  }
  return s::nullopt;
}

// A string; pos is at the opening quote.
static opt<s::size_t> scan_string(strv in, s::size_t& pos) {
  ++pos;
  while (true){
    if (pos == in.size()) return in.size();
    unsigned char c = static_cast<unsigned char>(in[pos]);
    if (c == '"'){
      ++pos;
      return s::nullopt;
    }
    if (c < 0x20) return pos;  // unescaped control character
    if (c == '\\'){
      ++pos;
      if (auto e = scan_escape(in, pos)) return e;
    } else if (c >= 0x80){
      if (auto e = scan_utf8_multibyte(in, pos)) return e;
    } else {
      ++pos;
    }
  }
}

// -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?; pos is at '-' or a digit.
// A number ends at the first byte that can't extend it; the caller checks that byte.
static opt<s::size_t> scan_number(strv in, s::size_t& pos) {
  auto digits = [&]() -> opt<s::size_t> {  // one or more digits
    if (pos == in.size()) return in.size();
    if (not is_digit(in[pos])) return pos;
    while (pos < in.size() and is_digit(in[pos])) ++pos;
    return s::nullopt;
  };
  if (in[pos] == '-') ++pos;
  if (pos == in.size()) return in.size();
  if (in[pos] == '0') ++pos;
  else if (auto e = digits()) return e;
  if (pos < in.size() and in[pos] == '.'){
    ++pos;
    if (auto e = digits()) return e;
  }
  if (pos < in.size() and (in[pos] == 'e' or in[pos] == 'E')){
    ++pos;
    if (pos < in.size() and (in[pos] == '+' or in[pos] == '-')) ++pos;
    if (auto e = digits()) return e;
  }
  return s::nullopt;
}

// true / false / null; pos is at its first letter.
static opt<s::size_t> scan_literal(strv in, s::size_t& pos) {
  strv word = in[pos] == 't' ? "true" : in[pos] == 'f' ? "false" : "null";
  for (char want : word){
    if (pos == in.size()) return in.size();
    if (in[pos] != want) return pos;
    ++pos;
  }
  return s::nullopt;
}

// Offset of the first byte at which `in` stops being a prefix of a valid JSON
// document; in.size() for a valid but incomplete prefix; nullopt if `in` is
// a complete, syntactically valid document.  Iterative with an explicit stack
// so deeply nested input can't overflow the call stack.
static opt<s::size_t> find_syntax_error(strv in) {
  enum class Expect : uint8_t {
    Value,              // any value
    ValueOrArrayClose,  // right after '['
    KeyOrObjectClose,   // right after '{'
    Key,                // after ',' in an object
    Colon,              // after a key
    CommaOrClose,       // after a value inside a container
    End,                // after the top-level value: only whitespace allowed
  };
  vec<char> open;  // '[' / '{' for each enclosing container
  s::size_t pos = 0;
  Expect expect = Expect::Value;
  auto after_value = [&]{ return open.empty() ? Expect::End : Expect::CommaOrClose; };

  while (true){
    while (pos < in.size() and is_ws(in[pos])) ++pos;
    if (pos == in.size()) return expect == Expect::End ? opt<s::size_t>() : in.size();
    char c = in[pos];

    switch (expect){
    break; case Expect::End:
      return pos;
    break; case Expect::Colon:
      if (c != ':') return pos;
      ++pos;
      expect = Expect::Value;
    break; case Expect::CommaOrClose: {
      char close = open.back() == '[' ? ']' : '}';
      if (c == ','){
        ++pos;
        expect = open.back() == '[' ? Expect::Value : Expect::Key;
      } else if (c == close){
        ++pos;
        open.pop_back();
        expect = after_value();
      } else {
        return pos;
      }
    }
    break; case Expect::KeyOrObjectClose: case Expect::Key:
      if (expect == Expect::KeyOrObjectClose and c == '}'){
        ++pos;
        open.pop_back();
        expect = after_value();
        break;
      }
      if (c != '"') return pos;
      if (auto e = scan_string(in, pos)) return e;
      expect = Expect::Colon;
    break; case Expect::ValueOrArrayClose: case Expect::Value:
      if (expect == Expect::ValueOrArrayClose and c == ']'){
        ++pos;
        open.pop_back();
        expect = after_value();
        break;
      }
      if (c == '[' or c == '{'){
        open.push_back(c);
        ++pos;
        expect = c == '[' ? Expect::ValueOrArrayClose : Expect::KeyOrObjectClose;
        break;
      }
      opt<s::size_t> e;
      if      (c == '"')                           e = scan_string(in, pos);
      else if (c == '-' or is_digit(c))            e = scan_number(in, pos);
      else if (c == 't' or c == 'f' or c == 'n')   e = scan_literal(in, pos);
      else                                         return pos;
      if (e) return e;
      expect = after_value();
    }
  }
}

s::expected<Value, ParseError> parse(strv input) {
  simdjson::dom::parser parser;
  parser.number_as_string(true);  // integers wider than 64 bits: BIGINT, not BIGINT_ERROR
  auto result = parser.parse(simdjson::padded_string(input));
  if (result.error()){
    // nullopt: the syntax is valid and simdjson failed for another reason
    // (e.g. DEPTH_ERROR), so there is no syntax position to report.
    s::size_t offset = find_syntax_error(input).value_or(0);
    return s::unexpected(ParseError{simdjson::error_message(result.error()), offset});
  }
  return convert(result.value());
}

}  // namespace pi::json
