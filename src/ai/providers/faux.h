#ifndef AI_PROVIDERS_FAUX_H
#define AI_PROVIDERS_FAUX_H

#include <cstddef>
#include <deque>
#include <string>
#include <variant>
#include <vector>

#include <ai/provider.h>
#include <ai/types.h>
#include <json/value.h>
#include <util/aliases.h>

// Scriptable fake provider for deterministic tests (Pi: providers/faux.ts,
// PiG: ai/faux.go).  Every agent-loop, tool and session test from M4 on uses it.

namespace pi::ai::providers {

// One step of a scripted response.
struct FauxText {
  str text;
};
struct FauxThinking {
  str thinking;
};
struct FauxToolCall {
  str         id;
  str         name;
  json::Value arguments = json::object_value();
};
using FauxStep = s::variant<FauxText, FauxThinking, FauxToolCall>;

// The scripted answer to one stream() call.
struct FauxResponse {
  vec<FauxStep> steps;
  // Stop reason for DoneEvent.  When unset: toolUse if any step is a tool call,
  // else stop.
  opt<StopReason> stop_reason;
  Usage           usage;
  // If set, the stream ends with ErrorEvent{error, *error} after the steps.
  opt<str>        error;

  static FauxResponse text(str text);
};

struct FauxOptions {
  // Text / thinking / tool-argument JSON is delivered in deltas of at most this
  // many bytes, never splitting a UTF-8 sequence (a delta may run longer to
  // finish one).  0 = each step in one delta.
  s::size_t chunk_size = 0;
  str       provider_id = "faux";
};

// Replays queued FauxResponses, one per stream() call, in FIFO order.
//
// stream() is synchronous: it pushes the whole event sequence (through a
// StreamBuilder) before returning, so tests need no threads.
//  - StartEvent, then one block per step: text → text_*, thinking → thinking_*,
//    tool call → toolcall_start, toolcall_delta(s) of json::to_json(arguments),
//    toolcall_end
//  - message: api "faux", provider = options.provider_id, model = model.id,
//    usage = response.usage
//  - no response queued → ErrorEvent{error, "no faux response queued"}
//  - `cancel` is checked before every delta; once cancelled →
//    ErrorEvent{aborted, "request aborted"} (content so far is kept)
//  - the Context of every call is recorded (also for the error cases)
struct FauxProvider : Provider {
  explicit FauxProvider(FauxOptions options = {});

  void enqueue(FauxResponse response);
  s::size_t pending() const;
  const vec<Context>& calls() const;

  strv id() const override;
  // One model: {id "faux", name "Faux", api "faux", provider = provider_id,
  // context_window 128000, max_tokens 4096}.
  vec<Model> models() override;
  EventStream stream(
      const Model& model,
      const Context& context,
      const StreamOptions& options,
      util::CancelToken cancel) override;

private:
  FauxOptions              mOptions;
  s::deque<FauxResponse>   mResponses;
  vec<Context>             mCalls;
};

}  // namespace pi::ai::providers

#endif  // AI_PROVIDERS_FAUX_H
