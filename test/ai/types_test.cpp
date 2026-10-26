#include <ai/types.h>

#include <string>

#include <gtest/gtest.h>

#include <json/parse.h>
#include <json/write.h>
#include <util/aliases.h>

using namespace pi::ai;
namespace j = pi::json;

namespace {
j::Value parse_or_die(strv text) {
    auto v = j::parse(text);
    EXPECT_TRUE(v.has_value()) << "bad test JSON: " << text;
    return v ? *v : j::Value{};
}

AssistantMessage sample_assistant() {
    AssistantMessage m;
    m.content = {ThinkingContent{"hmm"}, TextContent{"Hello"}, ToolCall{"call_1", "read", parse_or_die(R"({"path":"a.txt"})")}};
    m.api = "openai-completions";
    m.provider = "ollama";
    m.model = "qwen3.8:27b-mlx";
    m.usage = Usage{.input = 10, .output = 20, .cache_read = 5, .cache_write = 0, .total_tokens = 35, .cost = {}};
    m.stop_reason = StopReason::toolUse;
    m.timestamp = 1700000000123;
    return m;
}
}  // namespace

// ===========================================================================
// Helpers
// ===========================================================================

TEST(StopReason, ToStringUsesPiNames) {
    EXPECT_EQ(to_string(StopReason::stop), "stop");
    EXPECT_EQ(to_string(StopReason::length), "length");
    EXPECT_EQ(to_string(StopReason::toolUse), "toolUse");
    EXPECT_EQ(to_string(StopReason::error), "error");
    EXPECT_EQ(to_string(StopReason::aborted), "aborted");
}

TEST(StopReason, ParseRoundTrips) {
    for (StopReason r : {StopReason::stop, StopReason::length, StopReason::toolUse, StopReason::error, StopReason::aborted})
        EXPECT_EQ(parse_stop_reason(to_string(r)), r) << to_string(r);
}

TEST(StopReason, ParseRejectsUnknownAndWrongCase) {
    EXPECT_EQ(parse_stop_reason("Stop"), s::nullopt);
    EXPECT_EQ(parse_stop_reason("tool_use"), s::nullopt);
    EXPECT_EQ(parse_stop_reason(""), s::nullopt);
}

TEST(OllamaCompat, Flags) {
    OpenAiCompat c = ollama_compat();
    EXPECT_FALSE(c.supports_developer_role);
    EXPECT_FALSE(c.supports_reasoning_effort);
    EXPECT_TRUE(c.supports_usage_in_streaming);
    EXPECT_EQ(c.max_tokens_field, "max_tokens");
}

TEST(OllamaCompat, DiffersFromOpenAiDefaults) {
    EXPECT_NE(ollama_compat(), OpenAiCompat{});
}

TEST(TextOf, ConcatenatesTextBlocksOnly) {
    AssistantMessage m;
    m.content = {TextContent{"a"}, ThinkingContent{"skip"}, TextContent{"b"}, ToolCall{"1", "x", j::object_value()}};
    EXPECT_EQ(text_of(m), "ab");
}

TEST(TextOf, EmptyMessage) {
    EXPECT_EQ(text_of(AssistantMessage{}), "");
}

TEST(NowMs, IsWallClockMilliseconds) {
    int64_t t = now_ms();
    EXPECT_GT(t, 1'700'000'000'000);  // after Nov 2023
    EXPECT_LT(t, 4'100'000'000'000);  // before 2100
}

// ===========================================================================
// encode: exact JSON (key order is part of the contract)
// ===========================================================================

TEST(Encode, TextContent) {
    EXPECT_EQ(j::to_json(encode(AssistantContent{TextContent{"hi"}})), R"({"type":"text","text":"hi"})");
    EXPECT_EQ(j::to_json(encode(UserContent{TextContent{"hi"}})), R"({"type":"text","text":"hi"})");
}

TEST(Encode, ThinkingContent) {
    EXPECT_EQ(j::to_json(encode(AssistantContent{ThinkingContent{"hmm"}})), R"({"type":"thinking","thinking":"hmm"})");
}

TEST(Encode, ImageContent) {
    EXPECT_EQ(j::to_json(encode(UserContent{ImageContent{"AAAA", "image/png"}})),
              R"({"type":"image","data":"AAAA","mimeType":"image/png"})");
}

TEST(Encode, ToolCall) {
    EXPECT_EQ(j::to_json(encode(AssistantContent{ToolCall{"call_1", "read", parse_or_die(R"({"path":"a.txt"})")}})),
              R"({"type":"toolCall","id":"call_1","name":"read","arguments":{"path":"a.txt"}})");
}

TEST(Encode, Usage) {
    Usage u{.input = 1, .output = 2, .cache_read = 3, .cache_write = 4, .total_tokens = 10, .cost = {}};
    EXPECT_EQ(j::to_json(encode(u)),
              R"({"input":1,"output":2,"cacheRead":3,"cacheWrite":4,"totalTokens":10,)"
              R"("cost":{"input":0.0,"output":0.0,"cacheRead":0.0,"cacheWrite":0.0,"total":0.0}})");
}

TEST(Encode, UserMessage) {
    Message m = UserMessage{{TextContent{"hi"}}, 5};
    EXPECT_EQ(j::to_json(encode(m)), R"({"role":"user","content":[{"type":"text","text":"hi"}],"timestamp":5})");
}

TEST(Encode, AssistantMessageOmitsEmptyErrorMessage) {
    AssistantMessage a;
    a.content = {TextContent{"ok"}};
    a.api = "faux";
    a.provider = "faux";
    a.model = "faux";
    a.timestamp = 7;
    EXPECT_EQ(j::to_json(encode(Message{a})),
              R"({"role":"assistant","content":[{"type":"text","text":"ok"}],"api":"faux","provider":"faux","model":"faux",)"
              R"("usage":{"input":0,"output":0,"cacheRead":0,"cacheWrite":0,"totalTokens":0,)"
              R"("cost":{"input":0.0,"output":0.0,"cacheRead":0.0,"cacheWrite":0.0,"total":0.0}},)"
              R"("stopReason":"stop","timestamp":7})");
}

TEST(Encode, AssistantMessageWithErrorMessage) {
    AssistantMessage a;
    a.stop_reason = StopReason::error;
    a.error_message = "boom";
    j::Value v = encode(Message{a});
    ASSERT_TRUE(v.is_object());
    ASSERT_NE(v.find("errorMessage"), nullptr);
    EXPECT_EQ(v.find("errorMessage")->as_string(), "boom");
    EXPECT_EQ(v.find("stopReason")->as_string(), "error");
}

TEST(Encode, ToolResultMessage) {
    Message m = ToolResultMessage{"call_1", "read", {TextContent{"file body"}}, true, 9};
    EXPECT_EQ(j::to_json(encode(m)),
              R"({"role":"toolResult","toolCallId":"call_1","toolName":"read",)"
              R"("content":[{"type":"text","text":"file body"}],"isError":true,"timestamp":9})");
}

TEST(Encode, ContextOmitsEmptySystemPromptAndTools) {
    Context c;
    c.messages = {UserMessage{{TextContent{"q"}}, 1}};
    EXPECT_EQ(j::to_json(encode(c)),
              R"({"messages":[{"role":"user","content":[{"type":"text","text":"q"}],"timestamp":1}]})");
}

TEST(Encode, ContextWithSystemPromptAndTools) {
    Context c;
    c.system_prompt = "be brief";
    c.tools = {Tool{"read", "Read a file", parse_or_die(R"({"type":"object"})")}};
    EXPECT_EQ(j::to_json(encode(c)),
              R"({"systemPrompt":"be brief","messages":[],)"
              R"("tools":[{"name":"read","description":"Read a file","parameters":{"type":"object"}}]})");
}

TEST(Encode, Model) {
    Model m;
    m.id = "qwen";
    m.name = "Qwen";
    m.api = "openai-completions";
    m.provider = "ollama";
    m.base_url = "http://localhost:11434/v1";
    m.reasoning = true;
    m.context_window = 32768;
    m.max_tokens = 8192;
    m.compat = ollama_compat();
    EXPECT_EQ(j::to_json(encode(m)),
              R"({"id":"qwen","name":"Qwen","api":"openai-completions","provider":"ollama",)"
              R"("baseUrl":"http://localhost:11434/v1","reasoning":true,"contextWindow":32768,"maxTokens":8192,)"
              R"("compat":{"supportsDeveloperRole":false,"supportsReasoningEffort":false,)"
              R"("supportsUsageInStreaming":true,"maxTokensField":"max_tokens"}})");
}

// ===========================================================================
// decode: round trips
// ===========================================================================

TEST(Decode, UserMessageRoundTrip) {
    Message m = UserMessage{{TextContent{"hi"}, ImageContent{"AAAA", "image/jpeg"}}, 42};
    auto back = decode_message(encode(m));
    ASSERT_TRUE(back.has_value()) << back.error();
    EXPECT_EQ(*back, m);
}

TEST(Decode, AssistantMessageRoundTrip) {
    Message m = sample_assistant();
    auto back = decode_message(encode(m));
    ASSERT_TRUE(back.has_value()) << back.error();
    EXPECT_EQ(*back, m);
}

TEST(Decode, AssistantErrorMessageRoundTrip) {
    AssistantMessage a = sample_assistant();
    a.stop_reason = StopReason::aborted;
    a.error_message = "request aborted";
    auto back = decode_message(encode(Message{a}));
    ASSERT_TRUE(back.has_value()) << back.error();
    EXPECT_EQ(*back, Message{a});
}

TEST(Decode, ToolResultRoundTrip) {
    Message m = ToolResultMessage{"call_1", "read", {TextContent{"x"}}, false, 3};
    auto back = decode_message(encode(m));
    ASSERT_TRUE(back.has_value()) << back.error();
    EXPECT_EQ(*back, m);
}

TEST(Decode, ModelRoundTrip) {
    Model m;
    m.id = "qwen";
    m.name = "Qwen";
    m.api = "openai-completions";
    m.provider = "ollama";
    m.base_url = "http://x/v1";
    m.reasoning = true;
    m.context_window = 1000;
    m.max_tokens = 100;
    m.compat = ollama_compat();
    auto back = decode_model(encode(m));
    ASSERT_TRUE(back.has_value()) << back.error();
    EXPECT_EQ(*back, m);
}

TEST(Decode, ContextRoundTrip) {
    Context c;
    c.system_prompt = "sys";
    c.messages = {UserMessage{{TextContent{"q"}}, 1}, sample_assistant(), ToolResultMessage{"call_1", "read", {TextContent{"r"}}, false, 2}};
    c.tools = {Tool{"read", "Read a file", parse_or_die(R"({"type":"object","properties":{"path":{"type":"string"}}})")}};
    auto back = decode_context(encode(c));
    ASSERT_TRUE(back.has_value()) << back.error();
    EXPECT_EQ(*back, c);
}

TEST(Decode, UsageRoundTrip) {
    Usage u{.input = 1, .output = 2, .cache_read = 3, .cache_write = 4, .total_tokens = 10,
            .cost = {.input = 0.5, .output = 1.25, .cache_read = 0.0, .cache_write = 0.0, .total = 1.75}};
    auto back = decode_usage(encode(u));
    ASSERT_TRUE(back.has_value()) << back.error();
    EXPECT_EQ(*back, u);
}

// ===========================================================================
// decode: Pi compatibility and leniency
// ===========================================================================

TEST(Decode, UserContentMayBeAString) {
    auto m = decode_message(parse_or_die(R"({"role":"user","content":"hello","timestamp":1})"));
    ASSERT_TRUE(m.has_value()) << m.error();
    EXPECT_EQ(*m, (Message{UserMessage{{TextContent{"hello"}}, 1}}));
}

TEST(Decode, ToolResultContentMayBeAString) {
    auto m = decode_message(parse_or_die(R"({"role":"toolResult","toolCallId":"c","toolName":"t","content":"out","isError":false,"timestamp":1})"));
    ASSERT_TRUE(m.has_value()) << m.error();
    EXPECT_EQ(*m, (Message{ToolResultMessage{"c", "t", {TextContent{"out"}}, false, 1}}));
}

TEST(Decode, UnknownFieldsAreIgnored) {
    auto m = decode_message(parse_or_die(R"({"role":"user","content":"x","timestamp":1,"extra":{"a":1}})"));
    ASSERT_TRUE(m.has_value()) << m.error();
}

TEST(Decode, MissingOptionalFieldsKeepDefaults) {
    auto m = decode_message(parse_or_die(R"({"role":"assistant","content":[]})"));
    ASSERT_TRUE(m.has_value()) << m.error();
    EXPECT_EQ(*m, Message{AssistantMessage{}});
}

TEST(Decode, IntegralDoubleAcceptedAsInteger) {
    auto u = decode_usage(parse_or_die(R"({"input":12.0,"output":3})"));
    ASSERT_TRUE(u.has_value()) << u.error();
    EXPECT_EQ(u->input, 12);
    EXPECT_EQ(u->output, 3);
}

TEST(Decode, ToolCallWithoutArgumentsIsEmptyObject) {
    auto c = decode_assistant_content(parse_or_die(R"({"type":"toolCall","id":"1","name":"ls"})"));
    ASSERT_TRUE(c.has_value()) << c.error();
    EXPECT_EQ(*c, (AssistantContent{ToolCall{"1", "ls", j::object_value()}}));
}

TEST(Decode, ToolWithoutParametersIsEmptyObject) {
    auto t = decode_tool(parse_or_die(R"({"name":"ls"})"));
    ASSERT_TRUE(t.has_value()) << t.error();
    EXPECT_EQ(*t, (Tool{"ls", "", j::object_value()}));
}

// ===========================================================================
// decode: errors
// ===========================================================================

TEST(DecodeError, NotAnObject) {
    EXPECT_FALSE(decode_message(parse_or_die("[1,2]")).has_value());
    EXPECT_FALSE(decode_usage(parse_or_die("3")).has_value());
}

TEST(DecodeError, UnknownRoleIsNamed) {
    auto m = decode_message(parse_or_die(R"({"role":"bot","content":[]})"));
    ASSERT_FALSE(m.has_value());
    EXPECT_NE(m.error().find("bot"), str::npos) << m.error();
}

TEST(DecodeError, MissingRole) {
    EXPECT_FALSE(decode_message(parse_or_die(R"({"content":[]})")).has_value());
}

TEST(DecodeError, UnknownContentType) {
    EXPECT_FALSE(decode_assistant_content(parse_or_die(R"({"type":"video"})")).has_value());
    // image is user-only
    EXPECT_FALSE(decode_assistant_content(parse_or_die(R"({"type":"image","data":"A","mimeType":"image/png"})")).has_value());
    // thinking / toolCall are assistant-only
    EXPECT_FALSE(decode_user_content(parse_or_die(R"({"type":"thinking","thinking":"x"})")).has_value());
}

TEST(DecodeError, MissingRequiredText) {
    EXPECT_FALSE(decode_user_content(parse_or_die(R"({"type":"text"})")).has_value());
}

TEST(DecodeError, BadStopReason) {
    auto m = decode_message(parse_or_die(R"({"role":"assistant","content":[],"stopReason":"tool_use"})"));
    ASSERT_FALSE(m.has_value());
    EXPECT_NE(m.error().find("stopReason"), str::npos) << m.error();
}

TEST(DecodeError, WrongFieldTypeIsNamed) {
    auto u = decode_usage(parse_or_die(R"({"input":"ten"})"));
    ASSERT_FALSE(u.has_value());
    EXPECT_NE(u.error().find("input"), str::npos) << u.error();
}

TEST(DecodeError, NonIntegralDoubleRejected) {
    EXPECT_FALSE(decode_usage(parse_or_die(R"({"input":1.5})")).has_value());
}

TEST(DecodeError, ContextReportsMessageIndex) {
    auto c = decode_context(parse_or_die(R"({"messages":[{"role":"user","content":"ok"},{"role":"bot"}]})"));
    ASSERT_FALSE(c.has_value());
    EXPECT_NE(c.error().find("messages[1]"), str::npos) << c.error();
}

TEST(DecodeError, ModelRequiresId) {
    EXPECT_FALSE(decode_model(parse_or_die(R"({"name":"x"})")).has_value());
}
