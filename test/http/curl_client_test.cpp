// CurlClient against an in-process server on 127.0.0.1 (see local_http_server.h).
// Loopback only: no network, no Ollama needed.

#include <http/curl_client.h>

#include <chrono>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include <http/local_http_server.h>
#include <util/aliases.h>
#include <util/cancel.h>

using namespace pi::http;
using pi::test::LocalHttpServer;
using pi::test::ServerResponse;
using pi::util::CancelToken;

static ServerResponse ok(vec<str> chunks, long status = 200) {
    ServerResponse r;
    r.status = status;
    r.chunks = s::move(chunks);
    return r;
}

// A port with nothing listening: bind an ephemeral port, then close it.
static int closed_port() {
    int port = 0;
    {
        LocalHttpServer tmp;
        port = tmp.port();
    }
    return port;
}

// ===========================================================================
// post_stream — request
// ===========================================================================

TEST(CurlClient, PostSendsMethodTargetBodyAndHeaders) {
    LocalHttpServer server;
    server.enqueue(ok({"ok"}));
    CurlClient client;
    CancelToken cancel;
    str body = R"({"model":"m","stream":true})";
    auto r = client.post_stream(server.url("/v1/chat/completions"),
                                {{"Content-Type", "application/json"}, {"Authorization", "Bearer secret"}},
                                body, [](strv) {}, cancel);
    ASSERT_TRUE(r.has_value()) << r.error().message;
    auto reqs = server.requests();
    ASSERT_EQ(reqs.size(), 1u);
    EXPECT_EQ(reqs[0].method, "POST");
    EXPECT_EQ(reqs[0].target, "/v1/chat/completions");
    EXPECT_EQ(reqs[0].body, body);
    EXPECT_EQ(reqs[0].header("Content-Type"), "application/json");
    EXPECT_EQ(reqs[0].header("Authorization"), "Bearer secret");
}

TEST(CurlClient, PostLargeBodyArrivesIntact) {
    // Big enough that curl would send "Expect: 100-continue" unless disabled.
    LocalHttpServer server;
    server.enqueue(ok({"ok"}));
    CurlClient client;
    CancelToken cancel;
    str body(2 * 1024 * 1024, 'x');
    auto r = client.post_stream(server.url(), {}, body, [](strv) {}, cancel);
    ASSERT_TRUE(r.has_value()) << r.error().message;
    ASSERT_EQ(server.requests().size(), 1u);
    EXPECT_EQ(server.requests()[0].body.size(), body.size());
}

// ===========================================================================
// post_stream — response
// ===========================================================================

TEST(CurlClient, PostStreamsWholeBodyToCallback) {
    LocalHttpServer server;
    server.enqueue(ok({"data: a\n\n", "data: b\n\n", "data: [DONE]\n\n"}));
    CurlClient client;
    CancelToken cancel;
    str got;
    auto r = client.post_stream(server.url(), {}, "{}", [&](strv c) { got += c; }, cancel);
    ASSERT_TRUE(r.has_value()) << r.error().message;
    EXPECT_EQ(r->code, 200);
    EXPECT_EQ(got, "data: a\n\ndata: b\n\ndata: [DONE]\n\n");
}

TEST(CurlClient, PostDeliversChunksBeforeResponseEnds) {
    // The first chunk must reach the callback while the server is still sending:
    // this is what makes token-by-token streaming possible.
    LocalHttpServer server;
    ServerResponse resp = ok({"first", "second"});
    resp.delay = sc::milliseconds(300);
    server.enqueue(resp);
    CurlClient client;
    CancelToken cancel;
    auto start = sc::steady_clock::now();
    sc::milliseconds first_at{-1};
    auto r = client.post_stream(server.url(), {}, "{}", [&](strv) {
        if (first_at.count() < 0)
            first_at = sc::duration_cast<sc::milliseconds>(sc::steady_clock::now() - start);
    }, cancel);
    auto total = sc::duration_cast<sc::milliseconds>(sc::steady_clock::now() - start);
    ASSERT_TRUE(r.has_value()) << r.error().message;
    ASSERT_GE(first_at.count(), 0);
    EXPECT_LT(first_at.count() + 150, total.count())
        << "first chunk at " << first_at.count() << "ms, done at " << total.count() << "ms";
}

TEST(CurlClient, PostReturnsResponseHeaders) {
    LocalHttpServer server;
    ServerResponse resp = ok({"x"});
    resp.headers = {{"Content-Type", "text/event-stream"}, {"X-Request-Id", "abc"}};
    server.enqueue(resp);
    CurlClient client;
    CancelToken cancel;
    auto r = client.post_stream(server.url(), {}, "{}", [](strv) {}, cancel);
    ASSERT_TRUE(r.has_value()) << r.error().message;
    const HttpHeader* ct = find_header(r->headers, "content-type");
    ASSERT_NE(ct, nullptr);
    EXPECT_EQ(ct->value, "text/event-stream");
    const HttpHeader* id = find_header(r->headers, "X-Request-Id");
    ASSERT_NE(id, nullptr);
    EXPECT_EQ(id->value, "abc");
}

TEST(CurlClient, PostDropsHeadersOfInterimResponses) {
    // curl hands every response's headers to the header callback, 1xx included;
    // only the final response's headers may be returned.
    LocalHttpServer server;
    ServerResponse resp = ok({"x"});
    resp.interim = {{103, {{"Link", "</style.css>; rel=preload"}}}};
    resp.headers = {{"Content-Type", "text/event-stream"}};
    server.enqueue(resp);
    CurlClient client;
    CancelToken cancel;
    auto r = client.post_stream(server.url(), {}, "{}", [](strv) {}, cancel);
    ASSERT_TRUE(r.has_value()) << r.error().message;
    EXPECT_EQ(r->code, 200);
    EXPECT_EQ(find_header(r->headers, "Link"), nullptr);
    const HttpHeader* ct = find_header(r->headers, "Content-Type");
    ASSERT_NE(ct, nullptr);
    EXPECT_EQ(ct->value, "text/event-stream");
}

TEST(CurlClient, PostNon2xxIsStatusErrorWithBody) {
    LocalHttpServer server;
    server.enqueue(ok({R"({"error":"model 'nope' not found"})"}, 404));
    CurlClient client;
    CancelToken cancel;
    bool called = false;
    auto r = client.post_stream(server.url(), {}, "{}", [&](strv) { called = true; }, cancel);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().kind, HttpErrorKind::Status);
    EXPECT_EQ(r.error().status, 404);
    EXPECT_EQ(r.error().message, R"({"error":"model 'nope' not found"})");
    EXPECT_FALSE(called);
}

TEST(CurlClient, Post5xxIsStatusError) {
    LocalHttpServer server;
    server.enqueue(ok({"overloaded"}, 503));
    CurlClient client;
    CancelToken cancel;
    auto r = client.post_stream(server.url(), {}, "{}", [](strv) {}, cancel);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().kind, HttpErrorKind::Status);
    EXPECT_EQ(r.error().status, 503);
}

// ===========================================================================
// Errors and cancellation
// ===========================================================================

TEST(CurlClient, ConnectionRefusedIsConnectionError) {
    CurlClient client;
    CancelToken cancel;
    str url = "http://127.0.0.1:" + s::to_string(closed_port()) + "/";
    auto r = client.post_stream(url, {}, "{}", [](strv) {}, cancel);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().kind, HttpErrorKind::Connection) << r.error().message;
    EXPECT_FALSE(r.error().message.empty());
}

TEST(CurlClient, MalformedUrlIsInvalidRequest) {
    CurlClient client;
    CancelToken cancel;
    auto r = client.post_stream("http://[not-a-host", {}, "{}", [](strv) {}, cancel);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().kind, HttpErrorKind::InvalidRequest) << r.error().message;
}

TEST(CurlClient, UnsupportedSchemeIsInvalidRequest) {
    CurlClient client;
    CancelToken cancel;
    auto r = client.post_stream("nosuchscheme://x/", {}, "{}", [](strv) {}, cancel);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().kind, HttpErrorKind::InvalidRequest) << r.error().message;
}

TEST(CurlClient, CancelledBeforeStartSendsNothing) {
    LocalHttpServer server;
    server.enqueue(ok({"x"}));
    CurlClient client;
    CancelToken cancel;
    cancel.cancel();
    bool called = false;
    auto r = client.post_stream(server.url(), {}, "{}", [&](strv) { called = true; }, cancel);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().kind, HttpErrorKind::Cancelled);
    EXPECT_FALSE(called);
}

TEST(CurlClient, CancelFromCallbackStopsStream) {
    // The server sends one chunk and then holds the connection open for 10 s.
    // Cancelling from the callback must end the call quickly.
    LocalHttpServer server;
    ServerResponse resp = ok({"data: a\n\n"});
    resp.hold_open = sc::seconds(10);
    server.enqueue(resp);
    CurlClient client;
    CancelToken cancel;
    auto start = sc::steady_clock::now();
    auto r = client.post_stream(server.url(), {}, "{}", [&](strv) { cancel.cancel(); }, cancel);
    auto elapsed = sc::steady_clock::now() - start;
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().kind, HttpErrorKind::Cancelled);
    EXPECT_LT(elapsed, sc::seconds(3));
}

TEST(CurlClient, CancelFromAnotherThreadStopsStream) {
    LocalHttpServer server;
    ServerResponse resp = ok({"data: a\n\n"});
    resp.hold_open = sc::seconds(10);
    server.enqueue(resp);
    CurlClient client;
    CancelToken cancel;
    s::jthread canceller([&] {
        st::sleep_for(sc::milliseconds(200));
        cancel.cancel();
    });
    auto start = sc::steady_clock::now();
    auto r = client.post_stream(server.url(), {}, "{}", [](strv) {}, cancel);
    auto elapsed = sc::steady_clock::now() - start;
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().kind, HttpErrorKind::Cancelled);
    EXPECT_LT(elapsed, sc::seconds(3));
}

TEST(CurlClient, TotalTimeoutIsTimeoutError) {
    LocalHttpServer server;
    ServerResponse resp = ok({"x"});
    resp.hold_open = sc::seconds(10);
    server.enqueue(resp);
    CurlClient client(CurlOptions{.total_timeout = sc::milliseconds(300)});
    CancelToken cancel;
    auto r = client.post_stream(server.url(), {}, "{}", [](strv) {}, cancel);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().kind, HttpErrorKind::Timeout) << r.error().message;
}

// ===========================================================================
// get
// ===========================================================================

TEST(CurlClient, GetReturnsBodyAndStatus) {
    LocalHttpServer server;
    server.enqueue(ok({R"({"object":"list",)", R"("data":[{"id":"qwen"}]})"}));
    CurlClient client;
    CancelToken cancel;
    auto r = client.get(server.url("/v1/models"), {{"Authorization", "Bearer k"}}, cancel);
    ASSERT_TRUE(r.has_value()) << r.error().message;
    EXPECT_EQ(r->status.code, 200);
    EXPECT_EQ(r->body, R"({"object":"list","data":[{"id":"qwen"}]})");
    auto reqs = server.requests();
    ASSERT_EQ(reqs.size(), 1u);
    EXPECT_EQ(reqs[0].method, "GET");
    EXPECT_EQ(reqs[0].target, "/v1/models");
    EXPECT_EQ(reqs[0].header("Authorization"), "Bearer k");
}

TEST(CurlClient, GetDropsHeadersOfInterimResponses) {
    LocalHttpServer server;
    ServerResponse resp = ok({"body"});
    resp.interim = {{103, {{"Link", "</a.css>; rel=preload"}}},
                    {103, {{"Link", "</b.css>; rel=preload"}}}};
    resp.headers = {{"X-Request-Id", "abc"}};
    server.enqueue(resp);
    CurlClient client;
    CancelToken cancel;
    auto r = client.get(server.url(), {}, cancel);
    ASSERT_TRUE(r.has_value()) << r.error().message;
    EXPECT_EQ(r->status.code, 200);
    EXPECT_EQ(r->body, "body");
    EXPECT_EQ(find_header(r->status.headers, "Link"), nullptr);
    const HttpHeader* id = find_header(r->status.headers, "X-Request-Id");
    ASSERT_NE(id, nullptr);
    EXPECT_EQ(id->value, "abc");
}

TEST(CurlClient, GetNon2xxIsStatusError) {
    LocalHttpServer server;
    server.enqueue(ok({"nope"}, 401));
    CurlClient client;
    CancelToken cancel;
    auto r = client.get(server.url(), {}, cancel);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().kind, HttpErrorKind::Status);
    EXPECT_EQ(r.error().status, 401);
    EXPECT_EQ(r.error().message, "nope");
}

TEST(CurlClient, SequentialRequestsOnOneClient) {
    LocalHttpServer server;
    server.enqueue(ok({"one"}));
    server.enqueue(ok({"two"}));
    CurlClient client;
    CancelToken cancel;
    auto a = client.get(server.url(), {}, cancel);
    auto b = client.get(server.url(), {}, cancel);
    ASSERT_TRUE(a.has_value()) << a.error().message;
    ASSERT_TRUE(b.has_value()) << b.error().message;
    EXPECT_EQ(a->body, "one");
    EXPECT_EQ(b->body, "two");
}
