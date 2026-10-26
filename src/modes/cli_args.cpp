#include <modes/cli_args.h>

#include <CLI/CLI.hpp>

namespace pi::modes {

s::expected<ModelRef, str> parse_model_ref(strv text, strv default_provider) {
  // TODO(M3): see header.
  (void)text; (void)default_provider;
  return s::unexpected(str("not implemented"));
}

s::expected<CliArgs, CliExit> parse_cli_args(const vec<str>& args) {
  // TODO(M3): build a CLI::App (move the setup out of main.cpp):
  //   CliArgs out; str model_text;
  //   app.set_version_flag("-v,--version", "pi (PiCPP) " PICPP_VERSION);
  //   auto* print = app.add_option("-p,--print", ...);
  //   auto* model = app.add_option("--model", model_text)->envname("PI_MODEL");
  //   print->needs(model);
  //   --max-tokens ->check(CLI::PositiveNumber); --ollama-url ->envname("PI_OLLAMA_URL")
  //   try { app.parse(reversed copy of args); }
  //   catch (const CLI::ParseError& e) {
  //     // app.exit(e, out, err) prints to the streams you give it and returns
  //     // the exit code: capture with two s::ostringstream
  //   }
  //   then parse_model_ref(model_text) if given.
  // PICPP_VERSION comes from src/CMakeLists.txt (PUBLIC on pi_modes).
  (void)args;
  return s::unexpected(CliExit{1, "not implemented"});
}

s::expected<CliArgs, CliExit> parse_cli_args(int argc, const char* const* argv) {
  vec<str> args;
  for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
  return parse_cli_args(args);
}

}  // namespace pi::modes
