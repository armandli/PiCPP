#ifndef MODES_REPL_MODE_H
#define MODES_REPL_MODE_H

#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

#include <ai/provider.h>
#include <ai/types.h>
#include <util/aliases.h>
#include <util/cancel.h>

// `pi --model <m>` without -p: a plain stdin/stdout line REPL.  Not in Pi; a
// stepping stone that keeps a conversation before the TUI arrives in M9.

namespace pi::modes {

struct ReplOptions {
  bool show_thinking = false;  // as PrintOptions
  bool show_usage    = true;   // usage line on `err` after each answer
  str  prompt        = "> ";   // written to `out` before each line is read
};

enum class ReplLineKind : uint8_t {
  Prompt,   // text for the model
  Empty,    // blank line: ignored
  Quit,     // /quit or /exit
  Clear,    // /clear
  Models,   // /models
  Unknown,  // any other /word
};

struct ReplLine {
  ReplLineKind kind = ReplLineKind::Empty;
  str          text;  // Prompt: the trimmed line; Unknown: the command word ("/foo")

  bool operator==(const ReplLine&) const = default;
};

// Classify one input line (trailing '\r' included in the trim):
//  - only whitespace → Empty
//  - starts with '/': the command word is up to the first whitespace;
//    "/quit", "/exit" → Quit, "/clear" → Clear, "/models" → Models (arguments
//    after the word are ignored), anything else → Unknown{word}
//  - else Prompt{trimmed line}
ReplLine parse_repl_line(strv line);

// The /models listing: one line per model, "* <id>\n" for the one whose id is
// `current_id`, "  <id>\n" for the rest, in the given order.  No models →
// "no models available\n".
str format_model_list(const vec<ai::Model>& models, strv current_id);

// Run the REPL until EOF or /quit; returns the exit code.
//  - before each read: options.prompt on `out`, flushed
//  - EOF → '\n' on `out` (so the shell prompt starts on a fresh line); return 0
//  - Empty → read again; Quit → return 0
//  - Clear → forget the conversation (the system prompt stays);
//    "context cleared\n" on `err`
//  - Models → format_model_list(provider.models(), model.id) on `out`
//  - Unknown{w} → "unknown command: <w> (try /models, /clear, /quit)\n" on `err`
//  - Prompt{t} → append UserMessage{{TextContent{t}}, now_ms()} and answer with
//    print_reply() on a Context{system_prompt, conversation, {}}:
//      code 0   → append the reply's AssistantMessage; read again
//      code 1   → remove that UserMessage again, so every user turn in the
//                 history has an answer (a "poisoned" history otherwise
//                 carries a question the model never saw answered); read again
//      code 130 → return 130: `cancel` can't be reset, so one Ctrl-C ends the
//                 session (M8 aborts just the turn)
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
);

}  // namespace pi::modes

#endif  // MODES_REPL_MODE_H
