#include <ai/event_stream.h>

#include <deque>
#include <mutex>
#include <condition_variable>

namespace pi::ai {


struct EventStream::State {
   s::mutex                        mutex;
   s::condition_variable           cv; // notified on every push
   s::deque<AssistantMessageEvent> queue;
   bool                            terminal_pushed = false;
   bool                            terminal_taken  = false; // next() returned it
   opt<AssistantMessage>           final_message; // from the terminal event
   s::jthread                      worker;

   ~State(){
     if (worker.get_id() == s::this_thread::get_id())
       worker.detach();
   }
};

EventStream::EventStream()
  : mState(s::make_shared<State>()) {}

void EventStream::push(AssistantMessageEvent event) {
  {
    s::lock_guard<s::mutex> lock((*mState).mutex);
    if ((*mState).terminal_pushed) return;
    if (is_terminal(event)) {
      (*mState).terminal_pushed = true;
      if (s::holds_alternative<DoneEvent>(event))
        (*mState).final_message = s::get<DoneEvent>(event).message;
      else
        (*mState).final_message = s::get<ErrorEvent>(event).message;
    }
    (*mState).queue.emplace_back(s::move(event));
  }
  (*mState).cv.notify_all();
}

void EventStream::adopt_worker(s::jthread worker) {
  (*mState).worker = s::move(worker);
}

opt<AssistantMessageEvent> EventStream::next() {
  s::unique_lock<s::mutex> lock((*mState).mutex);
  (*mState).cv.wait(lock, [this]() { return not (*mState).queue.empty() or (*mState).terminal_taken; });

  if ((*mState).terminal_taken) return s::nullopt;

  AssistantMessageEvent e = s::move((*mState).queue.front());
  (*mState).queue.pop_front();
  if (is_terminal(e))
    (*mState).terminal_taken = true;
  return e;
}

AssistantMessage EventStream::result() const {
  s::unique_lock<s::mutex> lock((*mState).mutex);
  (*mState).cv.wait(lock, [this]() { return (*mState).terminal_pushed; });
  return *(*mState).final_message;
}

bool EventStream::finished() const {
  s::lock_guard<s::mutex> guard((*mState).mutex);
  return (*mState).terminal_pushed;
}

}  // namespace pi::ai
