#include <ai/events.h>

namespace pi::ai {

bool is_terminal(const AssistantMessageEvent& event) {
  // TODO(M3): s::holds_alternative<DoneEvent> or <ErrorEvent>.
  (void)event;
  return false;
}

strv event_type(const AssistantMessageEvent& event) {
  // TODO(M3): s::visit with an overloaded lambda set (or `if constexpr` on the
  //   decayed type) mapping each alternative to Pi's tag; see events.h.
  (void)event;
  return "";
}

}  // namespace pi::ai
