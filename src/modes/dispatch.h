#ifndef MODES_DISPATCH_H
#define MODES_DISPATCH_H

#include <ostream>

#include <ai/provider.h>
#include <ai/types.h>
#include <modes/cli_args.h>
#include <util/aliases.h>
#include <util/cancel.h>

// From parsed CliArgs to a running mode; what main() calls.  Replaces the M0
// banner in main.cpp (and the temporary M2 --raw-chat demo).

namespace pi::modes {

// The provider named by args.model->provider:
//  - "ollama" → OpenAiCompletionsProvider over a CurlClient
//  - "faux"   → an empty FauxProvider (useful to smoke-test the wiring)
//  - otherwise, or no --model → nullptr
uptr<ai::Provider> make_provider(const CliArgs& args);

// The Model for args.model: for "ollama", make_ollama_model(id, args.ollama_url
// + "/v1") (strip a trailing '/' from ollama_url first); for "faux", the faux
// provider's model with id replaced.  nullopt if no --model or unknown provider.
opt<ai::Model> resolve_model(const CliArgs& args);

// Run the mode selected by `args`:
//  - print_prompt set → run_print_mode with make_print_context(prompt,
//    system_prompt), StreamOptions{max_tokens}, PrintOptions{show_thinking}
//  - unknown provider → "unknown provider \"<p>\"\n" on err, return 2
//  - no mode selected → "nothing to do: interactive mode arrives in M4; use -p\n"
//    on err, return 2
// `cancel` is forwarded to the mode (main() cancels it on SIGINT).
int dispatch(const CliArgs& args, s::ostream& out, s::ostream& err, util::CancelToken cancel);

}  // namespace pi::modes

#endif  // MODES_DISPATCH_H
