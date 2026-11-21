#include <ai/transcript.h>

#include <string>

#include <gtest/gtest.h>

#include <json/parse.h>
#include <util/aliases.h>

// TODO(M3): every test here fails until the stubs in ai/transcript.cpp are
// implemented.  Expected values follow Pi v1.5.0's ai/src/utils/transcript.ts.

using namespace pi::ai;
namespace j = pi::json;

namespace {
Tool tool(str name, str description = "") {
    return Tool{s::move(name), s::move(description), j::object_value()};
}

Message user(str text, int64_t ts = 1) {
    return UserMessage{{TextContent{s::move(text)}}, ts};
}

SystemMessage sys(str content, SystemSections sections = {}, vec<Tool> added = {},
                  vec<ToolReference> removed = {}, int64_t ts = 0) {
    return SystemMessage{s::move(content), s::move(sections), s::move(added), s::move(removed), ts};
}

vec<str> names(const vec<Tool>& tools) {
    vec<str> out;
    for (const Tool& t : tools) out.push_back(t.name);
    return out;
}
}  // namespace

// ===========================================================================
// create_initial_system_message / normalize_context
// ===========================================================================

TEST(CreateInitialSystemMessage, EmptyPromptAndToolsIsNothing) {
    EXPECT_EQ(create_initial_system_message("", {}), s::nullopt);
}

TEST(CreateInitialSystemMessage, PromptOnly) {
    EXPECT_EQ(create_initial_system_message("be brief", {}), sys("be brief"));
}

TEST(CreateInitialSystemMessage, ToolsOnly) {
    EXPECT_EQ(create_initial_system_message("", {tool("read")}), sys("", {}, {tool("read")}));
}

TEST(NormalizeContext, PrependsTheInitialSystemMessage) {
    Context c;
    c.system_prompt = "be brief";
    c.tools = {tool("read")};
    c.messages = {user("hi")};
    TranscriptContext t = normalize_context(c);
    EXPECT_EQ(t.messages, (vec<Message>{sys("be brief", {}, {tool("read")}), user("hi")}));
}

TEST(NormalizeContext, NoPromptNoToolsLeavesMessagesAlone) {
    Context c;
    c.messages = {user("hi")};
    EXPECT_EQ(normalize_context(c).messages, c.messages);
}

TEST(GetInitialSystemMessage, OnlyALeadingOne) {
    vec<Message> leading = {sys("a"), user("hi")};
    vec<Message> later   = {user("hi"), sys("a")};
    ASSERT_NE(get_initial_system_message(leading), nullptr);
    EXPECT_EQ(get_initial_system_message(leading)->content, "a");
    EXPECT_EQ(get_initial_system_message(later), nullptr);
    EXPECT_EQ(get_initial_system_message({}), nullptr);
}

// ===========================================================================
// Replay
// ===========================================================================

TEST(GetCurrentTools, AddsRemovesAndReplacesByName) {
    vec<Message> m = {
        sys("", {}, {tool("read"), tool("bash")}),
        user("hi"),
        sys("", {}, {tool("read", "v2")}, {ToolReference{"bash"}}),
    };
    vec<Tool> tools = get_current_tools(m);
    EXPECT_EQ(names(tools), (vec<str>{"read"}));
    ASSERT_EQ(tools.size(), 1u);
    EXPECT_EQ(tools[0].description, "v2");
}

TEST(GetCurrentTools, ReaddedToolMovesToTheEnd) {
    vec<Message> m = {
        sys("", {}, {tool("a"), tool("b")}),
        sys("", {}, {}, {ToolReference{"a"}}),
        sys("", {}, {tool("a")}),
    };
    EXPECT_EQ(names(get_current_tools(m)), (vec<str>{"b", "a"}));
}

TEST(GetCurrentTools, ReplacedToolKeepsItsPosition) {
    vec<Message> m = {sys("", {}, {tool("a"), tool("b")}), sys("", {}, {tool("a", "v2")})};
    EXPECT_EQ(names(get_current_tools(m)), (vec<str>{"a", "b"}));
}

TEST(GetCurrentSystemMessage, NoSystemMessageIsNothing) {
    EXPECT_EQ(get_current_system_message({user("hi")}), s::nullopt);
}

TEST(GetCurrentSystemMessage, ReplaysContentSectionsAndTools) {
    vec<Message> m = {
        sys("base", {{"rules", str("<rules>a</rules>")}, {"cwd", str("<cwd>/x</cwd>")}}, {tool("read")}, {}, 7),
        user("hi", 8),
        sys("", {{"rules", str("<rules>b</rules>")}}, {}, {}, 9),
        sys("more", {{"cwd", s::nullopt}}, {}, {}, 10),
    };
    EXPECT_EQ(get_current_system_message(m),
              sys("base\n\nmore", {{"rules", str("<rules>b</rules>")}}, {tool("read")}, {}, 7));
}

TEST(SystemMessageText, ContentThenSectionsSkippingEmptyAndRemoved) {
    SystemMessage m = sys("base", {{"a", str("A")}, {"gone", s::nullopt}, {"empty", str("")}, {"b", str("B")}});
    EXPECT_EQ(system_message_text(m), "base\n\nA\n\nB");
    EXPECT_EQ(system_message_text(sys("", {{"a", str("A")}})), "A");
}

TEST(GetCurrentSystemPrompt, MatchesPiReadmeExample) {
    // pi:ai/README.md, "Context Serialization".
    vec<Message> m = {
        sys("You are helpful.", {{"rules", str("<rules>Be brief.</rules>")}}, {tool("read")}, {}, 1),
        user("hi", 2),
        sys("", {{"rules", str("<rules>Be thorough.</rules>")}}, {}, {ToolReference{"read"}}, 3),
    };
    EXPECT_EQ(get_current_system_prompt(m), "You are helpful.\n\n<rules>Be thorough.</rules>");
    EXPECT_TRUE(get_current_tools(m).empty());
}

TEST(GetCurrentSystemPrompt, EmptyWithoutSystemMessages) {
    EXPECT_EQ(get_current_system_prompt({user("hi")}), "");
}

TEST(RenderSystemMessageUpdate, FramesSectionChangesByName) {
    SystemMessage m = sys("also terse", {{"rules", str("<rules>b</rules>")}, {"cwd", s::nullopt}});
    EXPECT_EQ(render_system_message_update(m),
              "also terse\n\n"
              "Updated system prompt section \"rules\":\n\n<rules>b</rules>\n\n"
              "Removed system prompt section \"cwd\".");
}

// ===========================================================================
// collapse_system_messages / resolve_transcript
// ===========================================================================

TEST(CollapseSystemMessages, OneLeadingMessageWithTheReplayedState) {
    TranscriptContext t{{sys("base", {}, {}, {}, 1), user("q1", 2), sys("more", {}, {}, {}, 3), user("q2", 4)}};
    TranscriptContext c = collapse_system_messages(t);
    EXPECT_EQ(c.messages, (vec<Message>{sys("base\n\nmore", {}, {}, {}, 1), user("q1", 2), user("q2", 4)}));
}

TEST(CollapseSystemMessages, NoSystemMessagesIsUnchanged) {
    TranscriptContext t{{user("q1"), user("q2")}};
    EXPECT_EQ(collapse_system_messages(t), t);
}

TEST(ResolveTranscript, KeepsSystemMessagesInPlaceOnlyWhenSupported) {
    TranscriptContext t{{sys("base"), user("q1"), sys("more"), user("q2")}};
    EXPECT_EQ(resolve_transcript(t, true), t);
    EXPECT_EQ(resolve_transcript(t, false), collapse_system_messages(t));
    EXPECT_EQ(resolve_transcript(t, false).messages.size(), 3u);
}
