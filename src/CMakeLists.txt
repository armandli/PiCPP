# Modules in dependency order (each may only depend on modules above it).
pi_add_module(util)
pi_add_module(json   DEPS pi::util simdjson::simdjson)
pi_add_module(http   DEPS pi::util CURL::libcurl)
pi_add_module(ai     DEPS pi::json pi::http)
pi_add_module(agent  DEPS pi::ai)
pi_add_module(tools  DEPS pi::agent)
pi_add_module(coding DEPS pi::tools)
pi_add_module(tui    DEPS pi::coding ftxui::screen ftxui::dom ftxui::component)
pi_add_module(modes  DEPS pi::tui CLI11::CLI11)
# --version text; PUBLIC so the pi executable and the modes tests see it too.
target_compile_definitions(pi_modes PUBLIC PICPP_VERSION="${PROJECT_VERSION}")

add_executable(pi main.cpp)
# pi::modes transitively brings in every module and third-party library.
target_link_libraries(pi PRIVATE pi::modes)
pi_set_warnings(pi)
pi_set_sanitizers(pi PRIVATE)
