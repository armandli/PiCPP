#include <cstdlib>

#include <iostream>
#include <string_view>

#include <curl/curl.h>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <simdjson.h>

// M0 toolchain check: proves FTXUI, simdjson and libcurl all link.
// Replaced by modes/cli_args + mode dispatch in M3/M4 (see TODO.md).
int main(int argc, char** argv) {
  namespace f = ftxui;

  for (int i = 1; i < argc; ++i) {
    std::string_view arg = argv[i];
    if (arg == "--version" or arg == "-v") {
      std::cout << "pi (PiCPP) " << PICPP_VERSION << '\n';
      return EXIT_SUCCESS;
    }
  }

  simdjson::dom::parser parser;
  simdjson::dom::element doc;
  if (parser.parse(simdjson::padded_string(std::string_view(
          R"({"name":"pi","lang":"C++26"})"))).get(doc)) {
    std::cerr << "simdjson parse failed\n";
    return EXIT_FAILURE;
  }
  std::string_view lang;
  if (doc["lang"].get(lang)) {
    return EXIT_FAILURE;
  }

  f::Element banner = f::vbox({
      f::text("PiCPP " PICPP_VERSION) | f::bold,
      f::separator(),
      f::text("C++ standard : " + std::string(lang)),
      f::text("libcurl      : " + std::string(curl_version())),
      f::text("Nothing implemented yet. Start with M1 in TODO.md."),
  }) | f::border;

  f::Screen screen = f::Screen::Create(f::Dimension::Fit(banner));
  f::Render(screen, banner);
  screen.Print();
  std::cout << '\n';
  return EXIT_SUCCESS;
}
