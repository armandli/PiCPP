# Third-party dependencies, pinned to exact release tags.
include(FetchContent)

set(FETCHCONTENT_QUIET ON)

# FTXUI: terminal UI (screen / dom / component).
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG v7.0.3
  GIT_SHALLOW TRUE
  SYSTEM)

# simdjson: JSON parsing (plus string_builder for writing).
set(SIMDJSON_DEVELOPER_MODE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(simdjson
  GIT_REPOSITORY https://github.com/simdjson/simdjson.git
  GIT_TAG v5.0.2
  GIT_SHALLOW TRUE
  SYSTEM)

FetchContent_MakeAvailable(ftxui simdjson)

if(PICPP_BUILD_TESTS)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.18.0
    GIT_SHALLOW TRUE
    SYSTEM)
  FetchContent_MakeAvailable(googletest)
endif()

# libcurl: HTTP client for LLM providers (system copy; ships with macOS SDK).
find_package(CURL REQUIRED)

# On macOS FindCURL reports the SDK's usr/include, which the compiler already
# searches via the sysroot. As an explicit -isystem it lands ahead of libc++'s
# own wrapper headers and breaks <cmath>/<cstdlib>, so drop it.
if(APPLE)
  get_target_property(_picpp_curl_inc CURL::libcurl INTERFACE_INCLUDE_DIRECTORIES)
  if(_picpp_curl_inc)
    list(FILTER _picpp_curl_inc EXCLUDE REGEX "\\.sdk/usr/include$")
    set_target_properties(CURL::libcurl PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${_picpp_curl_inc}")
  endif()
endif()
