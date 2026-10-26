#include <ai/stream_builder.h>

#include <string>

#include <gtest/gtest.h>

#include <ai/event_helpers.h>
#include <json/parse.h>
#include <util/aliases.h>

using namespace pi::ai;
using namespace pi::ai::test;
namespace j = pi::json;

namespace {
AssistantMessage meta() {
    AssistantMessage m;
    m.api = "test-api";
    m.provider = "test";
    m.model = "m1";
    m.timestamp = 1234;
    return m;
}

// Owns the stream and builder for one test.
struct Fixture {
    EventStream   stream;
    StreamBuilder builder{stream, meta()};

    vec<AssistantMessageEvent> events() { return drain(stream); }
};
}  // namespace

// ===========================================================================
// Block handling
// ===========================================================================

TEST(StreamBuilder, ConstructorPushesNothing) {
    Fixture f;
    EXPECT_FALSE(f.stream.finished());
    f.builder.finish(StopReason::stop);
    EXPECT_EQ(kinds(f.events()), (vec<str>{"done"}));
}

TEST(StreamBuilder, StartPushesStartOnce) {
    Fixture f;
    f.builder.start();
    f.builder.start();
    f.builder.finish(StopReason::stop);
    EXPECT_EQ(kinds(f.events()), (vec<str>{"start", "done"}));
}

TEST(StreamBuilder, TextDeltasShareOneBlock) {
    Fixture f;
    f.builder.start();
    f.builder.text_delta("Hel");
    f.builder.text_delta("lo");
    f.builder.finish(StopReason::stop);
    vec<AssistantMessageEvent> ev = f.events();
    ASSERT_EQ(ev.size(), 6u);
    EXPECT_EQ(ev[0], AssistantMessageEvent{StartEvent{}});
    EXPECT_EQ(ev[1], AssistantMessageEvent{TextStartEvent{0}});
    EXPECT_EQ(ev[2], (AssistantMessageEvent{TextDeltaEvent{0, "Hel"}}));
    EXPECT_EQ(ev[3], (AssistantMessageEvent{TextDeltaEvent{0, "lo"}}));
    EXPECT_EQ(ev[4], (AssistantMessageEvent{TextEndEvent{0, "Hello"}}));
    ASSERT_TRUE(s::holds_alternative<DoneEvent>(ev[5]));
    const DoneEvent& done = s::get<DoneEvent>(ev[5]);
    EXPECT_EQ(done.reason, StopReason::stop);
    EXPECT_EQ(done.message.content, (vec<AssistantContent>{TextContent{"Hello"}}));
    EXPECT_EQ(done.message.stop_reason, StopReason::stop);
}

TEST(StreamBuilder, ThinkingThenTextOpensSecondBlock) {
    Fixture f;
    f.builder.start();
    f.builder.thinking_delta("let me ");
    f.builder.thinking_delta("think");
    f.builder.text_delta("4");
    f.builder.finish(StopReason::stop);
    vec<AssistantMessageEvent> ev = f.events();
    EXPECT_EQ(kinds(ev), (vec<str>{"start", "thinking_start", "thinking_delta", "thinking_delta", "thinking_end",
                                   "text_start", "text_delta", "text_end", "done"}));
    ASSERT_EQ(ev.size(), 9u);
    EXPECT_EQ(ev[4], (AssistantMessageEvent{ThinkingEndEvent{0, "let me think"}}));
    EXPECT_EQ(ev[5], AssistantMessageEvent{TextStartEvent{1}});
    EXPECT_EQ(ev[7], (AssistantMessageEvent{TextEndEvent{1, "4"}}));
    EXPECT_EQ(f.builder.message().content, (vec<AssistantContent>{ThinkingContent{"let me think"}, TextContent{"4"}}));
}

TEST(StreamBuilder, SwitchingBackOpensAThirdBlock) {
    Fixture f;
    f.builder.start();
    f.builder.text_delta("a");
    f.builder.thinking_delta("b");
    f.builder.text_delta("c");
    f.builder.finish(StopReason::stop);
    EXPECT_EQ(f.builder.message().content,
              (vec<AssistantContent>{TextContent{"a"}, ThinkingContent{"b"}, TextContent{"c"}}));
}

TEST(StreamBuilder, EmptyDeltasAreIgnored) {
    // Ollama sends "content":"" next to every reasoning delta.
    Fixture f;
    f.builder.start();
    f.builder.thinking_delta("a");
    f.builder.text_delta("");
    f.builder.thinking_delta("b");
    f.builder.thinking_delta("");
    f.builder.finish(StopReason::stop);
    EXPECT_EQ(kinds(f.events()),
              (vec<str>{"start", "thinking_start", "thinking_delta", "thinking_delta", "thinking_end", "done"}));
    EXPECT_EQ(f.builder.message().content, (vec<AssistantContent>{ThinkingContent{"ab"}}));
}

TEST(StreamBuilder, NoDeltasMeansNoBlocks) {
    Fixture f;
    f.builder.start();
    f.builder.text_delta("");
    f.builder.finish(StopReason::length);
    EXPECT_EQ(kinds(f.events()), (vec<str>{"start", "done"}));
    EXPECT_TRUE(f.builder.message().content.empty());
    EXPECT_EQ(f.builder.message().stop_reason, StopReason::length);
}

// ===========================================================================
// Tool calls
// ===========================================================================

TEST(StreamBuilder, ToolCallArgumentsAreParsedAtEnd) {
    Fixture f;
    f.builder.start();
    f.builder.toolcall_start("call_1", "read");
    f.builder.toolcall_delta(R"({"pa)");
    f.builder.toolcall_delta(R"(th":"a.txt"})");
    f.builder.finish(StopReason::toolUse);
    vec<AssistantMessageEvent> ev = f.events();
    EXPECT_EQ(kinds(ev), (vec<str>{"start", "toolcall_start", "toolcall_delta", "toolcall_delta", "toolcall_end", "done"}));
    ASSERT_EQ(ev.size(), 6u);
    EXPECT_EQ(ev[1], (AssistantMessageEvent{(ToolCallStartEvent{0, "call_1", "read"})}));
    EXPECT_EQ(ev[2], (AssistantMessageEvent{(ToolCallDeltaEvent{0, R"({"pa)"})}));
    ToolCall expected{"call_1", "read", *j::parse(R"({"path":"a.txt"})")};
    EXPECT_EQ(ev[4], (AssistantMessageEvent{(ToolCallEndEvent{0, expected})}));
    EXPECT_EQ(f.builder.message().content, (vec<AssistantContent>{expected}));
    EXPECT_EQ(s::get<DoneEvent>(ev[5]).reason, StopReason::toolUse);
}

TEST(StreamBuilder, ToolCallWithNoArgumentsIsEmptyObject) {
    Fixture f;
    f.builder.start();
    f.builder.toolcall_start("c", "ls");
    f.builder.finish(StopReason::toolUse);
    EXPECT_EQ(f.builder.message().content, (vec<AssistantContent>{ToolCall{"c", "ls", j::object_value()}}));
}

TEST(StreamBuilder, ConsecutiveToolCallsAreSeparateBlocks) {
    Fixture f;
    f.builder.start();
    f.builder.toolcall_start("a", "ls");
    f.builder.toolcall_delta("{}");
    f.builder.toolcall_start("b", "ls");
    f.builder.toolcall_delta("{}");
    f.builder.finish(StopReason::toolUse);
    EXPECT_EQ(f.builder.message().content.size(), 2u);
    vec<AssistantMessageEvent> ev = f.events();
    ASSERT_GE(ev.size(), 5u);
    EXPECT_EQ(kind_name(ev[3]), "toolcall_end") << "first call must close before the second starts";
    EXPECT_EQ(ev[4], (AssistantMessageEvent{(ToolCallStartEvent{1, "b", "ls"})}));
}

TEST(StreamBuilder, ToolCallDeltaWithoutOpenCallIsIgnored) {
    Fixture f;
    f.builder.start();
    f.builder.toolcall_delta("{}");
    f.builder.text_delta("t");
    f.builder.toolcall_delta("{}");
    f.builder.finish(StopReason::stop);
    EXPECT_EQ(kinds(f.events()), (vec<str>{"start", "text_start", "text_delta", "text_end", "done"}));
}

TEST(StreamBuilder, InvalidToolArgumentsTurnFinishIntoError) {
    Fixture f;
    f.builder.start();
    f.builder.toolcall_start("c", "read");
    f.builder.toolcall_delta(R"({"path":)");
    f.builder.finish(StopReason::toolUse);
    vec<AssistantMessageEvent> ev = f.events();
    ASSERT_FALSE(ev.empty());
    ASSERT_TRUE(s::holds_alternative<ErrorEvent>(ev.back())) << kind_name(ev.back());
    const ErrorEvent& e = s::get<ErrorEvent>(ev.back());
    EXPECT_EQ(e.reason, StopReason::error);
    EXPECT_NE(e.message.error_message.find("invalid tool call arguments"), str::npos) << e.message.error_message;
    EXPECT_NE(e.message.error_message.find("read"), str::npos) << e.message.error_message;
}

// ===========================================================================
// Terminal handling
// ===========================================================================

TEST(StreamBuilder, FailKeepsContentAndSkipsEnd) {
    Fixture f;
    f.builder.start();
    f.builder.text_delta("par");
    f.builder.fail(StopReason::error, "boom");
    vec<AssistantMessageEvent> ev = f.events();
    EXPECT_EQ(kinds(ev), (vec<str>{"start", "text_start", "text_delta", "error"}));
    ASSERT_FALSE(ev.empty());
    ASSERT_TRUE(s::holds_alternative<ErrorEvent>(ev.back()));
    const ErrorEvent& e = s::get<ErrorEvent>(ev.back());
    EXPECT_EQ(e.reason, StopReason::error);
    EXPECT_EQ(e.message.stop_reason, StopReason::error);
    EXPECT_EQ(e.message.error_message, "boom");
    EXPECT_EQ(e.message.content, (vec<AssistantContent>{TextContent{"par"}}));
}

TEST(StreamBuilder, FailAborted) {
    Fixture f;
    f.builder.start();
    f.builder.fail(StopReason::aborted, "request aborted");
    AssistantMessage m = f.stream.result();
    EXPECT_EQ(m.stop_reason, StopReason::aborted);
    EXPECT_EQ(m.error_message, "request aborted");
}

TEST(StreamBuilder, FailWithNonErrorReasonBecomesError) {
    Fixture f;
    f.builder.fail(StopReason::stop, "odd");
    EXPECT_EQ(f.stream.result().stop_reason, StopReason::error);
}

TEST(StreamBuilder, FinishWithErrorReasonRoutesToFail) {
    Fixture f;
    f.builder.start();
    f.builder.finish(StopReason::error);
    vec<AssistantMessageEvent> ev = f.events();
    ASSERT_FALSE(ev.empty());
    EXPECT_EQ(kind_name(ev.back()), "error");
}

TEST(StreamBuilder, EverythingAfterTerminalIsANoop) {
    Fixture f;
    f.builder.start();
    f.builder.text_delta("a");
    f.builder.finish(StopReason::stop);
    EXPECT_TRUE(f.builder.finished());
    f.builder.text_delta("b");
    f.builder.thinking_delta("c");
    f.builder.toolcall_start("x", "y");
    f.builder.finish(StopReason::length);
    f.builder.fail(StopReason::error, "late");
    EXPECT_EQ(kinds(f.events()), (vec<str>{"start", "text_start", "text_delta", "text_end", "done"}));
    EXPECT_EQ(f.builder.message().content, (vec<AssistantContent>{TextContent{"a"}}));
    EXPECT_EQ(f.builder.message().stop_reason, StopReason::stop);
}

TEST(StreamBuilder, FinishedIsFalseUntilTerminal) {
    Fixture f;
    EXPECT_FALSE(f.builder.finished());
    f.builder.start();
    f.builder.text_delta("x");
    EXPECT_FALSE(f.builder.finished());
    f.builder.fail(StopReason::error, "e");
    EXPECT_TRUE(f.builder.finished());
}

// ===========================================================================
// Metadata
// ===========================================================================

TEST(StreamBuilder, UsageAndMetadataReachTheMessage) {
    Fixture f;
    Usage u{.input = 28, .output = 33, .cache_read = 0, .cache_write = 0, .total_tokens = 61, .cost = {}};
    f.builder.start();
    f.builder.set_usage(Usage{.input = 1, .output = 1, .cache_read = 0, .cache_write = 0, .total_tokens = 2, .cost = {}});
    f.builder.set_usage(u);  // later report wins
    f.builder.finish(StopReason::stop);
    AssistantMessage m = f.stream.result();
    EXPECT_EQ(m.usage, u);
    EXPECT_EQ(m.api, "test-api");
    EXPECT_EQ(m.provider, "test");
    EXPECT_EQ(m.model, "m1");
    EXPECT_EQ(m.timestamp, 1234);
}

TEST(StreamBuilder, ZeroTimestampIsStampedWithNow) {
    EventStream stream;
    StreamBuilder builder(stream, AssistantMessage{});
    EXPECT_GT(builder.message().timestamp, 1'700'000'000'000);
}
