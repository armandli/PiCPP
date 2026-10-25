# Compiler-specific fixups that must apply to every target, including
# third-party dependencies fetched by Dependencies.cmake.

# Homebrew LLVM ships its own libc++ whose headers are newer than the system
# dylib in /usr/lib. Linking against the system libc++ then fails (or crashes
# at runtime) for newer C++26 library features, so link and rpath against
# Homebrew's copy instead.
if(APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  get_filename_component(_picpp_llvm_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
  get_filename_component(_picpp_llvm_root "${_picpp_llvm_bin}" DIRECTORY)
  set(_picpp_libcxx_dir "${_picpp_llvm_root}/lib/c++")
  # Skip when the user's LDFLAGS (Homebrew's suggested setup) already did it.
  string(FIND "${CMAKE_EXE_LINKER_FLAGS}" "-rpath,${_picpp_libcxx_dir}" _picpp_has_rpath)
  if(EXISTS "${_picpp_libcxx_dir}/libc++.dylib" AND _picpp_has_rpath EQUAL -1)
    message(STATUS "PiCPP: linking Homebrew libc++ from ${_picpp_libcxx_dir}")
    string(APPEND CMAKE_EXE_LINKER_FLAGS
      " -L${_picpp_libcxx_dir} -Wl,-rpath,${_picpp_libcxx_dir}"
      " -L${_picpp_llvm_root}/lib/unwind -lunwind")
  endif()
endif()

# The resource dir is detected once and cached under its versioned Cellar path;
# `brew upgrade llvm` deletes it and dependency scanning then fails with
# "'stdarg.h' file not found". `make` reconfigures automatically; this catches
# direct cmake users.
if(CMAKE_CXX_COMPILER_CLANG_RESOURCE_DIR
   AND NOT EXISTS "${CMAKE_CXX_COMPILER_CLANG_RESOURCE_DIR}")
  message(FATAL_ERROR
    "Cached clang resource dir ${CMAKE_CXX_COMPILER_CLANG_RESOURCE_DIR} no "
    "longer exists (compiler upgraded?). Reconfigure with `make configure` "
    "or `cmake --fresh -B ${CMAKE_BINARY_DIR}`.")
endif()
