#include <http/transport.h>

namespace pi::http {

const HttpHeader* find_header(const HttpHeaders& headers, strv name) {
  str lname(name);
  s::transform(s::begin(lname), s::end(lname), s::begin(lname), [](unsigned char c){ return s::tolower(c); });

  for (const HttpHeader& h : headers){
    str hn = h.name;
    s::transform(s::begin(hn), s::end(hn), s::begin(hn), [](unsigned char c){ return s::tolower(c); });
    if (hn == lname) return &h;
  }
  return nullptr;
}

}  // namespace pi::http
