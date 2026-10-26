#ifndef HTTP_CURL_CLIENT_H
#define HTTP_CURL_CLIENT_H

#include <chrono>

#include <http/transport.h>
#include <util/aliases.h>

namespace pi::http {

struct CurlOptions {
  // Time allowed to establish the connection.  0 = libcurl default.
  sc::milliseconds connect_timeout{10'000};
  // Whole-transfer limit.  0 = none: LLM streams can legitimately run for minutes,
  // so stopping a long stream is the CancelToken's job, not a timeout's.
  sc::milliseconds total_timeout{0};
  // Abort when fewer than 1 byte/s arrives for this long (stalled server).  0 = off.
  sc::milliseconds stall_timeout{0};
};

// libcurl implementation of HttpTransport.
//  - one curl easy handle per request; safe to use from several threads at once
//    as long as each call is on its own thread
//  - curl_global_init runs once per process (function-local static)
//  - CURLOPT_WRITEFUNCTION forwards chunks to on_chunk (2xx) or into the error
//    body (non-2xx); CURLOPT_HEADERFUNCTION collects response headers
//  - CURLOPT_XFERINFOFUNCTION returns non-zero once the CancelToken is cancelled,
//    which makes curl abort with CURLE_ABORTED_BY_CALLBACK → HttpErrorKind::Cancelled
struct CurlClient : HttpTransport {
  explicit CurlClient(CurlOptions options = {});

  s::expected<HttpStatus, HttpError> post_stream(
      strv url, const HttpHeaders& headers, strv body,
      ChunkCallback on_chunk, const util::CancelToken& cancel) override;

  s::expected<HttpResponse, HttpError> get(
      strv url, const HttpHeaders& headers, const util::CancelToken& cancel) override;

private:
  CurlOptions mOptions;
};

}  // namespace pi::http

#endif  // HTTP_CURL_CLIENT_H
