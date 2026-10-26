#include <atomic>
#include <csignal>
#include <cstdlib>

#include <iostream>
#include <string_view>
#include <thread>

#include <CLI/CLI.hpp>
#include <curl/curl.h>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <simdjson.h>

#include <http/curl_client.h>
#include <http/sse.h>
#include <json/value.h>
#include <json/write.h>
#include <util/aliases.h>
#include <util/cancel.h>

namespace {
namespace h = pi::http;
namespace j = pi::json;

volatile s::sig_atomic_t gInterrupted = 0;

void on_sigint(int){
  gInterrupted = 1;
}

// Parsed command line.  CLI11 binds to these by reference, so the instance must
// outlive parse().
struct CliOptions {
  str raw_prompt;
  str model;
  str ollama_url = "http://localhost:11434";
};

// M2 demo (temporary, removed once print mode lands in M3): POST one user
// message to Ollama's OpenAI-compatible endpoint and print every SSE `data:`
// payload as it arrives.  Ctrl-C cancels the stream through the CancelToken.
int raw_chat(const CliOptions& options){
  j::Value message = j::object_value();
  message.set("role", j::Value("user"));
  message.set("content", j::Value(options.raw_prompt));
  j::Value messages = j::array_value();
  messages.push_back(s::move(message));
  j::Value request = j::object_value();
  request.set("model", j::Value(options.model));
  request.set("stream", j::Value(true));
  request.set("messages", s::move(messages));

  // The XFERINFO callback polls the token, but cancelling from inside a signal
  // handler isn't async-signal-safe, so a watcher thread forwards the flag.
  pi::util::CancelToken cancel;
  s::signal(SIGINT, on_sigint);
  s::jthread watcher([&cancel](s::stop_token stop){
    while (not stop.stop_requested() and not gInterrupted)
      st::sleep_for(sc::milliseconds(50));
    if (gInterrupted) cancel.cancel();
  });

  h::CurlClient client;
  h::SseDecoder decoder;
  auto status = client.post_stream(
      options.ollama_url + "/v1/chat/completions",
      {{"Content-Type", "application/json"}},
      j::to_json(request),
      [&decoder](strv chunk){
        for (const h::SseEvent& event : decoder.feed(chunk))
          s::cout << event.data << '\n' << s::flush;
      },
      cancel);

  if (not status){
    const h::HttpError& error = status.error();
    if (error.kind == h::HttpErrorKind::Cancelled) s::cerr << "[cancelled]\n";
    else if (error.kind == h::HttpErrorKind::Status) s::cerr << "HTTP " << error.status << ": " << error.message << '\n';
    else s::cerr << "request failed: " << error.message << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
} // namespace

// M0 toolchain check: proves FTXUI, simdjson and libcurl all link.
// Replaced by modes/cli_args + mode dispatch in M3/M4 (see TODO.md).
int main(int argc, char** argv) {
  namespace f = ftxui;

  CliOptions options;
  CLI::App app{"PiCPP: a C++ port of the Pi coding agent", "pi"};
  app.set_version_flag("-v,--version", "pi (PiCPP) " PICPP_VERSION);
  CLI::Option* raw_chat_opt = app.add_option("--raw-chat", options.raw_prompt,
      "M2 demo: stream a reply to PROMPT and print each SSE data payload")
    ->option_text("PROMPT");
  CLI::Option* model_opt = app.add_option("--model", options.model, "Ollama model id (see `ollama list`)")
    ->envname("PI_OLLAMA_MODEL");
  app.add_option("--ollama-url", options.ollama_url, "Ollama server base URL")
    ->envname("PI_OLLAMA_URL")
    ->capture_default_str();
  raw_chat_opt->needs(model_opt);
  CLI11_PARSE(app, argc, argv);

  if (raw_chat_opt->count() > 0) return raw_chat(options);

  simdjson::dom::parser parser;
  simdjson::dom::element doc;
  if (parser.parse(simdjson::padded_string(strv(
          R"({"name":"pi","lang":"C++26"})"))).get(doc)) {
    s::cerr << "simdjson parse failed\n";
    return EXIT_FAILURE;
  }
  strv lang;
  if (doc["lang"].get(lang)) {
    return EXIT_FAILURE;
  }

  f::Element banner = f::vbox({
      f::text("PiCPP " PICPP_VERSION) | f::bold,
      f::separator(),
      f::text("C++ standard : " + str(lang)),
      f::text("libcurl      : " + str(curl_version())),
      f::text("Nothing implemented yet. Start with M1 in TODO.md."),
  }) | f::border;

  f::Screen screen = f::Screen::Create(f::Dimension::Fit(banner));
  f::Render(screen, banner);
  screen.Print();
  s::cout << '\n';
  return EXIT_SUCCESS;
}
