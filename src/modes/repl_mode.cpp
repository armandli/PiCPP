#include <modes/repl_mode.h>

#include <modes/print_mode.h>
#include <util/strings.h>

namespace pi::modes {

ReplLine parse_repl_line(strv line) {
  // TODO(M4): see header.
  (void)line;
  return {};
}

str format_model_list(const vec<ai::Model>& models, strv current_id) {
  // TODO(M4): see header.
  (void)models; (void)current_id;
  return {};
}

int run_repl_mode(
    ai::Provider& provider,
    const ai::Model& model,
    strv system_prompt,
    const ai::StreamOptions& stream_options,
    const ReplOptions& options,
    s::istream& in,
    s::ostream& out,
    s::ostream& err,
    util::CancelToken cancel
){
  // TODO(M4): see header.  Shape:
  //   vec<ai::Message> conversation;
  //   for (;;) {
  //     out << options.prompt << s::flush;
  //     str line;
  //     if (not s::getline(in, line)) { out << '\n'; return 0; }
  //     ReplLine r = parse_repl_line(line);
  //     switch (r.kind) { ... }
  //   }
  // PrintOptions{options.show_thinking, options.show_usage} for print_reply().
  (void)provider; (void)model; (void)system_prompt; (void)stream_options;
  (void)options; (void)in; (void)out; (void)err; (void)cancel;
  return 1;
}

}  // namespace pi::modes
