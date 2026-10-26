#ifndef MODES_CLI_ARGS_H
#define MODES_CLI_ARGS_H

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

#include <util/aliases.h>

// Command-line parsing for the pi executable (Pi: coding-agent/src/cli/args.ts).
// M3 covers print mode only; --continue, --resume, --mode and --session follow
// in later milestones.

namespace pi::modes {

// "<provider>/<id>" from --model.
struct ModelRef {
  str provider;
  str id;

  bool operator==(const ModelRef&) const = default;
};

// Split at the FIRST '/': "ollama/qwen3.8:27b-mlx" → {"ollama", "qwen3.8:27b-mlx"},
// "ollama/hf.co/org/model:q4" → {"ollama", "hf.co/org/model:q4"}.  Without a
// '/', the whole text is the id and the provider is `default_provider`.
// Errors: empty text, empty provider ("/x") or empty id ("ollama/").
s::expected<ModelRef, str> parse_model_ref(strv text, strv default_provider = "ollama");

struct CliArgs {
  opt<str>     print_prompt;   // -p,--print <prompt>
  opt<ModelRef> model;         // --model <provider>/<id>; env PI_MODEL
  str          system_prompt;  // --system-prompt <text>
  opt<int64_t> max_tokens;     // --max-tokens <n>, n > 0
  str          ollama_url = "http://localhost:11434";  // --ollama-url; env PI_OLLAMA_URL (server root, no /v1)
  bool         show_thinking = false;  // --show-thinking: print thinking to stderr

  bool operator==(const CliArgs&) const = default;
};

// Parsing stopped without CliArgs: --help / --version (code 0, text for stdout)
// or a usage error (CLI11's non-zero exit code, message for stderr).
struct CliExit {
  int code = 0;
  str output;
  bool is_error() const { return code != 0; }
};

// Parse `args` (argv WITHOUT the program name, in natural order).
//  - built on CLI11 (see main.cpp for the current setup); -h/--help and
//    -v/--version ("pi (PiCPP) <PICPP_VERSION>") come back as CliExit{0, text}
//  - -p requires --model (or PI_MODEL): CliExit{non-zero, "...requires --model..."}
//  - --model goes through parse_model_ref; a bad value is a usage error
//  - unknown arguments are usage errors
//  - error output comes from app.exit()-style formatting, e.g.
//    "--print requires --model\nRun with --help for more information."
// CLI11 pitfall: App::parse(vec<str>&) expects the vector REVERSED and consumes
// it; reverse a copy (or join and use the string overload, mind the quoting).
s::expected<CliArgs, CliExit> parse_cli_args(const vec<str>& args);

// Convenience for main(): skips argv[0].
s::expected<CliArgs, CliExit> parse_cli_args(int argc, const char* const* argv);

}  // namespace pi::modes

#endif  // MODES_CLI_ARGS_H
