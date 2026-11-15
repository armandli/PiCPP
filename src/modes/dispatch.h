#ifndef MODES_DISPATCH_H
#define MODES_DISPATCH_H

#include <istream>
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
//  - "ollama" → OpenAiCompletionsProvider over a CurlClient, with
//    options.base_url = ollama_base_url(args) so /models reaches the same
//    server as the chat requests
//  - "faux"   → an empty FauxProvider (useful to smoke-test the wiring)
//  - otherwise, or no --model → nullptr
uptr<ai::Provider> make_provider(const CliArgs& args);

// args.ollama_url without a trailing '/', plus "/v1": what make_provider and
// resolve_model both point Ollama at.
str ollama_base_url(const CliArgs& args);

// The Model for args.model: for "ollama", make_ollama_model(id,
// ollama_base_url(args)); for "faux", the faux provider's model with id
// replaced.  nullopt if no --model or unknown provider.
opt<ai::Model> resolve_model(const CliArgs& args);

// Run the mode selected by `args`:
//  - no --model → "no model: pass --model <provider>/<id> or set PI_MODEL\n"
//    on err, return 2 (-p without --model is already a CLI usage error)
//  - unknown provider → "unknown provider \"<p>\"\n" on err, return 2
//  - print_prompt set → run_print_mode with make_print_context(prompt,
//    system_prompt), StreamOptions{max_tokens}, PrintOptions{show_thinking};
//    `in` is not read
//  - otherwise → run_repl_mode(provider, model, system_prompt,
//    StreamOptions{max_tokens}, ReplOptions{show_thinking}, in, out, err, cancel)
// `cancel` is forwarded to the mode (main() cancels it on SIGINT).
int dispatch(
    const CliArgs& args,
    s::istream& in,
    s::ostream& out,
    s::ostream& err,
    util::CancelToken cancel
);

}  // namespace pi::modes

#endif  // MODES_DISPATCH_H
