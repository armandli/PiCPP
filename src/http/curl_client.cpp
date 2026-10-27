#include <http/curl_client.h>

#include <util/strings.h>

#include <charconv>

#include <curl/curl.h>

namespace pi::http {

namespace {

CURLcode curl_init(){
  static const CURLcode rcode = curl_global_init(CURL_GLOBAL_DEFAULT);
  return rcode;
}

bool is_2xx(long code){
  return code >= 200 and code < 300;
}

s::unexpected<HttpError> fail(HttpErrorKind kind, str message){
  return s::unexpected(HttpError{kind, 0, s::move(message)});
}

class CurlGuard {
  CURL* mHandle;
  struct curl_slist* mList;
public:
  CurlGuard(): mHandle(nullptr), mList(nullptr) {
    mHandle = curl_easy_init();
  }
  ~CurlGuard(){
    if (mList) curl_slist_free_all(mList);
    if (mHandle) curl_easy_cleanup(mHandle);
  }
  // owns both pointers; a copy would free them twice
  CurlGuard(const CurlGuard&) = delete;
  CurlGuard& operator=(const CurlGuard&) = delete;

  CURL* operator*(){
    return mHandle;
  }
  struct curl_slist* list(){
    return mList;
  }
  void append(const char* item){
    mList = curl_slist_append(mList, item);
  }
};

struct TransferContext {
  CURL*         handle;
  ChunkCallback on_chunk;
  str           error_body;
};

size_t header_callback(char* buffer, size_t sz, size_t nitems, void* userp){
  // buffer is not null terminated and sz == 1
  size_t tsz = sz * nitems;
  HttpStatus* status = static_cast<HttpStatus*>(userp);
  strv line(buffer, tsz);

  try {
    if (line.starts_with("HTTP/")){
      // HTTP/1.1 200 OK; a new status line (1xx, redirect) starts a new block
      long code = 0;
      size_t sp = line.find(' ');
      if (sp != line.npos)
        s::from_chars(line.data() + sp + 1, line.data() + line.size(), code);
      status->code = code;
      status->headers.clear();
    } else if (size_t cp = line.find(':'); cp != line.npos){
      // name: value
      strv name  = line.substr(0, cp);
      strv value = util::trim(line.substr(cp + 1));
      status->headers.emplace_back(str(name), str(value));
    }
  } catch (...){
    return 0;  // aborts the transfer instead of unwinding through curl
  }
  return tsz;
}

size_t write_callback(char* data, size_t sz, size_t nmemb, void* userp){
  // data is not null terminated; sz is always 1
  size_t realsize = sz * nmemb;
  TransferContext* ctx = static_cast<TransferContext*>(userp);
  // the header callback has already run by the time body bytes arrive
  long code = 0;
  curl_easy_getinfo(ctx->handle, CURLINFO_RESPONSE_CODE, &code);
  try {
    if (is_2xx(code)) ctx->on_chunk(strv(data, realsize));
    else              ctx->error_body.append(data, realsize);
  } catch (...){
    // an exception must not unwind through curl's C frames; returning a
    // count other than realsize aborts the transfer with CURLE_WRITE_ERROR
    return 0;
  }
  return realsize;
}

int xferinfo_callback(void* userp, curl_off_t, curl_off_t, curl_off_t, curl_off_t){
  const util::CancelToken* cancel = static_cast<const util::CancelToken*>(userp);
  return cancel->is_cancelled() ? 1 : 0;
}

HttpErrorKind to_error_kind(CURLcode code){
  switch (code){
  case CURLE_URL_MALFORMAT:
  case CURLE_UNSUPPORTED_PROTOCOL:
    return HttpErrorKind::InvalidRequest;
  case CURLE_COULDNT_RESOLVE_PROXY:
  case CURLE_COULDNT_RESOLVE_HOST:
  case CURLE_COULDNT_CONNECT:
  case CURLE_SEND_ERROR:
  case CURLE_RECV_ERROR:
  case CURLE_GOT_NOTHING:
  case CURLE_SSL_CONNECT_ERROR:
  case CURLE_SSL_CERTPROBLEM:
  case CURLE_SSL_CIPHER:
  case CURLE_SSL_CACERT_BADFILE:
  case CURLE_SSL_ISSUER_ERROR:
  case CURLE_SSL_PINNEDPUBKEYNOTMATCH:
  case CURLE_PEER_FAILED_VERIFICATION:
    return HttpErrorKind::Connection;
  case CURLE_OPERATION_TIMEDOUT:
    return HttpErrorKind::Timeout;
  case CURLE_ABORTED_BY_CALLBACK:
    return HttpErrorKind::Cancelled;
  default:
    return HttpErrorKind::Other;
  }
}

// Runs one request: a POST of `body` when given, otherwise a GET. A 2xx body
// streams to on_chunk; a non-2xx body becomes the HttpError's message.
s::expected<HttpStatus, HttpError> perform(
    const CurlOptions& options,
    strv url,
    const HttpHeaders& headers,
    opt<strv> body,
    ChunkCallback on_chunk,
    const util::CancelToken& cancel
){
  if (curl_init() != CURLE_OK) return fail(HttpErrorKind::Other, "global initialization failed");

  CurlGuard handle;
  if (*handle == nullptr) return fail(HttpErrorKind::Other, "unable to init handle");

  handle.append("Expect:");
  for (const HttpHeader& header : headers){
    str line = header.name + ": " + header.value;
    handle.append(line.c_str());
  }

  str surl(url);
  HttpStatus ret;
  TransferContext ctx{*handle, on_chunk, {}};

  curl_easy_setopt(*handle, CURLOPT_URL, surl.c_str());
  if (body){
    curl_easy_setopt(*handle, CURLOPT_POST, 1L);
    curl_easy_setopt(*handle, CURLOPT_POSTFIELDS, body->data());
    curl_easy_setopt(*handle, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body->size()));
  } else {
    curl_easy_setopt(*handle, CURLOPT_HTTPGET, 1L);
  }
  curl_easy_setopt(*handle, CURLOPT_HTTPHEADER, handle.list());
  curl_easy_setopt(*handle, CURLOPT_HEADERFUNCTION, header_callback);
  curl_easy_setopt(*handle, CURLOPT_HEADERDATA, &ret);
  curl_easy_setopt(*handle, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(*handle, CURLOPT_WRITEDATA, &ctx);
  curl_easy_setopt(*handle, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(*handle, CURLOPT_XFERINFOFUNCTION, xferinfo_callback);
  curl_easy_setopt(*handle, CURLOPT_XFERINFODATA, &cancel);
  curl_easy_setopt(*handle, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(options.connect_timeout.count()));
  curl_easy_setopt(*handle, CURLOPT_TIMEOUT_MS, static_cast<long>(options.total_timeout.count()));
  if (options.stall_timeout.count() > 0){
    long secs = (static_cast<long>(options.stall_timeout.count()) + 999L) / 1000L;
    curl_easy_setopt(*handle, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(*handle, CURLOPT_LOW_SPEED_TIME, secs);
  }

  CURLcode rcode = curl_easy_perform(*handle);
  if (rcode != CURLE_OK) return fail(to_error_kind(rcode), curl_easy_strerror(rcode));

  // a 4xx/5xx may still be CURLE_OK
  long code = 0;
  curl_easy_getinfo(*handle, CURLINFO_RESPONSE_CODE, &code);
  if (not is_2xx(code))
    return s::unexpected(HttpError{HttpErrorKind::Status, code, s::move(ctx.error_body)});

  ret.code = code;
  return ret;
}
} // namespace

CurlClient::CurlClient(CurlOptions options)
    : mOptions(options) {}

s::expected<HttpStatus, HttpError> CurlClient::post_stream(
    strv url,
    const HttpHeaders& headers,
    strv body,
    ChunkCallback on_chunk,
    const util::CancelToken& cancel
){
  return perform(mOptions, url, headers, body, on_chunk, cancel);
}

s::expected<HttpResponse, HttpError> CurlClient::get(
    strv url,
    const HttpHeaders& headers,
    const util::CancelToken& cancel
){
  str body;
  auto status = perform(mOptions, url, headers, s::nullopt, [&body](strv chunk){ body += chunk; }, cancel);
  if (not status) return s::unexpected(s::move(status.error()));
  return HttpResponse{s::move(*status), s::move(body)};
}

}  // namespace pi::http
