cmake_minimum_required(VERSION 3.28)

project(PiCPP
  VERSION 0.0.1
  DESCRIPTION "C++26 reimplementation of the Pi coding agent"
  LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Executables (pi and test binaries) land directly in the build directory.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

option(PICPP_BUILD_TESTS "Build the GoogleTest suites" ON)
option(PICPP_SANITIZE "Build project targets with ASan + UBSan" OFF)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(Toolchain)
include(Dependencies)
include(PiModule)

add_subdirectory(src)

if(PICPP_BUILD_TESTS)
  enable_testing()
  include(GoogleTest)
  add_subdirectory(test)
endif()
