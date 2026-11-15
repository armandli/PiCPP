#include <modes/dispatch.h>

#include <ai/providers/faux.h>
#include <ai/providers/openai_completions.h>
#include <http/curl_client.h>
#include <modes/print_mode.h>
#include <modes/repl_mode.h>

namespace pi::modes {

uptr<ai::Provider> make_provider(const CliArgs& args) {
  if (args.model.has_value() and (*args.model).provider == "ollama"){
    ai::providers::OpenAiCompletionsOptions options;
    options.provider_id = (*args.model).provider;
    // TODO(M4): options.base_url = ollama_base_url(args);
    return s::make_unique<ai::providers::OpenAiCompletionsProvider>(
        s::make_shared<http::CurlClient>(http::CurlOptions{}), s::move(options));
  }
  if (args.model.has_value() and (*args.model).provider == "faux"){
    return s::make_unique<ai::providers::FauxProvider>(ai::providers::FauxOptions{0, (*args.model).provider});
  }
  return nullptr;
}

str ollama_base_url(const CliArgs& args) {
  // TODO(M4): see header; then use it in resolve_model (replacing the inline
  // slash strip below) and make_provider.
  (void)args;
  return {};
}

opt<ai::Model> resolve_model(const CliArgs& args) {
  if (args.model.has_value() and (*args.model).provider == "ollama"){
    //TODO: refactor this out as a utility function to be shared
    str url = args.ollama_url;
    if (url.size() > 0 and url.back() == '/')
      url = url.substr(0, url.size()-1);
    return ai::providers::make_ollama_model((*args.model).id, url + "/v1");
  }
  if (args.model.has_value() and (*args.model).provider == "faux"){
    ai::providers::FauxProvider p;
    ai::Model model = p.models()[0];
    model.id = (*args.model).id;
    return model;
  }
  return s::nullopt;
}

int dispatch(
    const CliArgs& args,
    s::istream& in,
    s::ostream& out,
    s::ostream& err,
    util::CancelToken cancel
){
  // TODO(M4): see header: check for a missing --model first, and run the REPL
  // (reading `in`) instead of this message when -p isn't given.
  (void)in;
  if (not args.print_prompt){
    err << "nothing to do: interactive mode arrives in M4; use -p\n";
    return 2;
  }
  uptr<ai::Provider> provider = make_provider(args);
  if (provider == nullptr){
    err << s::format("unknown provider \"{}\"\n", args.model.has_value() ? (*args.model).provider : "");
    return 2;
  }
  opt<ai::Model> model = resolve_model(args);
  if (not model.has_value()){
    err << "unknown model\n";
    return 2;
  }
  ai::Context context = make_print_context(*args.print_prompt, args.system_prompt);
  ai::StreamOptions so;
  if (args.max_tokens.has_value())
    so.max_tokens = *args.max_tokens;
  PrintOptions po{args.show_thinking, true};
  return run_print_mode(*provider, *model, context, so, po, out, err, cancel);
}

}  // namespace pi::modes
