#include <modes/print_mode.h>

#include <ai/events.h>

namespace pi::modes {

ai::Context make_print_context(strv prompt, strv system_prompt) {
  // TODO(M3): see header.
  (void)prompt; (void)system_prompt;
  return {};
}

str format_usage(const ai::Usage& usage) {
  // TODO(M3): see header for the exact format.
  (void)usage;
  return {};
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
  // TODO(M3): see header.  Shape:
  //   ai::EventStream stream = provider.stream(model, context, stream_options, cancel);
  //   while (auto event = stream.next()) { s::visit(..., *event); }
  //   Track whether the last char written to `out` was '\n' for the final newline.
  (void)provider; (void)model; (void)context; (void)stream_options;
  (void)options; (void)out; (void)err; (void)cancel;
  return 1;
}

}  // namespace pi::modes
