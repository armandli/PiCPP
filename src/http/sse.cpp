#include <http/sse.h>

namespace pi::http {

vec<SseEvent> SseDecoder::feed(strv bytes) {
  // TODO(M2): see the rules in sse.h.  Suggested shape:
  //   for each byte:
  //     - skip a UTF-8 BOM (EF BB BF) at the very start of the stream
  //     - '\n' right after a '\r' that ended the previous line (possibly in the
  //       previous chunk): skip it
  //     - '\r' or '\n': process_line(mLine), clear mLine
  //     - otherwise append to mLine
  //   process_line: blank → dispatch; ':' first → comment; else split field:value.
  (void)bytes;
  return {};
}

}  // namespace pi::http
