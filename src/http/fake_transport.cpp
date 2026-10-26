#include <http/fake_transport.h>

#include <random>

#include <util/strings.h>

namespace pi::http {

namespace {
bool is_2xx(long status){
  return status >= 200 and status < 300;
}

HttpError cancelled_error(){
  return HttpError{HttpErrorKind::Cancelled, 0, "request cancelled"};
}

// Play `response` back the way a real transport would: non-2xx collects the
// body into a Status error, 2xx streams each chunk to `on_chunk`.
s::expected<HttpStatus, HttpError> play(
    FakeResponse& response,
    ChunkCallback on_chunk,
    const util::CancelToken& cancel
){
  if (cancel.is_cancelled()) return s::unexpected(cancelled_error());

  if (not is_2xx(response.status))
    return s::unexpected(HttpError{HttpErrorKind::Status, response.status, util::join(response.chunks, "")});

  for (const str& chunk : response.chunks){
    if (cancel.is_cancelled()) return s::unexpected(cancelled_error());
    on_chunk(chunk);
  }
  if (cancel.is_cancelled()) return s::unexpected(cancelled_error());

  if (response.error.has_value()) return s::unexpected(s::move(*response.error));
  return HttpStatus{response.status, s::move(response.headers)};
}
} // namespace

FakeResponse FakeResponse::from_body(str body, long status) {
  FakeResponse ret;
  ret.status = status;
  if (not body.empty()) ret.chunks.push_back(s::move(body));
  return ret;
}

FakeResponse FakeResponse::split_randomly(strv body, uint32_t seed, s::size_t max_chunk) {
  s::mt19937 rng(seed);
  s::uniform_int_distribution<s::size_t> len(1, s::max<s::size_t>(max_chunk, 1));
  FakeResponse ret;
  while (not body.empty()){
    s::size_t n = s::min(len(rng), body.size());
    ret.chunks.emplace_back(body.substr(0, n));
    body.remove_prefix(n);
  }
  return ret;
}

FakeResponse FakeResponse::byte_by_byte(strv body) {
  FakeResponse ret;
  ret.chunks.reserve(body.size());
  for (char byte : body)
    ret.chunks.emplace_back(1, byte);
  return ret;
}

void FakeTransport::enqueue(FakeResponse response) {
  mResponses.push_back(s::move(response));
}

s::expected<HttpStatus, HttpError> FakeTransport::post_stream(
    strv url,
    const HttpHeaders& headers,
    strv body,
    ChunkCallback on_chunk,
    const util::CancelToken& cancel
){
  mRequests.push_back(RecordedRequest{"POST", str(url), headers, str(body)});
  if (mResponses.empty())
    return s::unexpected(HttpError{HttpErrorKind::Connection, 0, "no fake response queued"});

  FakeResponse response = s::move(mResponses.front());
  mResponses.pop_front();
  return play(response, on_chunk, cancel);
}

s::expected<HttpResponse, HttpError> FakeTransport::get(
    strv url,
    const HttpHeaders& headers,
    const util::CancelToken& cancel
){
  mRequests.push_back(RecordedRequest{"GET", str(url), headers, ""});
  if (mResponses.empty())
    return s::unexpected(HttpError{HttpErrorKind::Connection, 0, "no fake response queued"});

  FakeResponse response = s::move(mResponses.front());
  mResponses.pop_front();
  str body;
  s::expected<HttpStatus, HttpError> status = play(response, [&](strv chunk){ body += chunk; }, cancel);
  if (not status.has_value()) return s::unexpected(s::move(status.error()));
  return HttpResponse{s::move(*status), s::move(body)};
}

}  // namespace pi::http
