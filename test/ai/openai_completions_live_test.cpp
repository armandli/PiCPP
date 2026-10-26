// Live test of OpenAiCompletionsProvider against a local Ollama (`make test-live`).
// Skipped when Ollama isn't reachable.  PI_OLLAMA_URL overrides the server
// (default http://localhost:11434, no /v1); PI_OLLAMA_MODEL picks the model
// (default: the smallest installed model whose name doesn't contain "embed").

#include <ai/providers/openai_completions.h>

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include <ai/event_helpers.h>
#include <http/curl_client.h>
#include <json/parse.h>
#include <util/aliases.h>
#include <util/cancel.h>

using namespace pi::ai;
using namespace pi::ai::providers;
using namespace pi::ai::test;
using pi::util::CancelToken;
namespace h = pi::http;
namespace j = pi::json;

namespace {
str base_url() {
    const char* env = s::getenv("PI_OLLAMA_URL");
    return env ? env : "http://localhost:11434";
}

// Empty if Ollama is unreachable or has no chat model.
str pick_model(h::CurlClient& client) {
    if (const char* env = s::getenv("PI_OLLAMA_MODEL")) return env;
    CancelToken cancel;
    auto r = client.get(base_url() + "/api/tags", {}, cancel);
    if (not r) return {};
    auto doc = j::parse(r->body);
    if (not doc or doc->find("models") == nullptr or not doc->find("models")->is_array()) return {};
    str best;
    int64_t best_size = INT64_MAX;
    for (const j::Value& m : *doc->find("models")->as_array()) {
        const j::Value* name = m.find("name");
        const j::Value* size = m.find("size");
        if (name == nullptr or size == nullptr or not name->as_string() or not size->as_int()) continue;
        if (name->as_string()->find("embed") != strv::npos) continue;
        if (*size->as_int() < best_size) {
            best = str(*name->as_string());
            best_size = *size->as_int();
        }
    }
    return best;
}
}  // namespace

TEST(OpenAiCompletionsLive, StreamsAReply) {
    auto client = s::make_shared<h::CurlClient>(h::CurlOptions{.connect_timeout = sc::milliseconds(1000)});
    str model_id = pick_model(*client);
    if (model_id.empty()) GTEST_SKIP() << "Ollama not reachable at " << base_url() << " or no chat model installed";

    OpenAiCompletionsProvider provider(client);
    Context context;
    context.system_prompt = "Answer in one short sentence.";
    context.messages = {UserMessage{{TextContent{"Say hi."}}, now_ms()}};
    StreamOptions options;
    options.max_tokens = 256;
    CancelToken cancel;
    EventStream stream = provider.stream(make_ollama_model(model_id, base_url() + "/v1"), context, options, cancel);
    vec<AssistantMessageEvent> ev = drain(stream);

    ASSERT_FALSE(ev.empty());
    EXPECT_EQ(kind_name(ev.front()), "start");
    ASSERT_TRUE(s::holds_alternative<DoneEvent>(ev.back()))
        << kind_name(ev.back()) << ": " << terminal_message(ev).error_message;
    AssistantMessage m = s::get<DoneEvent>(ev.back()).message;
    EXPECT_TRUE(m.stop_reason == StopReason::stop or m.stop_reason == StopReason::length);
    EXPECT_FALSE(m.content.empty()) << "expected some text or thinking";
    EXPECT_GT(m.usage.output, 0);
    EXPECT_EQ(m.model, model_id);
}
