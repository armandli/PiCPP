#ifndef AI_EVENT_STREAM_H
#define AI_EVENT_STREAM_H

#include <memory>
#include <thread>

#include <ai/events.h>
#include <ai/types.h>
#include <util/aliases.h>

namespace pi::ai {

// Thread-safe, unbounded queue of AssistantMessageEvents from one producer (a
// provider's worker thread) to one consumer (Pi: utils/event-stream.ts,
// PiG: ai/event_stream.go).
//
// EventStream is a cheap handle: copies share one queue, so a provider returns
// one copy to the caller and hands another to its worker thread.
//
// Rules:
//  - push() never blocks; next() blocks until an event is available
//  - events come out of next() in push() order
//  - exactly one terminal event (DoneEvent / ErrorEvent) is delivered: pushes
//    after the first terminal event are dropped, including a second terminal
//  - after the terminal event has been returned, next() returns nullopt at once
//  - result() blocks until the terminal event has been *pushed* (it doesn't
//    need next() to have drained the queue) and returns its message; it can be
//    called any number of times, before, during or after draining with next()
//  - nothing here throws on the stream's normal paths: once a stream has been
//    handed out, failures are reported as an ErrorEvent, never as an exception
struct EventStream {
  EventStream();

  // Producer side.
  void push(AssistantMessageEvent event);

  // Keep `worker` alive as long as any copy of this stream exists; joined when
  // the last copy goes away.  Lets a provider return immediately while its
  // request runs in the background.  A worker that holds its own copy of the
  // stream keeps it alive until the worker finishes.
  void adopt_worker(s::jthread worker);

  // Consumer side.
  opt<AssistantMessageEvent> next();
  AssistantMessage           result() const;

  // True once a terminal event has been pushed.
  bool finished() const;

private:
  struct State;
  sptr<State> mState;
};

}  // namespace pi::ai

#endif  // AI_EVENT_STREAM_H
