#include <ai/events.h>

#include <gtest/gtest.h>

#include <util/aliases.h>

using namespace pi::ai;

TEST(Events, OnlyDoneAndErrorAreTerminal) {
    EXPECT_TRUE(is_terminal(DoneEvent{}));
    EXPECT_TRUE(is_terminal(ErrorEvent{}));
    EXPECT_FALSE(is_terminal(StartEvent{}));
    EXPECT_FALSE(is_terminal(TextDeltaEvent{0, "x"}));
    EXPECT_FALSE(is_terminal(ThinkingEndEvent{0, "x"}));
    EXPECT_FALSE(is_terminal(ToolCallEndEvent{}));
}

TEST(Events, TypeTagsMatchPi) {
    EXPECT_EQ(event_type(StartEvent{}), "start");
    EXPECT_EQ(event_type(TextStartEvent{}), "text_start");
    EXPECT_EQ(event_type(TextDeltaEvent{}), "text_delta");
    EXPECT_EQ(event_type(TextEndEvent{}), "text_end");
    EXPECT_EQ(event_type(ThinkingStartEvent{}), "thinking_start");
    EXPECT_EQ(event_type(ThinkingDeltaEvent{}), "thinking_delta");
    EXPECT_EQ(event_type(ThinkingEndEvent{}), "thinking_end");
    EXPECT_EQ(event_type(ToolCallStartEvent{}), "toolcall_start");
    EXPECT_EQ(event_type(ToolCallDeltaEvent{}), "toolcall_delta");
    EXPECT_EQ(event_type(ToolCallEndEvent{}), "toolcall_end");
    EXPECT_EQ(event_type(DoneEvent{}), "done");
    EXPECT_EQ(event_type(ErrorEvent{}), "error");
}
