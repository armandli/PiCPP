#ifndef AI_PROVIDER_H
#define AI_PROVIDER_H

#include <cstdint>
#include <string>
#include <vector>

#include <ai/event_stream.h>
#include <ai/types.h>
#include <util/aliases.h>
#include <util/cancel.h>

namespace pi::ai {

// Per-request knobs (Pi: StreamOptions).  Unset → the provider / model default.
struct StreamOptions {
  opt<int64_t> max_tokens;    // else Model::max_tokens (if > 0)
  opt<double>  temperature;
  str          api_key;       // "Authorization: Bearer <key>" when non-empty
};

// One LLM backend (Pi: the per-API stream functions + provider registry entry).
//
// Contract for stream():
//  - returns immediately; the request runs on a worker thread kept alive by
//    EventStream::adopt_worker (a provider with nothing to wait on, like the
//    faux one, may push everything before returning)
//  - never throws and never returns an "empty" stream: every failure, including
//    ones detected before any I/O (bad model, missing response, cancelled
//    already) becomes the stream's ErrorEvent
//  - `cancel` is a copy that shares state with the caller's token; once it is
//    cancelled the stream ends with ErrorEvent{aborted} as soon as possible
//  - exactly one terminal event, per EventStream's rules
struct Provider {
  virtual ~Provider() = default;

  // Pi's provider id, e.g. "ollama", "faux".
  virtual strv id() const = 0;

  // The models this provider can serve (static for now; M7 lists Ollama's).
  virtual vec<Model> models() = 0;

  virtual EventStream stream(
      const Model& model,
      const Context& context,
      const StreamOptions& options,
      util::CancelToken cancel) = 0;
};

}  // namespace pi::ai

#endif  // AI_PROVIDER_H
