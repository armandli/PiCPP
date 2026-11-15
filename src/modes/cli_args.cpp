#include <modes/cli_args.h>

#include <format>
#include <sstream>

#include <CLI/CLI.hpp>

namespace pi::modes {

s::expected<ModelRef, str> parse_model_ref(strv text, strv default_provider) {
  if (text.empty()) return s::unexpected("empty model");
  size_t slash = text.find('/');
  if (slash == strv::npos)
    return ModelRef{(str)default_provider, (str)text};
  if (slash == 0)               return s::unexpected(s::format("empty provider in model \"{}\"", text));
  if (slash == text.size()-1)   return s::unexpected(s::format("empty id in model \"{}\"", text));
  return ModelRef{(str)text.substr(0, slash), (str)text.substr(slash+1)};
}

s::expected<CliArgs, CliExit> parse_cli_args(const vec<str>& args) {
  CliArgs out;
  str model_text;

  // -h,--help is built into CLI::App; registering it again throws OptionAlreadyAdded
  CLI::App app{"PiCPP: a C++ port of Pi coding agent", "pi"};
  app.set_version_flag("-v,--version", "pi (PiCPP) " PICPP_VERSION);
  CLI::Option* print = app.add_option("-p,--print", out.print_prompt, "Answer one prompt and exit");
  CLI::Option* model = app.add_option("--model", model_text, "<provider>/<id>")->envname("PI_MODEL");
  app.add_option("--system-prompt", out.system_prompt, "System prompt");
  app.add_option("--max-tokens", out.max_tokens, "Output token cap")->check(CLI::PositiveNumber);
  app.add_option("--ollama-url", out.ollama_url, "Ollama server root")->envname("PI_OLLAMA_URL");
  app.add_flag("--show-thinking", out.show_thinking, "Print thinking to stderr");

  print->needs(model);

  // App::parse(vec<str>&) expects the vector reversed and consumes it
  vec<str> rargs(args.rbegin(), args.rend());

  try {
    app.parse(rargs);
  } catch (const CLI::ParseError& e){
    // help/version go to stdout with code 0, errors to stderr with a non-zero code
    s::ostringstream sout, serr;
    int code = app.exit(e, sout, serr);
    return s::unexpected(CliExit{code, code == 0 ? sout.str() : serr.str()});
  }

  if (not model_text.empty()){
    s::expected<ModelRef, str> ref = parse_model_ref(model_text);
    if (not ref.has_value())
      return s::unexpected(CliExit{
          static_cast<int>(CLI::ExitCodes::ValidationError),
          s::format("--model: {}\nRun with --help for more information.\n", ref.error())});
    out.model = *ref;
  }

  return out;
}

s::expected<CliArgs, CliExit> parse_cli_args(int argc, const char* const* argv) {
  vec<str> args;
  for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
  return parse_cli_args(args);
}

}  // namespace pi::modes
