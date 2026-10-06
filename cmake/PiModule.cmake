# Helpers that turn each directory under src/ into a library target and each
# directory under test/ into a GoogleTest executable.
#
#   pi_add_module(<name> [DEPS <targets>...])
#     Target pi_<name> (alias pi::<name>) from src/<name>/**/*.cpp.
#     STATIC when .cpp files exist, INTERFACE while the module is still empty,
#     so the skeleton builds before anything is implemented. Re-run `make`
#     after adding the first .cpp; CONFIGURE_DEPENDS picks it up.
#
#   pi_add_test(<name> [DEPS <targets>...])
#     pi_<name>_tests       from test/<name>/*_test.cpp (minus live tests)
#     pi_<name>_live_tests  from test/<name>/*_live_test.cpp, ctest label "live"

function(pi_set_warnings target)
  target_compile_options(${target} PRIVATE
    $<$<CXX_COMPILER_ID:Clang,AppleClang,GNU>:-Wall -Wextra -Wpedantic>)
endfunction()

function(pi_set_sanitizers target scope)
  if(PICPP_SANITIZE)
    target_compile_options(${target} ${scope}
      -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} ${scope} -fsanitize=address,undefined)
  endif()
endfunction()

function(pi_add_module name)
  cmake_parse_arguments(ARG "" "" "DEPS" ${ARGN})
  set(target pi_${name})
  file(GLOB_RECURSE sources CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/src/${name}/*.cpp)

  if(sources)
    add_library(${target} STATIC ${sources})
    target_include_directories(${target} PUBLIC ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(${target} PUBLIC ${ARG_DEPS})
    pi_set_warnings(${target})
    pi_set_sanitizers(${target} PUBLIC)
  else()
    add_library(${target} INTERFACE)
    target_include_directories(${target} INTERFACE ${PROJECT_SOURCE_DIR}/src)
    target_link_libraries(${target} INTERFACE ${ARG_DEPS})
  endif()
  add_library(pi::${name} ALIAS ${target})
endfunction()

function(_pi_make_test_exe target sources deps)
  add_executable(${target} ${sources})
  target_include_directories(${target} PRIVATE
    ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/test)
  target_link_libraries(${target} PRIVATE ${deps} GTest::gtest_main GTest::gmock)
  pi_set_warnings(${target})
  pi_set_sanitizers(${target} PRIVATE)
endfunction()

function(pi_add_test name)
  cmake_parse_arguments(ARG "" "" "DEPS" ${ARGN})
  file(GLOB all_sources CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/test/${name}/*_test.cpp)
  file(GLOB live_sources CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/test/${name}/*_live_test.cpp)
  set(unit_sources ${all_sources})
  if(live_sources)
    list(REMOVE_ITEM unit_sources ${live_sources})
  endif()

  if(unit_sources)
    _pi_make_test_exe(pi_${name}_tests "${unit_sources}" "${ARG_DEPS}")
    gtest_discover_tests(pi_${name}_tests
      TEST_PREFIX "${name}."
      DISCOVERY_MODE PRE_TEST)
  endif()

  if(live_sources)
    _pi_make_test_exe(pi_${name}_live_tests "${live_sources}" "${ARG_DEPS}")
    gtest_discover_tests(pi_${name}_live_tests
      TEST_PREFIX "${name}.live."
      PROPERTIES LABELS live
      DISCOVERY_MODE PRE_TEST)
  endif()
endfunction()
