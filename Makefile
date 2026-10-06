# Thin wrapper around CMake. All real build logic lives in CMakeLists.txt.

BUILD_TYPE ?= Debug
BUILD_DIR  ?= build/$(BUILD_TYPE)
JOBS       ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
ARGS       ?=

# Homebrew LLVM has the most complete C++26 support on macOS; fall back to the
# system compiler elsewhere. Override with `make CXX=g++-16`.
HOMEBREW_CLANG := /opt/homebrew/opt/llvm/bin/clang++
ifeq ($(origin CXX),default)
  CXX := $(if $(wildcard $(HOMEBREW_CLANG)),$(HOMEBREW_CLANG),c++)
endif
ifeq ($(origin CC),default)
  CC := $(if $(wildcard /opt/homebrew/opt/llvm/bin/clang),/opt/homebrew/opt/llvm/bin/clang,cc)
endif

GENERATOR := $(if $(shell command -v ninja 2>/dev/null),Ninja,Unix Makefiles)

CMAKE_FLAGS ?=

.PHONY: all build configure test test-live test-all run release clean distclean help

all: build

$(BUILD_DIR)/CMakeCache.txt:
	cmake -S . -B $(BUILD_DIR) -G "$(GENERATOR)" \
	  -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
	  -DCMAKE_CXX_COMPILER=$(CXX) \
	  -DCMAKE_C_COMPILER=$(CC) \
	  $(CMAKE_FLAGS)
	@ln -sf $(BUILD_DIR)/compile_commands.json compile_commands.json

configure:
	@rm -f $(BUILD_DIR)/CMakeCache.txt
	@$(MAKE) --no-print-directory $(BUILD_DIR)/CMakeCache.txt

build: $(BUILD_DIR)/CMakeCache.txt
	cmake --build $(BUILD_DIR) -j $(JOBS)

# Unit tests only; tests that need a running Ollama carry the `live` label.
test: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure -LE live -j $(JOBS)

test-live: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure -L live --no-tests=ignore

test-all: build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

run: build
	$(BUILD_DIR)/pi $(ARGS)

release:
	@$(MAKE) --no-print-directory BUILD_TYPE=Release build

clean:
	@if [ -d $(BUILD_DIR) ]; then cmake --build $(BUILD_DIR) --target clean; fi

distclean:
	rm -rf build compile_commands.json

help:
	@echo "Targets:"
	@echo "  make [build]        configure (first time) and build ($(BUILD_TYPE))"
	@echo "  make configure      re-run CMake configure from scratch"
	@echo "  make test           run unit tests (excludes label 'live')"
	@echo "  make test-live      run tests that talk to a local Ollama"
	@echo "  make test-all       run every test"
	@echo "  make run ARGS=...   build and run ./$(BUILD_DIR)/pi"
	@echo "  make release        build with BUILD_TYPE=Release"
	@echo "  make clean          clean build outputs of $(BUILD_DIR)"
	@echo "  make distclean      delete the whole build/ tree"
	@echo "Variables: BUILD_TYPE=Debug|Release|RelWithDebInfo  CXX=...  JOBS=N"
	@echo "           CMAKE_FLAGS='-DPICPP_SANITIZE=ON'  ARGS='-p hello'"
