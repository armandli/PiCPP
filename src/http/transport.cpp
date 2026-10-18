#include <http/transport.h>

namespace pi::http {

const HttpHeader* find_header(const HttpHeaders& headers, strv name) {
  // TODO(M2): return the first header whose name equals `name` ignoring ASCII case.
  (void)headers; (void)name;
  return nullptr;
}

}  // namespace pi::http
