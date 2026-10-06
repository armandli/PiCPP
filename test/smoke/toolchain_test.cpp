#include <string>
#include <string_view>

#include <curl/curl.h>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <gtest/gtest.h>
#include <simdjson.h>

// Verifies every third-party dependency compiles and links under C++26.

TEST(Toolchain, CppStandardIs26OrNewer) {
  EXPECT_GT(__cplusplus, 202302L);
}

TEST(Toolchain, SimdjsonParsesObject) {
  simdjson::dom::parser parser;
  simdjson::dom::element doc;
  ASSERT_FALSE(parser.parse(simdjson::padded_string(
      std::string_view(R"({"role":"user","n":3})"))).get(doc));
  std::string_view role;
  int64_t n = 0;
  ASSERT_FALSE(doc["role"].get(role));
  ASSERT_FALSE(doc["n"].get(n));
  EXPECT_EQ(role, "user");
  EXPECT_EQ(n, 3);
}

TEST(Toolchain, FtxuiRendersToString) {
  namespace f = ftxui;
  f::Element el = f::text("hello");
  f::Screen screen = f::Screen::Create(f::Dimension::Fixed(5),
                                       f::Dimension::Fixed(1));
  f::Render(screen, el);
  EXPECT_NE(screen.ToString().find("hello"), std::string::npos);
}

TEST(Toolchain, CurlReportsVersion) {
  std::string_view version = curl_version();
  EXPECT_NE(version.find("libcurl"), std::string_view::npos);
}
