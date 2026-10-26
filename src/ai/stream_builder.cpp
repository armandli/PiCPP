#include <ai/stream_builder.h>

namespace pi::ai {

StreamBuilder::StreamBuilder(EventStream stream, AssistantMessage initial)
  : mStream(s::move(stream))
  , mMessage(s::move(initial)) {
  // TODO(M3): if mMessage.timestamp == 0, set it to now_ms().
}

void StreamBuilder::start() {
  // TODO(M3): push StartEvent once; later calls are no-ops.
}

void StreamBuilder::text_delta(strv delta) {
  // TODO(M3): ignore empty; switch blocks if the open block isn't text (see
  //   header); append to the TextContent at the back of mMessage.content; push
  //   TextDeltaEvent{index, delta}.
  (void)delta;
}

void StreamBuilder::thinking_delta(strv delta) {
  // TODO(M3): as text_delta, with ThinkingContent / Thinking*Event.
  (void)delta;
}

void StreamBuilder::toolcall_start(str id, str name) {
  // TODO(M3): close the open block; push ToolCall{id, name, {}} to content;
  //   push ToolCallStartEvent{index, id, name}; clear the raw-arguments buffer.
  (void)id; (void)name;
}

void StreamBuilder::toolcall_delta(strv json_fragment) {
  // TODO(M3): only while a tool-call block is open: append to the raw buffer and
  //   push ToolCallDeltaEvent{index, fragment}.
  (void)json_fragment;
}

void StreamBuilder::set_usage(const Usage& usage) {
  // TODO(M3): overwrite mMessage.usage (providers report cumulative usage).
  (void)usage;
}

void StreamBuilder::finish(StopReason reason) {
  // TODO(M3): route error/aborted to fail(); close the open block (a tool-call
  //   parse failure turns this into fail(error, ...)); set stop_reason; push
  //   DoneEvent{reason, mMessage}; mark finished.
  (void)reason;
}

void StreamBuilder::fail(StopReason reason, str message) {
  // TODO(M3): no *_end for the open block; set stop_reason (error unless
  //   aborted) and error_message; push ErrorEvent{reason, mMessage}; mark finished.
  (void)reason; (void)message;
}

const AssistantMessage& StreamBuilder::message() const {
  return mMessage;
}

bool StreamBuilder::finished() const {
  // TODO(M3)
  return false;
}

}  // namespace pi::ai
