// Live test against a local Ollama (`make test-live`).  Skipped when Ollama
// isn't reachable.  Override the server with PI_OLLAMA_URL (default
// http://localhost:11434) and the model with PI_OLLAMA_MODEL (default: the
// smallest installed model whose name doesn't contain "embed").

#include <http/curl_client.h>
#include <http/sse.h>

#include <cstdint>
#include <cstdlib>
#include <string>

#include <gtest/gtest.h>
#include <simdjson.h>

#include <util/aliases.h>
#include <util/cancel.h>

using namespace pi::http;
using pi::util::CancelToken;

static str base_url() {
    const char* env = s::getenv("PI_OLLAMA_URL");
    return env ? env : "http://localhost:11434";
}

// The model to use, or empty if none is available.  Picks the smallest model
// from Ollama's native /api/tags (which, unlike /v1/models, reports sizes) so
// the test runs on whatever fits in memory.
static str pick_model(CurlClient& client) {
    if (const char* env = s::getenv("PI_OLLAMA_MODEL")) return env;
    CancelToken cancel;
    auto r = client.get(base_url() + "/api/tags", {}, cancel);
    if (not r) return {};
    simdjson::dom::parser parser;
    simdjson::dom::element doc;
    if (parser.parse(simdjson::padded_string(r->body)).get(doc)) return {};
    simdjson::dom::array models;
    if (doc["models"].get(models)) return {};
    str best;
    uint64_t best_size = UINT64_MAX;
    for (simdjson::dom::element m : models) {
        strv name;
        uint64_t size = 0;
        if (m["name"].get(name) or m["size"].get(size)) continue;
        // Embedding models can't chat; Ollama doesn't flag them in this listing.
        if (name.find("embed") != strv::npos) continue;
        if (size < best_size) {
            best = name;
            best_size = size;
        }
    }
    return best;
}

TEST(OllamaLive, ListModels) {
    CurlClient client(CurlOptions{.connect_timeout = sc::milliseconds(1000)});
    CancelToken cancel;
    auto r = client.get(base_url() + "/v1/models", {}, cancel);
    if (not r and r.error().kind == HttpErrorKind::Connection)
        GTEST_SKIP() << "Ollama not reachable at " << base_url() << ": " << r.error().message;
    ASSERT_TRUE(r.has_value()) << r.error().message;
    EXPECT_EQ(r->status.code, 200);
    EXPECT_NE(r->body.find("\"data\""), str::npos) << r->body;
}

TEST(OllamaLive, StreamingChatCompletionYieldsSseEvents) {
    CurlClient client(CurlOptions{.connect_timeout = sc::milliseconds(1000)});
    CancelToken probe;
    if (auto r = client.get(base_url() + "/v1/models", {}, probe); not r)
        GTEST_SKIP() << "Ollama not reachable at " << base_url() << ": " << r.error().message;
    str model = pick_model(client);
    if (model.empty()) GTEST_SKIP() << "no model available; `ollama pull` one or set PI_OLLAMA_MODEL";

    str body = R"({"model":")" + model + R"(","stream":true,"max_tokens":16,)"
               R"("messages":[{"role":"user","content":"Say hi."}]})";
    SseDecoder decoder;
    vec<SseEvent> events;
    CancelToken cancel;
    auto r = client.post_stream(base_url() + "/v1/chat/completions",
                                {{"Content-Type", "application/json"}}, body,
                                [&](strv chunk) {
                                    for (SseEvent& e : decoder.feed(chunk)) events.push_back(s::move(e));
                                },
                                cancel);
    ASSERT_TRUE(r.has_value()) << r.error().message;
    ASSERT_GE(events.size(), 1u);
    EXPECT_EQ(events.back().data, "[DONE]");
    for (s::size_t i = 0; i + 1 < events.size(); ++i) {
        EXPECT_EQ(events[i].data.front(), '{') << "event " << i << ": " << events[i].data;
    }
}
