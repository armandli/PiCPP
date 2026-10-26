#include <ai/providers/openai_completions.h>

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include <ai/event_helpers.h>
#include <http/fake_transport.h>
#include <json/parse.h>
#include <json/write.h>
#include <util/aliases.h>
#include <util/cancel.h>

using namespace pi::ai;
using namespace pi::ai::providers;
using namespace pi::ai::test;
using pi::util::CancelToken;
namespace h = pi::http;
namespace j = pi::json;

namespace {
// Recorded from Ollama 0.40.11 / qwen3.8:27b-mlx: reasoning first (each chunk
// also carries "content":""), then content, finish_reason "stop", then a usage
// chunk with empty choices, then [DONE].
constexpr strv kFixture = "ai/fixtures/ollama_thinking_text.sse";
constexpr strv kFixtureThinking = "The user asks a simple math question. 2+2=4. They want a short sentence answer.";
constexpr strv kFixtureText = "2 + 2 equals 4.";

j::Value parse_or_die(strv text) {
    auto v = j::parse(text);
    EXPECT_TRUE(v.has_value()) << "bad JSON: " << text;
    return v ? *v : j::Value{};
}

Model ollama(str id = "qwen3.8:27b-mlx") {
    return make_ollama_model(s::move(id), "http://localhost:11434/v1");
}

Context ask(str text, str system = "") {
    Context c;
    c.system_prompt = s::move(system);
    c.messages = {UserMessage{{TextContent{s::move(text)}}, 1}};
    return c;
}

const h::HttpHeader* header(const h::HttpHeaders& headers, strv name) {
    return h::find_header(headers, name);
}

// Runs one request through a FakeTransport and drains the events.
struct Harness {
    sptr<h::FakeTransport> transport = s::make_shared<h::FakeTransport>();
    OpenAiCompletionsProvider provider{transport};
    CancelToken cancel;

    vec<AssistantMessageEvent> run(const Context& context = ask("What is 2+2?"), const StreamOptions& options = {}) {
        EventStream stream = provider.stream(ollama(), context, options, cancel);
        return drain(stream);
    }
};

// Feeds payloads straight into a ChunkDecoder.
struct DecoderHarness {
    EventStream   stream;
    StreamBuilder builder{stream, AssistantMessage{.content = {}, .api = "openai-completions", .provider = "ollama",
                                                   .model = "m", .usage = {}, .stop_reason = StopReason::stop,
                                                   .error_message = {}, .timestamp = 1}};
    ChunkDecoder  decoder{builder};

    vec<AssistantMessageEvent> events() { return drain(stream); }
};

str delta_chunk(strv delta_json, strv finish_reason = "null") {
    return R"({"choices":[{"index":0,"delta":)" + str(delta_json) + R"(,"finish_reason":)" + str(finish_reason) + "}]}";
}

// Delivers its first chunk, then waits (up to 5 s) for cancellation, like a
// slow server.  Lets a test cancel mid-stream deterministically.
struct StallingTransport : h::HttpTransport {
    str first_chunk;

    s::expected<h::HttpStatus, h::HttpError> post_stream(
        strv, const h::HttpHeaders&, strv, h::ChunkCallback on_chunk, const CancelToken& cancel) override {
        on_chunk(first_chunk);
        auto deadline = sc::steady_clock::now() + sc::seconds(5);
        while (not cancel.is_cancelled() and sc::steady_clock::now() < deadline) st::sleep_for(sc::milliseconds(2));
        if (cancel.is_cancelled()) return s::unexpected(h::HttpError{h::HttpErrorKind::Cancelled, 0, "cancelled"});
        return s::unexpected(h::HttpError{h::HttpErrorKind::Timeout, 0, "test timed out waiting for cancel"});
    }
    s::expected<h::HttpResponse, h::HttpError> get(strv, const h::HttpHeaders&, const CancelToken&) override {
        return s::unexpected(h::HttpError{h::HttpErrorKind::Other, 0, "unused"});
    }
};
}  // namespace

// ===========================================================================
// make_ollama_model
// ===========================================================================

TEST(MakeOllamaModel, Fields) {
    Model m = make_ollama_model("qwen3.8:27b-mlx", "http://localhost:11434/v1");
    EXPECT_EQ(m.id, "qwen3.8:27b-mlx");
    EXPECT_EQ(m.name, "qwen3.8:27b-mlx");
    EXPECT_EQ(m.api, "openai-completions");
    EXPECT_EQ(m.api, kOpenAiCompletionsApi);
    EXPECT_EQ(m.provider, "ollama");
    EXPECT_EQ(m.base_url, "http://localhost:11434/v1");
    EXPECT_TRUE(m.reasoning);
    EXPECT_EQ(m.context_window, 32768);
    EXPECT_EQ(m.max_tokens, 8192);
    EXPECT_EQ(m.compat, ollama_compat());
}

TEST(MakeOllamaModel, StripsTrailingSlash) {
    EXPECT_EQ(make_ollama_model("m", "http://host:1/v1/").base_url, "http://host:1/v1");
}

TEST(MakeOllamaModel, DefaultBaseUrl) {
    EXPECT_EQ(make_ollama_model("m").base_url, "http://localhost:11434/v1");
}

// ===========================================================================
// convert_messages
// ===========================================================================

TEST(ConvertMessages, SystemPromptUsesSystemRoleForOllama) {
    j::Value msgs = convert_messages(ollama(), ask("hi", "be brief"));
    EXPECT_EQ(j::to_json(msgs), R"([{"role":"system","content":"be brief"},{"role":"user","content":"hi"}])");
}

TEST(ConvertMessages, DeveloperRoleWhenSupportedAndReasoning) {
    Model m = ollama();
    m.compat = OpenAiCompat{};
    m.reasoning = true;
    EXPECT_EQ(j::to_json(convert_messages(m, ask("hi", "sys"))),
              R"([{"role":"developer","content":"sys"},{"role":"user","content":"hi"}])");
    m.reasoning = false;
    EXPECT_EQ(j::to_json(convert_messages(m, ask("hi", "sys"))),
              R"([{"role":"system","content":"sys"},{"role":"user","content":"hi"}])");
}

TEST(ConvertMessages, EmptySystemPromptIsOmitted) {
    EXPECT_EQ(j::to_json(convert_messages(ollama(), ask("hi"))), R"([{"role":"user","content":"hi"}])");
}

TEST(ConvertMessages, MultipleTextBlocksJoinWithNewline) {
    Context c;
    c.messages = {UserMessage{{TextContent{"a"}, TextContent{"b"}}, 1}};
    EXPECT_EQ(j::to_json(convert_messages(ollama(), c)), R"([{"role":"user","content":"a\nb"}])");
}

TEST(ConvertMessages, ImagesUseContentArray) {
    Context c;
    c.messages = {UserMessage{{TextContent{"what is this?"}, ImageContent{"AAAA", "image/png"}}, 1}};
    EXPECT_EQ(j::to_json(convert_messages(ollama(), c)),
              R"([{"role":"user","content":[{"type":"text","text":"what is this?"},)"
              R"({"type":"image_url","image_url":{"url":"data:image/png;base64,AAAA"}}]}])");
}

TEST(ConvertMessages, AssistantKeepsTextDropsThinking) {
    AssistantMessage a;
    a.content = {ThinkingContent{"secret"}, TextContent{"4"}};
    Context c = ask("2+2?");
    c.messages.push_back(a);
    c.messages.push_back(UserMessage{{TextContent{"and 3+3?"}}, 2});
    EXPECT_EQ(j::to_json(convert_messages(ollama(), c)),
              R"([{"role":"user","content":"2+2?"},{"role":"assistant","content":"4"},)"
              R"({"role":"user","content":"and 3+3?"}])");
}

TEST(ConvertMessages, AssistantWithoutTextIsSkipped) {
    AssistantMessage a;
    a.content = {ThinkingContent{"only thinking"}};
    Context c = ask("q");
    c.messages.push_back(a);
    EXPECT_EQ(j::to_json(convert_messages(ollama(), c)), R"([{"role":"user","content":"q"}])");
}

TEST(ConvertMessages, ToolResultsAreSkippedUntilM5) {
    Context c = ask("q");
    c.messages.push_back(ToolResultMessage{"call_1", "read", {TextContent{"x"}}, false, 2});
    EXPECT_EQ(j::to_json(convert_messages(ollama(), c)), R"([{"role":"user","content":"q"}])");
}

// ===========================================================================
// build_request
// ===========================================================================

TEST(BuildRequest, OllamaDefaults) {
    EXPECT_EQ(j::to_json(build_request(ollama("m"), ask("hi"), {})),
              R"({"model":"m","messages":[{"role":"user","content":"hi"}],"stream":true,)"
              R"("stream_options":{"include_usage":true},"max_tokens":8192})");
}

TEST(BuildRequest, OptionsOverrideMaxTokensAndAddTemperature) {
    StreamOptions o;
    o.max_tokens = 16;
    o.temperature = 0.5;
    EXPECT_EQ(j::to_json(build_request(ollama("m"), ask("hi"), o)),
              R"({"model":"m","messages":[{"role":"user","content":"hi"}],"stream":true,)"
              R"("stream_options":{"include_usage":true},"max_tokens":16,"temperature":0.5})");
}

TEST(BuildRequest, MaxTokensFieldFollowsCompat) {
    Model m = ollama("m");
    m.compat = OpenAiCompat{};
    j::Value body = build_request(m, ask("hi"), {});
    EXPECT_NE(body.find("max_completion_tokens"), nullptr);
    EXPECT_EQ(body.find("max_tokens"), nullptr);
}

TEST(BuildRequest, MaxTokensOmittedWhenUnknown) {
    Model m = ollama("m");
    m.max_tokens = 0;
    j::Value body = build_request(m, ask("hi"), {});
    EXPECT_EQ(body.find("max_tokens"), nullptr);
    EXPECT_EQ(body.find("max_completion_tokens"), nullptr);
}

TEST(BuildRequest, NoStreamOptionsWithoutUsageSupport) {
    Model m = ollama("m");
    m.compat.supports_usage_in_streaming = false;
    EXPECT_EQ(build_request(m, ask("hi"), {}).find("stream_options"), nullptr);
}

TEST(BuildRequest, NeverSendsReasoningEffortToOllama) {
    EXPECT_EQ(build_request(ollama("m"), ask("hi"), {}).find("reasoning_effort"), nullptr);
}

// ===========================================================================
// map_stop_reason / parse_usage
// ===========================================================================

TEST(MapStopReason, Table) {
    EXPECT_EQ(map_stop_reason("stop"), StopReason::stop);
    EXPECT_EQ(map_stop_reason("length"), StopReason::length);
    EXPECT_EQ(map_stop_reason("tool_calls"), StopReason::toolUse);
    EXPECT_EQ(map_stop_reason("function_call"), StopReason::toolUse);
    EXPECT_EQ(map_stop_reason("content_filter"), StopReason::error);
    EXPECT_EQ(map_stop_reason(""), StopReason::error);
}

TEST(ParseUsage, FromOllamaChunk) {
    Usage u = parse_usage(parse_or_die(
        R"({"prompt_tokens":28,"prompt_tokens_details":{"cached_tokens":0},"completion_tokens":33,"total_tokens":61})"));
    EXPECT_EQ(u.input, 28);
    EXPECT_EQ(u.output, 33);
    EXPECT_EQ(u.cache_read, 0);
    EXPECT_EQ(u.total_tokens, 61);
}

TEST(ParseUsage, CachedTokensAreNotInput) {
    Usage u = parse_usage(parse_or_die(
        R"({"prompt_tokens":100,"prompt_tokens_details":{"cached_tokens":60},"completion_tokens":5,"total_tokens":105})"));
    EXPECT_EQ(u.input, 40);
    EXPECT_EQ(u.cache_read, 60);
    EXPECT_EQ(u.output, 5);
    EXPECT_EQ(u.total_tokens, 105);
}

TEST(ParseUsage, MissingTotalIsComputed) {
    Usage u = parse_usage(parse_or_die(R"({"prompt_tokens":10,"completion_tokens":5})"));
    EXPECT_EQ(u.input, 10);
    EXPECT_EQ(u.total_tokens, 15);
}

TEST(ParseUsage, GarbageIsZero) {
    EXPECT_EQ(parse_usage(parse_or_die(R"({"prompt_tokens":"many"})")), Usage{});
    EXPECT_EQ(parse_usage(parse_or_die("null")), Usage{});
}

// ===========================================================================
// ChunkDecoder
// ===========================================================================

TEST(ChunkDecoder, TextThenDone) {
    DecoderHarness d;
    d.decoder.on_data(delta_chunk(R"({"role":"assistant","content":"Hi"})"));
    d.decoder.on_data(delta_chunk(R"({"content":" there"})"));
    d.decoder.on_data(delta_chunk("{}", R"("stop")"));
    d.decoder.on_data("[DONE]");
    vec<AssistantMessageEvent> ev = d.events();
    EXPECT_EQ(kinds(ev), (vec<str>{"start", "text_start", "text_delta", "text_delta", "text_end", "done"}));
    EXPECT_EQ(text_of(terminal_message(ev)), "Hi there");
}

TEST(ChunkDecoder, ReasoningBecomesThinkingAndEmptyContentIsIgnored) {
    DecoderHarness d;
    d.decoder.on_data(delta_chunk(R"({"content":"","reasoning":"hmm"})"));
    d.decoder.on_data(delta_chunk(R"({"content":"ok"})"));
    d.decoder.on_data(delta_chunk("{}", R"("stop")"));
    d.decoder.on_data("[DONE]");
    vec<AssistantMessageEvent> ev = d.events();
    EXPECT_EQ(kinds(ev), (vec<str>{"start", "thinking_start", "thinking_delta", "thinking_end",
                                   "text_start", "text_delta", "text_end", "done"}));
    EXPECT_EQ(terminal_message(ev).content, (vec<AssistantContent>{ThinkingContent{"hmm"}, TextContent{"ok"}}));
}

TEST(ChunkDecoder, ReasoningContentAlias) {
    DecoderHarness d;
    d.decoder.on_data(delta_chunk(R"({"reasoning_content":"deep"})"));
    d.decoder.on_data("[DONE]");
    EXPECT_EQ(joined_thinking_deltas(d.events()), "deep");
}

TEST(ChunkDecoder, UsageAfterFinishReasonIsKept) {
    DecoderHarness d;
    d.decoder.on_data(delta_chunk(R"({"content":"x"})"));
    d.decoder.on_data(delta_chunk("{}", R"("length")"));
    d.decoder.on_data(R"({"choices":[],"usage":{"prompt_tokens":3,"completion_tokens":4,"total_tokens":7}})");
    d.decoder.on_data("[DONE]");
    vec<AssistantMessageEvent> ev = d.events();
    ASSERT_FALSE(ev.empty());
    ASSERT_TRUE(s::holds_alternative<DoneEvent>(ev.back())) << kind_name(ev.back());
    const DoneEvent& done = s::get<DoneEvent>(ev.back());
    EXPECT_EQ(done.reason, StopReason::length);
    EXPECT_EQ(done.message.usage.input, 3);
    EXPECT_EQ(done.message.usage.output, 4);
    EXPECT_EQ(done.message.usage.total_tokens, 7);
}

TEST(ChunkDecoder, DoneWithoutFinishReasonIsStop) {
    DecoderHarness d;
    d.decoder.on_data(delta_chunk(R"({"content":"x"})"));
    d.decoder.on_data("[DONE]");
    vec<AssistantMessageEvent> ev = d.events();
    ASSERT_FALSE(ev.empty());
    ASSERT_TRUE(s::holds_alternative<DoneEvent>(ev.back()));
    EXPECT_EQ(s::get<DoneEvent>(ev.back()).reason, StopReason::stop);
}

TEST(ChunkDecoder, EndWithoutDoneUsesFinishReason) {
    DecoderHarness d;
    d.decoder.on_data(delta_chunk(R"({"content":"x"})", R"("stop")"));
    d.decoder.on_end();
    vec<AssistantMessageEvent> ev = d.events();
    ASSERT_FALSE(ev.empty());
    EXPECT_EQ(kind_name(ev.back()), "done");
}

TEST(ChunkDecoder, EndWithoutFinishReasonIsError) {
    DecoderHarness d;
    d.decoder.on_data(delta_chunk(R"({"content":"cut"})"));
    d.decoder.on_end();
    vec<AssistantMessageEvent> ev = d.events();
    ASSERT_FALSE(ev.empty());
    ASSERT_TRUE(s::holds_alternative<ErrorEvent>(ev.back())) << kind_name(ev.back());
    const ErrorEvent& e = s::get<ErrorEvent>(ev.back());
    EXPECT_EQ(e.reason, StopReason::error);
    EXPECT_EQ(e.message.error_message, "stream ended without finish_reason");
    EXPECT_EQ(text_of(e.message), "cut");
}

TEST(ChunkDecoder, EndAfterDoneIsANoop) {
    DecoderHarness d;
    d.decoder.on_data(delta_chunk(R"({"content":"x"})", R"("stop")"));
    d.decoder.on_data("[DONE]");
    d.decoder.on_end();
    EXPECT_EQ(kinds(d.events()), (vec<str>{"start", "text_start", "text_delta", "text_end", "done"}));
}

TEST(ChunkDecoder, InvalidJsonFails) {
    DecoderHarness d;
    d.decoder.on_data("{not json");
    vec<AssistantMessageEvent> ev = d.events();
    ASSERT_FALSE(ev.empty());
    ASSERT_TRUE(s::holds_alternative<ErrorEvent>(ev.back())) << kind_name(ev.back());
    EXPECT_TRUE(s::get<ErrorEvent>(ev.back()).message.error_message.starts_with("invalid chunk"))
        << s::get<ErrorEvent>(ev.back()).message.error_message;
}

TEST(ChunkDecoder, ErrorObjectFails) {
    DecoderHarness d;
    d.decoder.on_data(delta_chunk(R"({"content":"x"})"));
    d.decoder.on_data(R"({"error":{"message":"model ran out of memory"}})");
    vec<AssistantMessageEvent> ev = d.events();
    ASSERT_FALSE(ev.empty());
    ASSERT_TRUE(s::holds_alternative<ErrorEvent>(ev.back())) << kind_name(ev.back());
    EXPECT_EQ(s::get<ErrorEvent>(ev.back()).message.error_message, "model ran out of memory");
}

TEST(ChunkDecoder, PayloadsAfterTerminalAreIgnored) {
    DecoderHarness d;
    d.decoder.on_data("[DONE]");
    d.decoder.on_data(delta_chunk(R"({"content":"late"})"));
    vec<AssistantMessageEvent> ev = d.events();
    EXPECT_EQ(joined_text_deltas(ev), "");
    ASSERT_FALSE(ev.empty());
    EXPECT_EQ(kind_name(ev.back()), "done");
}

// ===========================================================================
// Provider: recorded Ollama stream
// ===========================================================================

TEST(OpenAiCompletions, RecordedStreamDecodes) {
    str bytes = read_fixture(kFixture);
    ASSERT_FALSE(bytes.empty()) << "missing fixture " << kFixture;
    Harness t;
    t.transport->enqueue(h::FakeResponse::from_body(bytes));
    vec<AssistantMessageEvent> ev = t.run();

    EXPECT_EQ(collapsed_kinds(ev), (vec<str>{"start", "thinking_start", "thinking_delta", "thinking_end",
                                             "text_start", "text_delta", "text_end", "done"}));
    EXPECT_EQ(joined_thinking_deltas(ev), kFixtureThinking);
    EXPECT_EQ(joined_text_deltas(ev), kFixtureText);

    AssistantMessage m = terminal_message(ev);
    EXPECT_EQ(m.content, (vec<AssistantContent>{ThinkingContent{str(kFixtureThinking)}, TextContent{str(kFixtureText)}}));
    EXPECT_EQ(m.stop_reason, StopReason::stop);
    EXPECT_EQ(m.usage.input, 28);
    EXPECT_EQ(m.usage.output, 33);
    EXPECT_EQ(m.usage.total_tokens, 61);
    EXPECT_EQ(m.api, "openai-completions");
    EXPECT_EQ(m.provider, "ollama");
    EXPECT_EQ(m.model, "qwen3.8:27b-mlx");
}

TEST(OpenAiCompletions, BlockIndicesFollowContent) {
    Harness t;
    t.transport->enqueue(h::FakeResponse::from_body(read_fixture(kFixture)));
    vec<AssistantMessageEvent> ev = t.run();
    ASSERT_FALSE(ev.empty());
    for (const AssistantMessageEvent& e : ev) {
        if (const auto* x = s::get_if<ThinkingDeltaEvent>(&e)) EXPECT_EQ(x->content_index, 0u);
        if (const auto* x = s::get_if<TextDeltaEvent>(&e)) EXPECT_EQ(x->content_index, 1u);
    }
}

TEST(OpenAiCompletions, ByteByByteGivesTheSameEvents) {
    str bytes = read_fixture(kFixture);
    Harness whole;
    whole.transport->enqueue(h::FakeResponse::from_body(bytes));
    Harness bytewise;
    bytewise.transport->enqueue(h::FakeResponse::byte_by_byte(bytes));
    vec<AssistantMessageEvent> a = whole.run();
    vec<AssistantMessageEvent> b = bytewise.run();
    ASSERT_FALSE(a.empty());
    // timestamps differ between runs; compare everything else
    EXPECT_EQ(kinds(a), kinds(b));
    EXPECT_EQ(terminal_message(a).content, terminal_message(b).content);
    EXPECT_EQ(terminal_message(a).usage, terminal_message(b).usage);
}

TEST(OpenAiCompletions, RandomSplitsGiveTheSameEvents) {
    str bytes = read_fixture(kFixture);
    Harness whole;
    whole.transport->enqueue(h::FakeResponse::from_body(bytes));
    vec<AssistantMessageEvent> expected = whole.run();
    ASSERT_FALSE(expected.empty());
    for (uint32_t seed = 1; seed <= 20; ++seed) {
        Harness split;
        split.transport->enqueue(h::FakeResponse::split_randomly(bytes, seed, 37));
        vec<AssistantMessageEvent> got = split.run();
        EXPECT_EQ(kinds(got), kinds(expected)) << "seed " << seed;
        EXPECT_EQ(terminal_message(got).content, terminal_message(expected).content) << "seed " << seed;
    }
}

// ===========================================================================
// Provider: the request
// ===========================================================================

TEST(OpenAiCompletions, PostsToChatCompletions) {
    Harness t;
    t.transport->enqueue(h::FakeResponse::from_body(read_fixture(kFixture)));
    t.run(ask("What is 2+2?", "Answer in one short sentence."));
    ASSERT_EQ(t.transport->requests().size(), 1u);
    const h::RecordedRequest& r = t.transport->requests()[0];
    EXPECT_EQ(r.method, "POST");
    EXPECT_EQ(r.url, "http://localhost:11434/v1/chat/completions");
}

TEST(OpenAiCompletions, BodyIsBuildRequest) {
    Harness t;
    t.transport->enqueue(h::FakeResponse::from_body(read_fixture(kFixture)));
    StreamOptions o;
    o.max_tokens = 400;
    Context c = ask("What is 2+2?", "Answer in one short sentence.");
    t.run(c, o);
    ASSERT_EQ(t.transport->requests().size(), 1u);
    auto sent = j::parse(t.transport->requests()[0].body);
    ASSERT_TRUE(sent.has_value()) << t.transport->requests()[0].body;
    EXPECT_EQ(*sent, build_request(ollama(), c, o));
}

TEST(OpenAiCompletions, Headers) {
    Harness t;
    t.transport->enqueue(h::FakeResponse::from_body(read_fixture(kFixture)));
    t.run();
    ASSERT_EQ(t.transport->requests().size(), 1u);
    const h::HttpHeaders& hs = t.transport->requests()[0].headers;
    ASSERT_NE(header(hs, "Content-Type"), nullptr);
    EXPECT_EQ(header(hs, "Content-Type")->value, "application/json");
    ASSERT_NE(header(hs, "Accept"), nullptr);
    EXPECT_EQ(header(hs, "Accept")->value, "text/event-stream");
    EXPECT_EQ(header(hs, "Authorization"), nullptr) << "no api key → no Authorization header";
}

TEST(OpenAiCompletions, ApiKeyBecomesBearerToken) {
    Harness t;
    t.transport->enqueue(h::FakeResponse::from_body(read_fixture(kFixture)));
    StreamOptions o;
    o.api_key = "sk-test";
    t.run(ask("q"), o);
    ASSERT_EQ(t.transport->requests().size(), 1u);
    const h::HttpHeader* auth = header(t.transport->requests()[0].headers, "Authorization");
    ASSERT_NE(auth, nullptr);
    EXPECT_EQ(auth->value, "Bearer sk-test");
}

TEST(OpenAiCompletions, ExtraHeadersAreSent) {
    auto transport = s::make_shared<h::FakeTransport>();
    transport->enqueue(h::FakeResponse::from_body(read_fixture(kFixture)));
    OpenAiCompletionsProvider provider(transport, OpenAiCompletionsOptions{.provider_id = "ollama",
                                                                           .extra_headers = {{"X-Test", "1"}}});
    CancelToken cancel;
    EventStream stream = provider.stream(ollama(), ask("q"), {}, cancel);
    stream.result();
    ASSERT_EQ(transport->requests().size(), 1u);
    const h::HttpHeader* x = header(transport->requests()[0].headers, "X-Test");
    ASSERT_NE(x, nullptr);
    EXPECT_EQ(x->value, "1");
}

// ===========================================================================
// Provider: failures
// ===========================================================================

TEST(OpenAiCompletions, HttpErrorStatusIsErrorEvent) {
    Harness t;
    t.transport->enqueue(h::FakeResponse::from_body(R"({"error":"model 'nope' not found"})", 404));
    vec<AssistantMessageEvent> ev = t.run();
    ASSERT_FALSE(ev.empty());
    ASSERT_TRUE(s::holds_alternative<ErrorEvent>(ev.back())) << kind_name(ev.back());
    const ErrorEvent& e = s::get<ErrorEvent>(ev.back());
    EXPECT_EQ(e.reason, StopReason::error);
    EXPECT_EQ(e.message.error_message, R"(HTTP 404: {"error":"model 'nope' not found"})");
}

TEST(OpenAiCompletions, ConnectionErrorIsErrorEvent) {
    Harness t;  // nothing queued → FakeTransport reports a Connection error
    vec<AssistantMessageEvent> ev = t.run();
    ASSERT_FALSE(ev.empty());
    ASSERT_TRUE(s::holds_alternative<ErrorEvent>(ev.back())) << kind_name(ev.back());
    EXPECT_EQ(s::get<ErrorEvent>(ev.back()).message.error_message, "no fake response queued");
}

TEST(OpenAiCompletions, MidStreamTransportErrorKeepsPartialContent) {
    Harness t;
    h::FakeResponse r;
    r.chunks = {"data: " + delta_chunk(R"({"content":"Hel"})") + "\n\n"};
    r.error = h::HttpError{h::HttpErrorKind::Connection, 0, "connection reset"};
    t.transport->enqueue(r);
    vec<AssistantMessageEvent> ev = t.run();
    ASSERT_FALSE(ev.empty());
    ASSERT_TRUE(s::holds_alternative<ErrorEvent>(ev.back())) << kind_name(ev.back());
    const ErrorEvent& e = s::get<ErrorEvent>(ev.back());
    EXPECT_EQ(e.reason, StopReason::error);
    EXPECT_EQ(e.message.error_message, "connection reset");
    EXPECT_EQ(text_of(e.message), "Hel");
}

TEST(OpenAiCompletions, BodyEndingWithoutFinishIsError) {
    Harness t;
    t.transport->enqueue(h::FakeResponse::from_body("data: " + delta_chunk(R"({"content":"cut"})") + "\n\n"));
    AssistantMessage m = terminal_message(t.run());
    EXPECT_EQ(m.stop_reason, StopReason::error);
    EXPECT_EQ(m.error_message, "stream ended without finish_reason");
}

TEST(OpenAiCompletions, CancelledBeforeStartIsAborted) {
    Harness t;
    t.transport->enqueue(h::FakeResponse::from_body(read_fixture(kFixture)));
    t.cancel.cancel();
    vec<AssistantMessageEvent> ev = t.run();
    ASSERT_FALSE(ev.empty());
    ASSERT_TRUE(s::holds_alternative<ErrorEvent>(ev.back())) << kind_name(ev.back());
    EXPECT_EQ(s::get<ErrorEvent>(ev.back()).reason, StopReason::aborted);
}

TEST(OpenAiCompletions, CancelMidStreamIsAbortedWithPartialContent) {
    auto transport = s::make_shared<StallingTransport>();
    transport->first_chunk = "data: " + delta_chunk(R"({"content":"Hel"})") + "\n\n";
    OpenAiCompletionsProvider provider(transport);
    CancelToken cancel;
    EventStream stream = provider.stream(ollama(), ask("q"), {}, cancel);
    vec<AssistantMessageEvent> ev;
    while (auto event = stream.next()) {
        if (s::holds_alternative<TextDeltaEvent>(*event)) cancel.cancel();
        ev.push_back(s::move(*event));
    }
    ASSERT_FALSE(ev.empty());
    ASSERT_TRUE(s::holds_alternative<ErrorEvent>(ev.back())) << kind_name(ev.back());
    const ErrorEvent& e = s::get<ErrorEvent>(ev.back());
    EXPECT_EQ(e.reason, StopReason::aborted);
    EXPECT_EQ(e.message.stop_reason, StopReason::aborted);
    EXPECT_EQ(text_of(e.message), "Hel");
}

TEST(OpenAiCompletions, StreamOutlivesProvider) {
    auto transport = s::make_shared<h::FakeTransport>();
    transport->enqueue(h::FakeResponse::byte_by_byte(read_fixture(kFixture)));
    CancelToken cancel;
    EventStream stream = [&] {
        OpenAiCompletionsProvider provider(transport);
        return provider.stream(ollama(), ask("q"), {}, cancel);
    }();
    EXPECT_EQ(text_of(stream.result()), kFixtureText);
}

TEST(OpenAiCompletions, IdAndModels) {
    Harness t;
    EXPECT_EQ(t.provider.id(), "ollama");
    EXPECT_TRUE(t.provider.models().empty());
}
