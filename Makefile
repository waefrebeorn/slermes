# ── Slermes Makefile ────────────────────────────────────────────────
# Top-level entry point. Build configuration is modularized under build/*.mk
#
# Quick start:
#   make -j$$(nproc)          # build everything (phase5)
#   make -j$$(nproc) clean    # clean build artifacts
#   make install             # install to /usr/local
#   make test                # run test suite
#   make help                # show all targets
#
# Parallel builds: make -j$$(nproc)
# Cross-compile:   make CC=clang  or  make CC=x86_64-w64-mingw32-gcc
# Single phases:   make phase1 phase2 phase3 phase4 phase5

# ── Load modular build system ───────────────────────────────────────
include build/config.mk
include build/libs-config.mk
include build/objects.mk
include build/rules.mk
include build/targets.mk
include build/test.mk
include build/install.mk
include build/dist.mk
include build/dev.mk
include build/plugins.mk
include build/clean.mk

# ── Legacy alias / backward compatibility ───────────────────────────

.PHONY: help

help:
	@echo "Slermes Build System -- modular Makefile"
	@echo ""
	@echo "BUILD TARGETS:"
	@echo "  all (default)     Build Phase 5 (full slermes binary)"
	@echo "  phase{1..5}       Build incrementally by phase"
	@echo "  slermes            Link the main binary (same as 'all')"
	@echo "  tui               Build with ncurses full-screen TUI"
	@echo "  desktop           Build legacy ncurses desktop app"
	@echo "  desktop-gui       Build SDL2-based GUI desktop app"
	@echo "  static            Build fully static binary"
	@echo "  fuzz              Build fuzz test harness"
	@echo "  libs              Build all standalone .a libraries"
	@echo ""
	@echo "TEST & QUALITY TARGETS:"
	@echo "  test              Run the test suite (needs slermes)"
	@echo "  test-libs         Test each standalone library"
	@echo "  check             Lint + build + test suite"
	@echo "  asan              Build with AddressSanitizer"
	@echo "  asan-test         ASan build + run tests under ASan"
	@echo "  valgrind          Run valgrind leak check on --help"
	@echo "  coverage          Build with gcov, run tests, generate HTML report"
	@echo "  coverage-gate     Coverage check with threshold (COVERAGE_THRESHOLD=%)"
	@echo "  perf-gate         Check binary size & startup time vs baseline"
	@echo ""
	@echo "INSTALL & DISTRIBUTION:"
	@echo "  install           Install to PREFIX (/usr/local)"
	@echo "  uninstall         Remove installed binary (FORCE=1 for config too)"
	@echo "  dist-docker       Build Docker image"
	@echo "  dist-appimage     Build AppImage"
	@echo "  dist-nsis         Build Windows NSIS installer"
	@echo "  dist-nix          Build Nix derivation"
	@echo ""
	@echo "DEVELOPER TARGETS:"
	@echo "  docs              Generate Doxygen API docs"
	@echo "  static-analysis   Run cppcheck on src/"
	@echo "  upstream-sync     Show new Python changes from upstream"
	@echo "  upstream-merge    Fetch + merge upstream + generate stubs"
	@echo "  digest            Run local diff digest"
	@echo "  python-deps       Install Python bridge dependencies"
	@echo "  release           Run release script"
	@echo "  clean             Remove all build artifacts"
	@echo ""
	@echo "VARIABLES:"
	@echo "  CC                C compiler (auto: clang/gcc)"
	@echo "  CXX               C++ compiler (auto: clang++/g++)"
	@echo "  CFLAGS_EXTRA      Append extra C flags"
	@echo "  PREFIX            Install prefix (default: /usr/local)"
	@echo "  DESTDIR           Staging directory for install"
	@echo ""
	@echo "EXAMPLES:"
	@echo "  make -j$$$$(nproc)                       # full parallel build"
	@echo "  make test PREFIX=~/.local               # test + custom prefix"
	@echo "  make clean all CC=clang                 # clean build with clang"
	@echo "  make asan-test COVERAGE_THRESHOLD=10.0  # asan + 10%% coverage gate"
