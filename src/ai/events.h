#ifndef AI_EVENTS_H
#define AI_EVENTS_H

#include <cstddef>
#include <string>
#include <variant>

#include <ai/types.h>
#include <util/aliases.h>

// Streaming events for one assistant message (Pi: AssistantMessageEvent in
// ai/src/types.ts).  A well-formed stream is:
//
//   start
//   ( text_start text_delta* text_end
//   | thinking_start thinking_delta* thinking_end
//   | toolcall_start toolcall_delta* toolcall_end )*
//   ( done | error )
//
// `content_index` is the index of the block in AssistantMessage::content that
// the event belongs to, so a consumer can rebuild the message incrementally.
// Unlike Pi, events don't carry a copy of the partial message (that would copy
// the whole message on every delta); use StreamBuilder::message() or the
// terminal event's message instead.
//
// On `error` the open block (if any) gets no *_end event.

namespace pi::ai {

struct StartEvent {
  bool operator==(const StartEvent&) const = default;
};

struct TextStartEvent {
  s::size_t content_index = 0;
  bool operator==(const TextStartEvent&) const = default;
};
struct TextDeltaEvent {
  s::size_t content_index = 0;
  str       delta;
  bool operator==(const TextDeltaEvent&) const = default;
};
struct TextEndEvent {
  s::size_t content_index = 0;
  str       content;  // the block's full text
  bool operator==(const TextEndEvent&) const = default;
};

struct ThinkingStartEvent {
  s::size_t content_index = 0;
  bool operator==(const ThinkingStartEvent&) const = default;
};
struct ThinkingDeltaEvent {
  s::size_t content_index = 0;
  str       delta;
  bool operator==(const ThinkingDeltaEvent&) const = default;
};
struct ThinkingEndEvent {
  s::size_t content_index = 0;
  str       content;  // the block's full thinking text
  bool operator==(const ThinkingEndEvent&) const = default;
};

struct ToolCallStartEvent {
  s::size_t content_index = 0;
  str       id;
  str       name;
  bool operator==(const ToolCallStartEvent&) const = default;
};
struct ToolCallDeltaEvent {
  s::size_t content_index = 0;
  str       delta;  // a fragment of the arguments JSON text
  bool operator==(const ToolCallDeltaEvent&) const = default;
};
struct ToolCallEndEvent {
  s::size_t content_index = 0;
  ToolCall  tool_call;  // arguments parsed
  bool operator==(const ToolCallEndEvent&) const = default;
};

// Terminal: the model finished.  reason is stop, length or toolUse.
struct DoneEvent {
  StopReason       reason = StopReason::stop;
  AssistantMessage message;
  bool operator==(const DoneEvent&) const = default;
};

// Terminal: the request failed or was cancelled.  reason is error or aborted;
// message.stop_reason == reason, message.error_message says why, and
// message.content holds whatever arrived before the failure.
struct ErrorEvent {
  StopReason       reason = StopReason::error;
  AssistantMessage message;
  bool operator==(const ErrorEvent&) const = default;
};

using AssistantMessageEvent = s::variant<
    StartEvent,
    TextStartEvent, TextDeltaEvent, TextEndEvent,
    ThinkingStartEvent, ThinkingDeltaEvent, ThinkingEndEvent,
    ToolCallStartEvent, ToolCallDeltaEvent, ToolCallEndEvent,
    DoneEvent, ErrorEvent>;

// True for DoneEvent and ErrorEvent.
bool is_terminal(const AssistantMessageEvent& event);

// Pi's event type tag: "start", "text_start", "text_delta", "text_end",
// "thinking_start", ..., "toolcall_start", "toolcall_delta", "toolcall_end",
// "done", "error".  Used by json mode (M14) and test failure messages.
strv event_type(const AssistantMessageEvent& event);

}  // namespace pi::ai

#endif  // AI_EVENTS_H
