#include <json/write.h>

#include <cassert>
#include <cmath>
#include <sstream>
#include <charconv>

#include <util/aliases.h>
#include <util/utf8.h>

namespace pi::json {

str to_json(const Value& v) {
  s::stringstream ss;
  switch (v.kind()){
  break; case Value::Kind::Null:
    ss << "null";
  break; case Value::Kind::Bool:
    ss << (*v.as_bool() ? "true" : "false");
  break; case Value::Kind::Int:
    ss << s::to_string(*v.as_int());
  break; case Value::Kind::Double: {
    double d = *v.as_double();
    // JSON has no NaN / Infinity literals; match JSON.stringify and emit null.
    if (not s::isfinite(d)){
      ss << "null";
      break;
    }
    char buf[64];
    auto [end, ec] = s::to_chars(buf, buf + sizeof(buf), d, s::chars_format::general);
    if (ec == s::errc()){
      strv out(buf, end - buf);
      ss << out;
      // Keep integral doubles (0.0 -> "0") parsing back as doubles, not ints.
      if (out.find_first_of(".eE") == strv::npos) ss << ".0";
    }
  }
  break; case Value::Kind::String: {
    ss << '\"';
    size_t pos = 0;
    for (opt<util::DecodedCodePoint> cp = util::decode_utf8(*v.as_string(), pos);
        (cp = util::decode_utf8(*v.as_string(), pos)) != s::nullopt;
        pos = (*cp).next){
      switch ((*cp).code_point){
      break; case '"':  ss << "\\\"";
      break; case '\\': ss << "\\\\";
      break; case '\b': ss << "\\b";
      break; case '\f': ss << "\\f";
      break; case '\n': ss << "\\n";
      break; case '\r': ss << "\\r";
      break; case '\t': ss << "\\t";
      break; case 0x0:  ss << "\\u0000";
      break; case 0x1:  ss << "\\u0001";
      break; case 0x2:  ss << "\\u0002";
      break; case 0x3:  ss << "\\u0003";
      break; case 0x4:  ss << "\\u0004";
      break; case 0x5:  ss << "\\u0005";
      break; case 0x6:  ss << "\\u0006";
      break; case 0x7:  ss << "\\u0007";
      break; case 0xB:  ss << "\\u000B";
      break; case 0xE:  ss << "\\u000E";
      break; case 0xF:  ss << "\\u000F";
      break; case 0x10: ss << "\\u0010";
      break; case 0x11: ss << "\\u0011";
      break; case 0x12: ss << "\\u0012";
      break; case 0x13: ss << "\\u0013";
      break; case 0x14: ss << "\\u0014";
      break; case 0x15: ss << "\\u0015";
      break; case 0x16: ss << "\\u0016";
      break; case 0x17: ss << "\\u0017";
      break; case 0x18: ss << "\\u0018";
      break; case 0x19: ss << "\\u0019";
      break; case 0x1A: ss << "\\u001A";
      break; case 0x1B: ss << "\\u001B";
      break; case 0x1C: ss << "\\u001C";
      break; case 0x1D: ss << "\\u001D";
      break; case 0x1E: ss << "\\u001E";
      break; case 0x1F: ss << "\\u001F";
      break; default: ss << (*v.as_string()).substr(pos, (*cp).next - pos);
      }
    }
    ss << '\"';
  }
  break; case Value::Kind::Array: {
    ss << '[';
    bool is_first = true;
    for (const Value& val : *v.as_array()){
      if (is_first) is_first = false;
      else          ss << ',';
      str elem = to_json(val);
      ss << elem;
    }
    ss << ']';
  }
  break; case Value::Kind::Object: {
    ss << '{';
    bool is_first = true;
    for (const auto& [key, val] : (*v.as_object()).entries){
      if (is_first) is_first = false;
      else          ss << ',';
      str elem = to_json(val);
      ss << '"' << key << "\":" << elem;
    }
    ss << '}';
  }
  break; default: assert(false);
  }
  return ss.str();
}

}  // namespace pi::json
