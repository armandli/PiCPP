#include <http/fake_transport.h>

namespace pi::http {

FakeResponse FakeResponse::from_body(str body, long status) {
  // TODO(M2): one chunk holding `body` (no chunks if body is empty).
  (void)body; (void)status;
  return {};
}

FakeResponse FakeResponse::split_randomly(strv body, uint32_t seed, s::size_t max_chunk) {
  // TODO(M2): s::mt19937 rng(seed); repeatedly take uniform [1, max_chunk] bytes.
  //   Concatenating the chunks must give back `body`.
  (void)body; (void)seed; (void)max_chunk;
  return {};
}

FakeResponse FakeResponse::byte_by_byte(strv body) {
  // TODO(M2): one chunk per byte.
  (void)body;
  return {};
}

void FakeTransport::enqueue(FakeResponse response) {
  // TODO(M2): push to the back of mResponses.
  (void)response;
}

s::expected<HttpStatus, HttpError> FakeTransport::post_stream(
    strv url, const HttpHeaders& headers, strv body,
    const ChunkCallback& on_chunk, const util::CancelToken& cancel) {
  // TODO(M2): record {"POST", url, headers, body}; pop the next response (none →
  //   Connection error); non-2xx → Status error with the joined chunks; else
  //   deliver chunks (check `cancel` before each), then return `error` if set,
  //   else HttpStatus{status, headers}.
  (void)url; (void)headers; (void)body; (void)on_chunk; (void)cancel;
  return s::unexpected(HttpError{HttpErrorKind::Other, 0, "not implemented"});
}

s::expected<HttpResponse, HttpError> FakeTransport::get(
    strv url, const HttpHeaders& headers, const util::CancelToken& cancel) {
  // TODO(M2): record {"GET", url, headers, ""}; same rules, body = joined chunks.
  (void)url; (void)headers; (void)cancel;
  return s::unexpected(HttpError{HttpErrorKind::Other, 0, "not implemented"});
}

}  // namespace pi::http
