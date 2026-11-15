#include <modes/print_mode.h>

#include <ai/events.h>
#include <util/language.h>

namespace pi::modes {

ai::Context make_print_context(strv prompt, strv system_prompt) {
  vec<ai::Message> msgs;
  msgs.push_back(ai::UserMessage{vec<ai::UserContent>{ai::TextContent{(str)prompt}}, ai::now_ms()});
  return ai::Context{(str)system_prompt, msgs, vec<ai::Tool>{}};
}

str format_usage(const ai::Usage& usage) {
  return s::format("tokens: {} in, {} out, {} cached, {} total", usage.input, usage.output, usage.cache_read, usage.total_tokens);
}

int run_print_mode(
    ai::Provider& provider,
    const ai::Model& model,
    const ai::Context& context,
    const ai::StreamOptions& stream_options,
    const PrintOptions& options,
    s::ostream& out,
    s::ostream& err,
    util::CancelToken cancel
){
  // Last char written to each stream ('\0' = nothing written yet).
  char out_last = '\0';
  char err_last = '\0';

  // End `out` with '\n' if the answer text didn't.
  auto finish_out = [&]() {
    if (out_last != '\0' && out_last != '\n'){
      out << '\n';
      out_last = '\n';
    }
    out.flush();
  };

  ai::EventStream stream = provider.stream(model, context, stream_options, cancel);
  while (auto event = stream.next()){
    // -1: keep streaming; otherwise the exit code.
    int rcode = s::visit(util::Overloaded {
      [&](const ai::TextDeltaEvent& e) -> int {
        if (e.delta.empty()) return -1;
        out << e.delta;
        out.flush();
        out_last = e.delta.back();
        return -1;
      },
      [&](const ai::ThinkingDeltaEvent& e) -> int {
        if (not options.show_thinking or e.delta.empty()) return -1;
        err << e.delta;
        err.flush();
        err_last = e.delta.back();
        return -1;
      },
      [&](const ai::ThinkingEndEvent&) -> int {
        if (not options.show_thinking or err_last == '\0') return -1;
        // Blank line between the thinking and the answer.
        err << (err_last == '\n' ? "\n" : "\n\n");
        err.flush();
        err_last = '\n';
        return -1;
      },
      [&](const ai::DoneEvent& e) -> int {
        finish_out();
        if (options.show_usage) err << format_usage(e.message.usage) << '\n';
        err.flush();
        return 0;
      },
      [&](const ai::ErrorEvent& e) -> int {
        finish_out();
        if (e.reason == ai::StopReason::aborted){
          err << "aborted\n";
          err.flush();
          return 130;
        }
        err << "error: " << e.message.error_message << '\n';
        err.flush();
        return 1;
      },
      [](const auto&) -> int { return -1; },
    }, *event);
    if (rcode >= 0) return rcode;
  }
  // The stream ended without a terminal event; treat it as a provider error.
  finish_out();
  err << "error: stream ended without a result\n";
  err.flush();
  return 1;
}

}  // namespace pi::modes
