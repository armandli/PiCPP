#ifndef HTTP_FAKE_TRANSPORT_H
#define HTTP_FAKE_TRANSPORT_H

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

#include <http/transport.h>
#include <util/aliases.h>

namespace pi::http {

// What the fake saw for one request.
struct RecordedRequest {
  str         method;  // "POST" or "GET"
  str         url;
  HttpHeaders headers;
  str         body;    // empty for GET
};

// One scripted response.
struct FakeResponse {
  long        status = 200;
  HttpHeaders headers;
  vec<str>    chunks;  // body pieces, delivered one on_chunk call each (2xx only)
  // If set, returned after all chunks are delivered, e.g. to simulate a
  // connection reset mid-stream.
  opt<HttpError> error;

  // The whole body as one chunk.
  static FakeResponse from_body(str body, long status = 200);
  // `body` cut at `seed`-determined random points into pieces of 1..max_chunk
  // bytes.  Same seed → same split.
  static FakeResponse split_randomly(strv body, uint32_t seed, s::size_t max_chunk = 16);
  // `body` one byte per chunk.
  static FakeResponse byte_by_byte(strv body);
};

// In-memory HttpTransport for tests: records each request and answers with the
// next queued FakeResponse (FIFO).
//  - no response queued → HttpError{Connection, 0, "no fake response queued"}
//  - non-2xx → chunks joined into HttpError{Status, status, body}, on_chunk not called
//  - `cancel` is checked before each chunk; once cancelled → HttpError{Cancelled}
//  - get() returns the chunks joined as the body
struct FakeTransport : HttpTransport {
  void enqueue(FakeResponse response);

  const vec<RecordedRequest>& requests() const { return mRequests; }
  s::size_t pending() const { return mResponses.size(); }

  s::expected<HttpStatus, HttpError> post_stream(
      strv url, const HttpHeaders& headers, strv body,
      const ChunkCallback& on_chunk, const util::CancelToken& cancel) override;

  s::expected<HttpResponse, HttpError> get(
      strv url, const HttpHeaders& headers, const util::CancelToken& cancel) override;

private:
  s::deque<FakeResponse> mResponses;
  vec<RecordedRequest>   mRequests;
};

}  // namespace pi::http

#endif  // HTTP_FAKE_TRANSPORT_H
