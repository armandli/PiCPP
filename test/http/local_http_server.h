#ifndef TEST_HTTP_LOCAL_HTTP_SERVER_H
#define TEST_HTTP_LOCAL_HTTP_SERVER_H

// Minimal HTTP/1.1 server on 127.0.0.1 for testing CurlClient without network.
// Test infrastructure, not production code: one connection at a time, serves
// scripted responses in order, closes the connection after each response.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <util/aliases.h>

namespace pi::test {

// An informational (1xx) response sent ahead of the final one, e.g. 103 Early Hints.
struct InterimResponse {
  long status = 103;
  vec<s::pair<str, str>> headers;
};

struct ServerResponse {
  long status = 200;
  vec<s::pair<str, str>> headers;
  vec<InterimResponse> interim;  // sent in order before the final status line
  vec<str> chunks;              // written one by one, with `delay` before each
  sc::milliseconds delay{0};
  // After the last chunk, keep the connection open this long before closing
  // (used to test cancellation of a stream that never ends on its own).
  sc::milliseconds hold_open{0};
};

struct ReceivedRequest {
  str method;
  str target;                       // e.g. "/v1/chat/completions"
  vec<s::pair<str, str>> headers;   // names as sent
  str body;

  // Case-insensitive header lookup; empty if absent.
  str header(strv name) const {
    for (const auto& [k, v] : headers){
      if (k.size() == name.size() and
          s::equal(k.begin(), k.end(), name.begin(), [](char a, char b){ return s::tolower(a) == s::tolower(b); }))
        return v;
    }
    return {};
  }
};

struct LocalHttpServer {
  LocalHttpServer() {
    mListen = ::socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    ::setsockopt(mListen, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;  // ephemeral
    if (::bind(mListen, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0 or ::listen(mListen, 8) != 0)
      s::abort();
    socklen_t len = sizeof(addr);
    ::getsockname(mListen, reinterpret_cast<sockaddr*>(&addr), &len);
    mPort = ntohs(addr.sin_port);
    mThread = s::jthread([this](s::stop_token st){ run(st); });
  }

  ~LocalHttpServer() {
    mThread.request_stop();
    if (mThread.joinable()) mThread.join();
    ::close(mListen);
  }

  LocalHttpServer(const LocalHttpServer&) = delete;
  LocalHttpServer& operator=(const LocalHttpServer&) = delete;

  int port() const { return mPort; }
  str url(strv path = "/") const { return "http://127.0.0.1:" + s::to_string(mPort) + str(path); }

  void enqueue(ServerResponse r) {
    s::lock_guard lock(mMutex);
    mResponses.push_back(s::move(r));
  }

  vec<ReceivedRequest> requests() const {
    s::lock_guard lock(mMutex);
    return mRequests;
  }

private:
  // Wait up to 50 ms for fd to become readable; false on timeout.
  static bool readable(int fd) {
    pollfd p{fd, POLLIN, 0};
    return ::poll(&p, 1, 50) > 0;
  }

  // Sleep in small steps so the destructor never waits long.
  static bool sleep_for(sc::milliseconds d, const s::stop_token& st) {
    auto until = sc::steady_clock::now() + d;
    while (sc::steady_clock::now() < until){
      if (st.stop_requested()) return false;
      st::sleep_for(sc::milliseconds(5));
    }
    return not st.stop_requested();
  }

  static bool send_all(int fd, strv data) {
    while (not data.empty()){
      ssize_t n = ::send(fd, data.data(), data.size(), 0);
      if (n <= 0) return false;
      data.remove_prefix(static_cast<s::size_t>(n));
    }
    return true;
  }

  void run(const s::stop_token& st) {
    while (not st.stop_requested()){
      if (not readable(mListen)) continue;
      int fd = ::accept(mListen, nullptr, nullptr);
      if (fd < 0) continue;
#ifdef SO_NOSIGPIPE
      int one = 1;
      ::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
      serve(fd, st);
      ::close(fd);
    }
  }

  void serve(int fd, const s::stop_token& st) {
    // Read the request head, then Content-Length bytes of body.
    str buf;
    s::size_t head_end = str::npos;
    while (head_end == str::npos){
      if (st.stop_requested()) return;
      if (not readable(fd)) continue;
      char tmp[4096];
      ssize_t n = ::recv(fd, tmp, sizeof(tmp), 0);
      if (n <= 0) return;
      buf.append(tmp, static_cast<s::size_t>(n));
      head_end = buf.find("\r\n\r\n");
    }
    ReceivedRequest req;
    strv head(buf.data(), head_end);
    s::size_t line_end = head.find("\r\n");
    strv request_line = head.substr(0, line_end);
    s::size_t sp1 = request_line.find(' ');
    s::size_t sp2 = request_line.find(' ', sp1 + 1);
    req.method = str(request_line.substr(0, sp1));
    req.target = str(request_line.substr(sp1 + 1, sp2 - sp1 - 1));
    while (line_end != strv::npos and line_end < head.size()){
      s::size_t next = head.find("\r\n", line_end + 2);
      strv line = head.substr(line_end + 2, next == strv::npos ? strv::npos : next - line_end - 2);
      s::size_t colon = line.find(':');
      if (colon != strv::npos){
        strv value = line.substr(colon + 1);
        while (not value.empty() and value.front() == ' ') value.remove_prefix(1);
        req.headers.emplace_back(str(line.substr(0, colon)), str(value));
      }
      line_end = next;
    }
    s::size_t content_length = 0;
    if (str cl = req.header("Content-Length"); not cl.empty()) content_length = s::stoul(cl);
    req.body = buf.substr(head_end + 4);
    while (req.body.size() < content_length){
      if (st.stop_requested()) return;
      if (not readable(fd)) continue;
      char tmp[4096];
      ssize_t n = ::recv(fd, tmp, sizeof(tmp), 0);
      if (n <= 0) break;
      req.body.append(tmp, static_cast<s::size_t>(n));
    }

    ServerResponse resp;
    {
      s::lock_guard lock(mMutex);
      mRequests.push_back(req);
      if (mResponses.empty()){
        resp.status = 500;
        resp.chunks = {"LocalHttpServer: no response queued"};
      } else {
        resp = s::move(mResponses.front());
        mResponses.pop_front();
      }
    }

    str out;
    for (const InterimResponse& ir : resp.interim){
      out += "HTTP/1.1 " + s::to_string(ir.status) + " Interim\r\n";
      for (const auto& [k, v] : ir.headers) out += k + ": " + v + "\r\n";
      out += "\r\n";
    }
    // No Content-Length: the body ends when the connection closes.
    out += "HTTP/1.1 " + s::to_string(resp.status) + " Status\r\nConnection: close\r\n";
    for (const auto& [k, v] : resp.headers) out += k + ": " + v + "\r\n";
    out += "\r\n";
    if (not send_all(fd, out)) return;
    for (const str& chunk : resp.chunks){
      if (not sleep_for(resp.delay, st)) return;
      if (not send_all(fd, chunk)) return;
    }
    sleep_for(resp.hold_open, st);
  }

  int mListen = -1;
  int mPort = 0;
  mutable s::mutex mMutex;
  s::deque<ServerResponse> mResponses;
  vec<ReceivedRequest> mRequests;
  s::jthread mThread;
};

}  // namespace pi::test

#endif  // TEST_HTTP_LOCAL_HTTP_SERVER_H
