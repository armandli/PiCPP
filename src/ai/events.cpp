#include <ai/events.h>

namespace pi::ai {

bool is_terminal(const AssistantMessageEvent& event) {
  return s::holds_alternative<DoneEvent>(event) or
         s::holds_alternative<ErrorEvent>(event);
}

// TODO refactor to util and share
namespace {
template <typename... Ts> struct overloaded : Ts... { using Ts::operator()...; };
} // namespace

strv event_type(const AssistantMessageEvent& event) {
  return s::visit(overloaded {
    [](const StartEvent&) -> strv { return "start"; },
    [](const TextStartEvent&) -> strv { return "text_start"; },
    [](const TextDeltaEvent&) -> strv { return "text_delta"; },
    [](const TextEndEvent&) -> strv { return "text_end"; },
    [](const ThinkingStartEvent&) -> strv { return "thinking_start"; },
    [](const ThinkingDeltaEvent&) -> strv { return "thinking_delta"; },
    [](const ThinkingEndEvent&) -> strv { return "thinking_end"; },
    [](const ToolCallStartEvent&) -> strv { return "toolcall_start"; },
    [](const ToolCallDeltaEvent&) -> strv { return "toolcall_delta"; },
    [](const ToolCallEndEvent&) -> strv { return "toolcall_end"; },
    [](const DoneEvent&) -> strv { return "done"; },
    [](const ErrorEvent&) -> strv { return "error"; },
  }, event);
}

}  // namespace pi::ai
