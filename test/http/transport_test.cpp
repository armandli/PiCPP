#include <http/fake_transport.h>
#include <http/transport.h>

#include <string>

#include <gtest/gtest.h>

#include <util/aliases.h>
#include <util/cancel.h>

using namespace pi::http;
using pi::util::CancelToken;

static str join(const vec<str>& parts) {
    str out;
    for (const str& p : parts) out += p;
    return out;
}

// ===========================================================================
// find_header
// ===========================================================================

TEST(FindHeader, ExactName) {
    HttpHeaders h = {{"Content-Type", "text/event-stream"}};
    const HttpHeader* found = find_header(h, "Content-Type");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->value, "text/event-stream");
}

TEST(FindHeader, CaseInsensitive) {
    HttpHeaders h = {{"Retry-After", "3"}};
    const HttpHeader* found = find_header(h, "retry-after");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->value, "3");
}

TEST(FindHeader, FirstOfDuplicates) {
    HttpHeaders h = {{"X-A", "1"}, {"x-a", "2"}};
    const HttpHeader* found = find_header(h, "X-A");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->value, "1");
}

TEST(FindHeader, Missing) {
    HttpHeaders h = {{"X-A", "1"}};
    EXPECT_EQ(find_header(h, "X-B"), nullptr);
    EXPECT_EQ(find_header({}, "X-A"), nullptr);
}

TEST(FindHeader, PrefixIsNotAMatch) {
    HttpHeaders h = {{"Content-Type-Extra", "x"}};
    EXPECT_EQ(find_header(h, "Content-Type"), nullptr);
}

// ===========================================================================
// FakeResponse builders
// ===========================================================================

static const str kBody = "data: {\"a\":1}\n\ndata: [DONE]\n\n";

TEST(FakeResponse, FromBodyIsOneChunk) {
    FakeResponse r = FakeResponse::from_body(kBody);
    EXPECT_EQ(r.status, 200);
    ASSERT_EQ(r.chunks.size(), 1u);
    EXPECT_EQ(r.chunks[0], kBody);
}

TEST(FakeResponse, FromBodyKeepsStatus) {
    EXPECT_EQ(FakeResponse::from_body("oops", 503).status, 503);
}

TEST(FakeResponse, FromEmptyBodyHasNoChunks) {
    EXPECT_TRUE(FakeResponse::from_body("").chunks.empty());
}

TEST(FakeResponse, ByteByByte) {
    FakeResponse r = FakeResponse::byte_by_byte(kBody);
    ASSERT_EQ(r.chunks.size(), kBody.size());
    for (const str& c : r.chunks) EXPECT_EQ(c.size(), 1u);
    EXPECT_EQ(join(r.chunks), kBody);
}

TEST(FakeResponse, SplitRandomlyReassembles) {
    for (uint32_t seed = 0; seed < 50; ++seed) {
        FakeResponse r = FakeResponse::split_randomly(kBody, seed, 5);
        EXPECT_EQ(join(r.chunks), kBody) << "seed=" << seed;
        for (const str& c : r.chunks) {
            EXPECT_GE(c.size(), 1u);
            EXPECT_LE(c.size(), 5u);
        }
    }
}

TEST(FakeResponse, SplitRandomlyIsDeterministic) {
    EXPECT_EQ(FakeResponse::split_randomly(kBody, 7).chunks, FakeResponse::split_randomly(kBody, 7).chunks);
}

TEST(FakeResponse, SplitRandomlyDependsOnSeed) {
    // With 16-byte max chunks over ~30 bytes, at least one of a few seeds differs.
    auto base = FakeResponse::split_randomly(kBody, 1).chunks;
    bool differs = false;
    for (uint32_t seed = 2; seed < 10; ++seed) {
        differs = differs or FakeResponse::split_randomly(kBody, seed).chunks != base;
    }
    EXPECT_TRUE(differs);
}

// ===========================================================================
// FakeTransport
// ===========================================================================

TEST(FakeTransport, RecordsPostRequest) {
    FakeTransport t;
    t.enqueue(FakeResponse::from_body("ok"));
    CancelToken cancel;
    HttpHeaders headers = {{"Content-Type", "application/json"}, {"Authorization", "Bearer k"}};
    auto r = t.post_stream("http://x/v1/chat/completions", headers, R"({"stream":true})",
                           [](strv) {}, cancel);
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(t.requests().size(), 1u);
    const RecordedRequest& req = t.requests()[0];
    EXPECT_EQ(req.method, "POST");
    EXPECT_EQ(req.url, "http://x/v1/chat/completions");
    EXPECT_EQ(req.headers, headers);
    EXPECT_EQ(req.body, R"({"stream":true})");
}

TEST(FakeTransport, DeliversChunksInOrder) {
    FakeTransport t;
    FakeResponse resp;
    resp.chunks = {"a", "bc", "def"};
    t.enqueue(resp);
    CancelToken cancel;
    vec<str> got;
    auto r = t.post_stream("u", {}, "", [&](strv c) { got.emplace_back(c); }, cancel);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(got, (vec<str>{"a", "bc", "def"}));
}

TEST(FakeTransport, ReturnsStatusAndHeaders) {
    FakeTransport t;
    FakeResponse resp = FakeResponse::from_body("x", 201);
    resp.headers = {{"Content-Type", "text/event-stream"}};
    t.enqueue(resp);
    CancelToken cancel;
    auto r = t.post_stream("u", {}, "", [](strv) {}, cancel);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->code, 201);
    EXPECT_EQ(r->headers, resp.headers);
}

TEST(FakeTransport, ResponsesAreFifo) {
    FakeTransport t;
    t.enqueue(FakeResponse::from_body("first"));
    t.enqueue(FakeResponse::from_body("second"));
    EXPECT_EQ(t.pending(), 2u);
    CancelToken cancel;
    str got;
    ASSERT_TRUE(t.post_stream("u", {}, "", [&](strv c) { got += c; }, cancel).has_value());
    EXPECT_EQ(got, "first");
    got.clear();
    ASSERT_TRUE(t.post_stream("u", {}, "", [&](strv c) { got += c; }, cancel).has_value());
    EXPECT_EQ(got, "second");
    EXPECT_EQ(t.pending(), 0u);
}

TEST(FakeTransport, NoResponseQueuedIsConnectionError) {
    FakeTransport t;
    CancelToken cancel;
    auto r = t.post_stream("u", {}, "body", [](strv) {}, cancel);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().kind, HttpErrorKind::Connection);
    // The request is still recorded.
    EXPECT_EQ(t.requests().size(), 1u);
}

TEST(FakeTransport, Non2xxIsStatusErrorWithBody) {
    FakeTransport t;
    FakeResponse resp;
    resp.status = 404;
    resp.chunks = {R"({"error":"model )", R"('x' not found"})"};
    t.enqueue(resp);
    CancelToken cancel;
    bool called = false;
    auto r = t.post_stream("u", {}, "", [&](strv) { called = true; }, cancel);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().kind, HttpErrorKind::Status);
    EXPECT_EQ(r.error().status, 404);
    EXPECT_EQ(r.error().message, R"({"error":"model 'x' not found"})");
    EXPECT_FALSE(called) << "on_chunk must not see a non-2xx body";
}

TEST(FakeTransport, ScriptedErrorAfterChunks) {
    FakeTransport t;
    FakeResponse resp;
    resp.chunks = {"data: a\n\n"};
    resp.error = HttpError{HttpErrorKind::Connection, 0, "connection reset"};
    t.enqueue(resp);
    CancelToken cancel;
    str got;
    auto r = t.post_stream("u", {}, "", [&](strv c) { got += c; }, cancel);
    EXPECT_EQ(got, "data: a\n\n");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().kind, HttpErrorKind::Connection);
    EXPECT_EQ(r.error().message, "connection reset");
}

TEST(FakeTransport, CancelledBeforeStart) {
    FakeTransport t;
    t.enqueue(FakeResponse::from_body("never"));
    CancelToken cancel;
    cancel.cancel();
    bool called = false;
    auto r = t.post_stream("u", {}, "", [&](strv) { called = true; }, cancel);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().kind, HttpErrorKind::Cancelled);
    EXPECT_FALSE(called);
}

TEST(FakeTransport, CancelMidStreamStopsDelivery) {
    FakeTransport t;
    FakeResponse resp;
    resp.chunks = {"1", "2", "3", "4"};
    t.enqueue(resp);
    CancelToken cancel;
    vec<str> got;
    auto r = t.post_stream("u", {}, "", [&](strv c) {
        got.emplace_back(c);
        if (got.size() == 2) cancel.cancel();
    }, cancel);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().kind, HttpErrorKind::Cancelled);
    EXPECT_EQ(got, (vec<str>{"1", "2"}));
}

TEST(FakeTransport, GetReturnsJoinedBody) {
    FakeTransport t;
    FakeResponse resp;
    resp.chunks = {R"({"data":[)", R"({"id":"qwen"}]})"};
    t.enqueue(resp);
    CancelToken cancel;
    auto r = t.get("http://x/v1/models", {{"Authorization", "Bearer k"}}, cancel);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->status.code, 200);
    EXPECT_EQ(r->body, R"({"data":[{"id":"qwen"}]})");
    ASSERT_EQ(t.requests().size(), 1u);
    EXPECT_EQ(t.requests()[0].method, "GET");
    EXPECT_EQ(t.requests()[0].url, "http://x/v1/models");
    EXPECT_TRUE(t.requests()[0].body.empty());
}

TEST(FakeTransport, GetNon2xxIsStatusError) {
    FakeTransport t;
    t.enqueue(FakeResponse::from_body("unauthorized", 401));
    CancelToken cancel;
    auto r = t.get("u", {}, cancel);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().kind, HttpErrorKind::Status);
    EXPECT_EQ(r.error().status, 401);
    EXPECT_EQ(r.error().message, "unauthorized");
}

TEST(FakeTransport, UsableThroughBaseInterface) {
    FakeTransport fake;
    fake.enqueue(FakeResponse::from_body("x"));
    HttpTransport& t = fake;
    CancelToken cancel;
    EXPECT_TRUE(t.post_stream("u", {}, "", [](strv) {}, cancel).has_value());
}
