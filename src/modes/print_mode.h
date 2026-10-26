#ifndef MODES_PRINT_MODE_H
#define MODES_PRINT_MODE_H

#include <ostream>
#include <string>

#include <ai/provider.h>
#include <ai/types.h>
#include <util/aliases.h>
#include <util/cancel.h>

// `pi -p "<prompt>"`: one request, streamed to stdout (Pi: modes/print-mode.ts).

namespace pi::modes {

struct PrintOptions {
  bool show_thinking = false;  // thinking deltas go to `err` as they arrive
  bool show_usage    = true;   // format_usage(...) line on `err` at the end
};

// One UserMessage (single TextContent `prompt`, timestamp now_ms()) and the
// system prompt.
ai::Context make_print_context(strv prompt, strv system_prompt = "");

// "tokens: <input> in, <output> out, <cache_read> cached, <total_tokens> total"
str format_usage(const ai::Usage& usage);

// Stream one reply and print it.
//  - text deltas → `out` immediately (flush after each), so the answer appears
//    as it is generated; stdout carries only the answer text
//  - thinking deltas → `err` when show_thinking; a blank line on `err` separates
//    thinking from the answer
//  - on DoneEvent: end `out` with '\n' if the text didn't; usage line on `err`
//    (if show_usage); return 0
//  - on ErrorEvent{error}: "error: <error_message>\n" on `err`; return 1
//  - on ErrorEvent{aborted}: "aborted\n" on `err`; return 130 (like SIGINT)
int run_print_mode(
    ai::Provider& provider,
    const ai::Model& model,
    const ai::Context& context,
    const ai::StreamOptions& stream_options,
    const PrintOptions& options,
    s::ostream& out,
    s::ostream& err,
    util::CancelToken cancel);

}  // namespace pi::modes

#endif  // MODES_PRINT_MODE_H
