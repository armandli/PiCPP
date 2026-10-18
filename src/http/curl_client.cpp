#include <http/curl_client.h>

#include <curl/curl.h>

namespace pi::http {

CurlClient::CurlClient(CurlOptions options)
    : mOptions(options) {}

s::expected<HttpStatus, HttpError> CurlClient::post_stream(
    strv url, const HttpHeaders& headers, strv body,
    const ChunkCallback& on_chunk, const util::CancelToken& cancel) {
  // TODO(M2):
  //   - static const bool init = (curl_global_init(CURL_GLOBAL_DEFAULT), true);
  //   - curl_easy_init; CURLOPT_URL (str(url), it must be NUL-terminated),
  //     CURLOPT_POST, CURLOPT_POSTFIELDS + CURLOPT_POSTFIELDSIZE_LARGE,
  //     CURLOPT_HTTPHEADER (curl_slist; add "Expect:" to disable 100-continue)
  //   - CURLOPT_HEADERFUNCTION: parse the status line and "Name: value" lines into
  //     HttpStatus::headers (reset them when a new status line starts, e.g. after
  //     a redirect or 100 Continue)
  //   - CURLOPT_WRITEFUNCTION: if the status (CURLINFO_RESPONSE_CODE) is 2xx →
  //     on_chunk, else append to an error-body string
  //   - CURLOPT_NOPROGRESS = 0 + CURLOPT_XFERINFOFUNCTION: return 1 when
  //     cancel.is_cancelled()
  //   - timeouts from mOptions (CURLOPT_CONNECTTIMEOUT_MS, CURLOPT_TIMEOUT_MS,
  //     CURLOPT_LOW_SPEED_LIMIT / CURLOPT_LOW_SPEED_TIME)
  //   - map CURLcode → HttpErrorKind (CURLE_URL_MALFORMAT / UNSUPPORTED_PROTOCOL →
  //     InvalidRequest, COULDNT_CONNECT / COULDNT_RESOLVE_HOST / RECV_ERROR /
  //     SSL_* → Connection, OPERATION_TIMEDOUT → Timeout,
  //     ABORTED_BY_CALLBACK → Cancelled); non-2xx → Status with the body
  //   - always curl_slist_free_all + curl_easy_cleanup (an RAII wrapper helps)
  (void)url; (void)headers; (void)body; (void)on_chunk; (void)cancel;
  return s::unexpected(HttpError{HttpErrorKind::Other, 0, "not implemented"});
}

s::expected<HttpResponse, HttpError> CurlClient::get(
    strv url, const HttpHeaders& headers, const util::CancelToken& cancel) {
  // TODO(M2): same as post_stream with CURLOPT_HTTPGET, collecting the body.
  //   Consider sharing one private helper between the two.
  (void)url; (void)headers; (void)cancel;
  return s::unexpected(HttpError{HttpErrorKind::Other, 0, "not implemented"});
}

}  // namespace pi::http
