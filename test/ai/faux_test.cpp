#include <ai/providers/faux.h>

#include <string>

#include <gtest/gtest.h>

#include <ai/event_helpers.h>
#include <json/parse.h>
#include <json/write.h>
#include <util/aliases.h>
#include <util/cancel.h>

using namespace pi::ai;
using namespace pi::ai::providers;
using namespace pi::ai::test;
using pi::util::CancelToken;
namespace j = pi::json;

namespace {
Model faux_model() {
    Model m;
    m.id = "faux-1";
    m.provider = "faux";
    m.api = "faux";
    return m;
}

Context ask(str text) {
    Context c;
    c.messages = {UserMessage{{TextContent{s::move(text)}}, 1}};
    return c;
}

vec<AssistantMessageEvent> run(FauxProvider& provider, const Context& context = ask("q")) {
    CancelToken cancel;
    EventStream stream = provider.stream(faux_model(), context, {}, cancel);
    return drain(stream);
}

vec<str> deltas_of(const vec<AssistantMessageEvent>& events) {
    vec<str> out;
    for (const AssistantMessageEvent& e : events) {
        if (const auto* t = s::get_if<TextDeltaEvent>(&e)) out.push_back(t->delta);
        if (const auto* t = s::get_if<ThinkingDeltaEvent>(&e)) out.push_back(t->delta);
        if (const auto* t = s::get_if<ToolCallDeltaEvent>(&e)) out.push_back(t->delta);
    }
    return out;
}
}  // namespace

// ===========================================================================
// FauxResponse
// ===========================================================================

TEST(FauxResponse, TextHelperIsOneTextStep) {
    FauxResponse r = FauxResponse::text("hello");
    ASSERT_EQ(r.steps.size(), 1u);
    ASSERT_TRUE(s::holds_alternative<FauxText>(r.steps[0]));
    EXPECT_EQ(s::get<FauxText>(r.steps[0]).text, "hello");
    EXPECT_FALSE(r.stop_reason.has_value());
    EXPECT_FALSE(r.error.has_value());
}

// ===========================================================================
// Event sequences
// ===========================================================================

TEST(FauxProvider, TextResponse) {
    FauxProvider p;
    p.enqueue(FauxResponse::text("hello"));
    vec<AssistantMessageEvent> ev = run(p);
    EXPECT_EQ(kinds(ev), (vec<str>{"start", "text_start", "text_delta", "text_end", "done"}));
    AssistantMessage m = terminal_message(ev);
    EXPECT_EQ(text_of(m), "hello");
    EXPECT_EQ(m.stop_reason, StopReason::stop);
}

TEST(FauxProvider, ResultIsAvailableWithoutDraining) {
    FauxProvider p;
    p.enqueue(FauxResponse::text("hi"));
    CancelToken cancel;
    EventStream stream = p.stream(faux_model(), ask("q"), {}, cancel);
    EXPECT_TRUE(stream.finished()) << "faux stream() is synchronous";
    EXPECT_EQ(text_of(stream.result()), "hi");
}

TEST(FauxProvider, MessageMetadata) {
    FauxProvider p;
    p.enqueue(FauxResponse::text("x"));
    AssistantMessage m = terminal_message(run(p));
    EXPECT_EQ(m.api, "faux");
    EXPECT_EQ(m.provider, "faux");
    EXPECT_EQ(m.model, "faux-1");
    EXPECT_GT(m.timestamp, 0);
}

TEST(FauxProvider, ChunkSizeSplitsDeltas) {
    FauxProvider p(FauxOptions{.chunk_size = 2});
    p.enqueue(FauxResponse::text("hello"));
    vec<AssistantMessageEvent> ev = run(p);
    EXPECT_EQ(deltas_of(ev), (vec<str>{"he", "ll", "o"}));
    EXPECT_EQ(text_of(terminal_message(ev)), "hello");
}

TEST(FauxProvider, ChunkingNeverSplitsUtf8) {
    FauxProvider p(FauxOptions{.chunk_size = 1});
    p.enqueue(FauxResponse::text("a\xC3\xA9" "b"));  // "aéb"
    EXPECT_EQ(deltas_of(run(p)), (vec<str>{"a", "\xC3\xA9", "b"}));
}

TEST(FauxProvider, ChunkSizeZeroMeansOneDeltaPerStep) {
    FauxProvider p;
    p.enqueue(FauxResponse::text("a long sentence"));
    EXPECT_EQ(deltas_of(run(p)), (vec<str>{"a long sentence"}));
}

TEST(FauxProvider, ThinkingThenText) {
    FauxProvider p;
    p.enqueue(FauxResponse{.steps = {FauxThinking{"hmm"}, FauxText{"4"}}, .stop_reason = {}, .usage = {}, .error = {}});
    vec<AssistantMessageEvent> ev = run(p);
    EXPECT_EQ(kinds(ev), (vec<str>{"start", "thinking_start", "thinking_delta", "thinking_end",
                                   "text_start", "text_delta", "text_end", "done"}));
    EXPECT_EQ(terminal_message(ev).content, (vec<AssistantContent>{ThinkingContent{"hmm"}, TextContent{"4"}}));
}

TEST(FauxProvider, ToolCallStepDefaultsToToolUse) {
    j::Value args = *j::parse(R"({"path":"a.txt"})");
    FauxProvider p;
    p.enqueue(FauxResponse{.steps = {FauxText{"reading"}, FauxToolCall{"call_1", "read", args}}, .stop_reason = {}, .usage = {}, .error = {}});
    vec<AssistantMessageEvent> ev = run(p);
    EXPECT_EQ(collapsed_kinds(ev), (vec<str>{"start", "text_start", "text_delta", "text_end",
                                             "toolcall_start", "toolcall_delta", "toolcall_end", "done"}));
    ASSERT_FALSE(ev.empty());
    ASSERT_TRUE(s::holds_alternative<DoneEvent>(ev.back()));
    EXPECT_EQ(s::get<DoneEvent>(ev.back()).reason, StopReason::toolUse);
    EXPECT_EQ(terminal_message(ev).content, (vec<AssistantContent>{TextContent{"reading"}, ToolCall{"call_1", "read", args}}));
}

TEST(FauxProvider, ToolCallDeltasSpellTheArgumentsJson) {
    j::Value args = *j::parse(R"({"path":"a.txt","limit":10})");
    FauxProvider p(FauxOptions{.chunk_size = 4});
    p.enqueue(FauxResponse{.steps = {FauxToolCall{"c", "read", args}}, .stop_reason = {}, .usage = {}, .error = {}});
    vec<AssistantMessageEvent> ev = run(p);
    str joined;
    for (const AssistantMessageEvent& e : ev)
        if (const auto* d = s::get_if<ToolCallDeltaEvent>(&e)) joined += d->delta;
    EXPECT_EQ(joined, j::to_json(args));
    EXPECT_GT(deltas_of(ev).size(), 1u);
}

TEST(FauxProvider, ExplicitStopReasonWins) {
    FauxProvider p;
    FauxResponse r = FauxResponse::text("trunc");
    r.stop_reason = StopReason::length;
    p.enqueue(r);
    vec<AssistantMessageEvent> ev = run(p);
    ASSERT_FALSE(ev.empty());
    ASSERT_TRUE(s::holds_alternative<DoneEvent>(ev.back()));
    EXPECT_EQ(s::get<DoneEvent>(ev.back()).reason, StopReason::length);
}

TEST(FauxProvider, UsageIsReported) {
    FauxProvider p;
    FauxResponse r = FauxResponse::text("x");
    r.usage = Usage{.input = 5, .output = 7, .cache_read = 0, .cache_write = 0, .total_tokens = 12, .cost = {}};
    p.enqueue(r);
    EXPECT_EQ(terminal_message(run(p)).usage, r.usage);
}

// ===========================================================================
// Errors and cancellation
// ===========================================================================

TEST(FauxProvider, ScriptedErrorAfterSteps) {
    FauxProvider p;
    FauxResponse r = FauxResponse::text("par");
    r.error = "boom";
    p.enqueue(r);
    vec<AssistantMessageEvent> ev = run(p);
    EXPECT_EQ(kinds(ev), (vec<str>{"start", "text_start", "text_delta", "error"}));
    ASSERT_FALSE(ev.empty());
    ASSERT_TRUE(s::holds_alternative<ErrorEvent>(ev.back()));
    const ErrorEvent& e = s::get<ErrorEvent>(ev.back());
    EXPECT_EQ(e.reason, StopReason::error);
    EXPECT_EQ(e.message.error_message, "boom");
    EXPECT_EQ(text_of(e.message), "par");
}

TEST(FauxProvider, NoResponseQueuedIsAnErrorEvent) {
    FauxProvider p;
    vec<AssistantMessageEvent> ev = run(p);
    ASSERT_FALSE(ev.empty());
    ASSERT_TRUE(s::holds_alternative<ErrorEvent>(ev.back())) << kind_name(ev.back());
    EXPECT_EQ(s::get<ErrorEvent>(ev.back()).message.error_message, "no faux response queued");
    EXPECT_EQ(p.calls().size(), 1u) << "the call is recorded even when it fails";
}

TEST(FauxProvider, CancelledBeforeStartIsAborted) {
    FauxProvider p;
    p.enqueue(FauxResponse::text("never"));
    CancelToken cancel;
    cancel.cancel();
    EventStream stream = p.stream(faux_model(), ask("q"), {}, cancel);
    vec<AssistantMessageEvent> ev = drain(stream);
    ASSERT_FALSE(ev.empty());
    ASSERT_TRUE(s::holds_alternative<ErrorEvent>(ev.back())) << kind_name(ev.back());
    EXPECT_EQ(s::get<ErrorEvent>(ev.back()).reason, StopReason::aborted);
    EXPECT_EQ(joined_text_deltas(ev), "");
}

// ===========================================================================
// Bookkeeping
// ===========================================================================

TEST(FauxProvider, ResponsesAreFifoAndContextsRecorded) {
    FauxProvider p;
    p.enqueue(FauxResponse::text("first"));
    p.enqueue(FauxResponse::text("second"));
    EXPECT_EQ(p.pending(), 2u);
    EXPECT_EQ(text_of(terminal_message(run(p, ask("one")))), "first");
    EXPECT_EQ(text_of(terminal_message(run(p, ask("two")))), "second");
    EXPECT_EQ(p.pending(), 0u);
    ASSERT_EQ(p.calls().size(), 2u);
    EXPECT_EQ(p.calls()[0], ask("one"));
    EXPECT_EQ(p.calls()[1], ask("two"));
}

TEST(FauxProvider, IdAndModels) {
    FauxProvider p;
    EXPECT_EQ(p.id(), "faux");
    vec<Model> models = p.models();
    ASSERT_EQ(models.size(), 1u);
    EXPECT_EQ(models[0].id, "faux");
    EXPECT_EQ(models[0].api, "faux");
    EXPECT_EQ(models[0].provider, "faux");
}

TEST(FauxProvider, CustomProviderId) {
    FauxProvider p(FauxOptions{.chunk_size = 0, .provider_id = "fake-ollama"});
    p.enqueue(FauxResponse::text("x"));
    EXPECT_EQ(p.id(), "fake-ollama");
    EXPECT_EQ(terminal_message(run(p)).provider, "fake-ollama");
    EXPECT_EQ(p.models().at(0).provider, "fake-ollama");
}
