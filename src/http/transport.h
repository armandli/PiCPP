#ifndef HTTP_TRANSPORT_H
#define HTTP_TRANSPORT_H

#include <cstdint>
#include <expected>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <util/aliases.h>
#include <util/cancel.h>

namespace pi::http {

// One HTTP header.  Kept as an ordered list (not a map): order is observable on
// the wire and a name may legally repeat.
struct HttpHeader {
  str name;
  str value;

  bool operator==(const HttpHeader&) const = default;
};

using HttpHeaders = vec<HttpHeader>;

// Case-insensitive lookup of the first header named `name`; nullptr if absent.
const HttpHeader* find_header(const HttpHeaders& headers, strv name);

// Outcome of a request that reached the server and got a 2xx response.
struct HttpStatus {
  long        code = 0;  // e.g. 200
  HttpHeaders headers;   // response headers in arrival order (names as sent)
};

// Response of a non-streaming request (get()).
struct HttpResponse {
  HttpStatus status;
  str        body;
};

enum class HttpErrorKind : uint8_t {
  InvalidRequest,  // malformed URL, unsupported scheme, ... (nothing was sent)
  Connection,      // DNS failure, connection refused / reset, TLS failure
  Timeout,         // connect or transfer timeout
  Cancelled,       // the CancelToken was cancelled before or during the transfer
  Status,          // the server answered with a non-2xx status; see status / message
  Other,
};

// Any failure of a request.  For kind == Status, `status` is the HTTP code and
// `message` is the response body (servers put their error text there).  For
// other kinds `status` is 0 and `message` describes the failure.
struct HttpError {
  HttpErrorKind kind = HttpErrorKind::Other;
  long          status = 0;
  str           message;
};

// Receives each piece of the response body as it arrives.  Pieces are split at
// arbitrary byte positions (mid-line, mid-UTF-8 sequence), so consumers must
// buffer.  Called on the thread that called post_stream().
using ChunkCallback = s::function<void(strv chunk)>;

// Abstract HTTP client so providers can be tested with canned bytes
// (FakeTransport) and run for real against libcurl (CurlClient).
struct HttpTransport {
  virtual ~HttpTransport() = default;

  // POST `body`, streaming the 2xx response body to `on_chunk` piece by piece.
  // Blocks until the response is complete, fails, or `cancel` is cancelled.
  //   - 2xx: every body byte goes to on_chunk; returns the status.
  //   - non-2xx: on_chunk is NOT called; the body is collected into
  //     HttpError{Status, code, body}.
  //   - cancelled: returns HttpError{Cancelled}, even if some chunks were delivered.
  virtual s::expected<HttpStatus, HttpError> post_stream(
      strv url, const HttpHeaders& headers, strv body,
      const ChunkCallback& on_chunk, const util::CancelToken& cancel) = 0;

  // GET `url` and return the whole body.  Same error rules as post_stream().
  virtual s::expected<HttpResponse, HttpError> get(
      strv url, const HttpHeaders& headers, const util::CancelToken& cancel) = 0;
};

}  // namespace pi::http

#endif  // HTTP_TRANSPORT_H
