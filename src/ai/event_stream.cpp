#include <ai/event_stream.h>

namespace pi::ai {

// TODO(M3): shared state behind every copy of an EventStream.  Suggested members:
//   s::mutex                        mutex;
//   s::condition_variable           cv;          // notified on every push
//   s::deque<AssistantMessageEvent> queue;
//   bool                            terminal_pushed = false;
//   bool                            terminal_taken  = false;  // next() returned it
//   opt<AssistantMessage>           final_message;  // from the terminal event
//   s::jthread                      worker;
//
// Destructor pitfall: the worker thread usually holds a copy of the stream, so
// the last copy can die *on the worker itself*.  Joining your own thread throws
// (resource_deadlock_would_occur).  In ~State: if worker.get_id() equals
// st::get_id(), detach() instead of joining.
struct EventStream::State {
};

EventStream::EventStream()
  : mState(s::make_shared<State>()) {}

void EventStream::push(AssistantMessageEvent event) {
  // TODO(M3): lock; drop the event if terminal_pushed; otherwise enqueue, and
  //   if it is terminal record final_message and set terminal_pushed.  Notify
  //   all waiters (both next() and result() may be blocked).
  (void)event;
}

void EventStream::adopt_worker(s::jthread worker) {
  // TODO(M3): store into the shared state (one worker per stream).
  (void)worker;
}

opt<AssistantMessageEvent> EventStream::next() {
  // TODO(M3): if terminal_taken → nullopt.  Otherwise wait on cv until the
  //   queue is non-empty, pop the front, set terminal_taken if it is terminal.
  return s::nullopt;
}

AssistantMessage EventStream::result() const {
  // TODO(M3): wait on cv until terminal_pushed; return *final_message.
  return {};
}

bool EventStream::finished() const {
  // TODO(M3): terminal_pushed, under the lock.
  return false;
}

}  // namespace pi::ai
