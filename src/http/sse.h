#ifndef HTTP_SSE_H
#define HTTP_SSE_H

#include <string>
#include <string_view>
#include <vector>

#include <util/aliases.h>

namespace pi::http {

// One dispatched Server-Sent Event.
struct SseEvent {
  str event = "message";  // value of the last `event:` field, "message" if none
  str data;               // `data:` lines joined with '\n' (no trailing '\n')
  str id;                 // last event id seen on the stream so far (persists across events)

  bool operator==(const SseEvent&) const = default;
};

// Incremental SSE decoder, per the WHATWG HTML spec section
// "Interpreting an event stream".
//
// Bytes may be split anywhere between feed() calls — mid-line, between '\r' and
// '\n', mid-UTF-8 sequence — and the output must be identical to feeding
// everything at once.
//
// Rules:
//  - lines end with "\r\n", "\n" or a lone "\r"
//  - a UTF-8 BOM at the very start of the stream is skipped
//  - a line starting with ':' is a comment and is ignored
//  - "field:value" — one leading space of value is removed; a line with no ':'
//    is a field with an empty value
//  - fields: event (sets type), data (appends value + '\n' to the data buffer),
//    id (sets last event id, ignored if it contains U+0000), retry (ignored);
//    unknown fields are ignored
//  - a blank line dispatches: if the data buffer is empty, nothing is emitted;
//    otherwise drop one trailing '\n' and emit.  Then reset event type and data
//    (the id persists)
//  - an event that was never terminated by a blank line is never emitted
//
// "[DONE]" (OpenAI's end-of-stream marker) is ordinary data here; the provider
// interprets it.
struct SseDecoder {
  // Consume the next bytes; return the events completed by them, in order.
  vec<SseEvent> feed(strv bytes);
protected:
  s::optional<SseEvent> process_line();
private:
  str  mLine;              // bytes of current, unfinished line
  bool mLastWasCR = false; // previous chunk ended in '\r'
  bool mStarted = false;   // BOM check done
  str  mEventType;
  str  mData;
  str  mLastId;
};

}  // namespace pi::http

#endif  // HTTP_SSE_H
