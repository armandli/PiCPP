#ifndef AI_STREAM_BUILDER_H
#define AI_STREAM_BUILDER_H

#include <string>

#include <ai/event_stream.h>
#include <ai/events.h>
#include <ai/types.h>
#include <util/aliases.h>

namespace pi::ai {

// Turns provider-level "here is more text / thinking / tool-call JSON" calls
// into a well-formed event sequence on an EventStream, while accumulating the
// AssistantMessage (PiG: ai/stream_builder.go).  Every provider uses one, so the
// block bookkeeping lives in a single place.
//
// Behaviour:
//  - the constructor pushes nothing; start() pushes StartEvent (call it once,
//    before any delta)
//  - text_delta / thinking_delta append to the *current* block if it has the
//    same kind, otherwise close the current block (pushing its *_end) and open
//    a new one (pushing *_start) at index content().size()
//  - empty deltas are ignored entirely: no event, no block (Ollama sends
//    "content":"" next to every reasoning delta)
//  - toolcall_start always opens a new block; toolcall_delta appends arguments
//    JSON text to the current tool-call block (ignored if none is open);
//    closing a tool-call block parses the accumulated text with json::parse
//    (empty text → {}; unparsable → finish/fail decides, see below) and pushes
//    ToolCallEndEvent with the parsed ToolCall
//  - finish(reason) closes the open block, stamps stop_reason, pushes DoneEvent.
//    Precondition: reason is stop, length or toolUse; error / aborted are routed
//    to fail().  If a tool call's arguments don't parse, finish() becomes
//    fail(error, "invalid tool call arguments for <name>: ...")
//  - fail(reason, message) does NOT close the open block (no *_end); it stamps
//    stop_reason and error_message and pushes ErrorEvent.  reason must be error
//    or aborted (anything else is treated as error)
//  - after finish() or fail(), every call is a no-op
struct StreamBuilder {
  // `stream` receives the events; `initial` carries api / provider / model and
  // any other metadata (content must be empty).  The timestamp is set to now_ms()
  // if it is 0.
  StreamBuilder(EventStream stream, AssistantMessage initial);

  void start();
  void text_delta(strv delta);
  void thinking_delta(strv delta);
  void toolcall_start(str id, str name);
  void toolcall_delta(strv json_fragment);
  void set_usage(const Usage& usage);
  void finish(StopReason reason);
  void fail(StopReason reason, str message);

  // The message as built so far.
  const AssistantMessage& message() const;
  bool finished() const;

private:
  // TODO(M3): state.  Suggested:
  //   EventStream      mStream;
  //   AssistantMessage mMessage;
  //   enum class Open : uint8_t { None, Text, Thinking, ToolCall } mOpen = Open::None;
  //   str              mToolArgs;      // raw JSON of the open tool call
  //   bool             mStarted = false, mFinished = false;
  EventStream      mStream;
  AssistantMessage mMessage;
};

}  // namespace pi::ai

#endif  // AI_STREAM_BUILDER_H
