# ── Build Configuration ─────────────────────────────────────────────
# Version detection, compiler selection, platform detection, CFLAGS/LDFLAGS
# Included by top-level Makefile. Expected vars: HERMES_VERSION, HERMES_RELEASE_DATE

# Dynamically detect Python Hermes version from upstream source
HERMES_VERSION := $(shell python3 -c "import sys; sys.path.insert(0, '..'); from hermes_cli import __version__; print(__version__)" 2>/dev/null || echo "0.15.1")
HERMES_RELEASE_DATE := $(shell python3 -c "import sys; sys.path.insert(0, '..'); from hermes_cli import __release_date__; print(__release_date__)" 2>/dev/null || echo "2026.5.29")

# ── Cross-distro build setup ─────────────────────────────────────────────
# Auto-detect compiler: prefer clang on macOS/FreeBSD, gcc on Linux.
# When ccache is available it is inserted as a compiler wrapper so repeated
# (and CI) builds hit the cache — a clean warm-cache build is ~30s on 24 cores
# instead of minutes, and no-op rebuilds finish in well under a second.
ifeq ($(origin CC),default)
    ifneq ($(shell command -v ccache 2>/dev/null),)
        # ccache is a native Windows binary: it cannot exec a POSIX path like
        # /c/winlibs/... so the compiler path is converted to C:/... form.
        # `cygpath -m` is correct for every MSYS mount (/c/x -> C:/x,
        # /mingw64/x -> C:/msys64/mingw64/x); the old toupper(substr trick was
        # only right for /c/ and mangled /mingw64/ into "M:ingw64/".
        CC := ccache $(shell c=$$(command -v clang 2>/dev/null || command -v gcc 2>/dev/null || echo cc); cygpath -m "$$c" 2>/dev/null || echo "$$c")
    else
        CC := $(shell command -v clang 2>/dev/null || command -v gcc 2>/dev/null || echo cc)
    endif
endif
ifeq ($(origin CXX),default)
    ifneq ($(shell command -v ccache 2>/dev/null),)
        CXX := ccache $(shell c=$$(command -v clang++ 2>/dev/null || command -v g++ 2>/dev/null || echo c++); cygpath -m "$$c" 2>/dev/null || echo "$$c")
    else
        CXX := $(shell command -v clang++ 2>/dev/null || command -v g++ 2>/dev/null || echo c++)
    endif
endif
ifeq ($(origin AR),default)
    AR := $(shell command -v ar 2>/dev/null || echo ar)
endif

# Platform detection
UNAME_S := $(shell uname -s)

# Termux (Android Linux): detect by checking Termux prefix
ifeq ($(UNAME_S),Linux)
    TERMUX_PREFIX := /data/data/com.termux/files/usr
    ifneq ("$(wildcard $(TERMUX_PREFIX)/bin/pkg)","")
        SSL_CFLAGS := -I$(TERMUX_PREFIX)/include
        SSL_LDFLAGS := -L$(TERMUX_PREFIX)/lib -lssl -lcrypto
        PLATFORM_LDFLAGS := -lz
        OS_DEF := -D__TERMUX__
    else ifeq ($(shell ldd --version 2>/dev/null | head -1 | grep -c musl || echo 0),1)
# musl/Alpine: -ldl and -lpthread are in libc — omit them
        SSL_CFLAGS := $(shell pkg-config --cflags openssl 2>/dev/null || echo "")
        SSL_LDFLAGS := $(shell pkg-config --libs openssl 2>/dev/null || echo "-lssl -lcrypto")
        PLATFORM_LDFLAGS := -lz
        OS_DEF := -D__MUSL__
    else
# Standard Linux (glibc)
        SSL_CFLAGS := $(shell pkg-config --cflags openssl 2>/dev/null || echo "")
        SSL_LDFLAGS := $(shell pkg-config --libs openssl 2>/dev/null || echo "-lssl -lcrypto")
        PLATFORM_LDFLAGS := -ldl -lpthread -lz -lpcre2-8
        PLATFORM_LDFLAGS += $(shell pkg-config --libs wayland-client wayland-egl xkbcommon gbm EGL GLESv2 2>/dev/null || echo "-lwayland-client -lwayland-egl -lxkbcommon -lgbm -lEGL -lGLESv2")
        OS_DEF :=
    endif
else ifeq ($(UNAME_S),Darwin)
    BREW_SSL := $(shell brew --prefix openssl 2>/dev/null || echo /usr/local/opt/openssl)
    SSL_CFLAGS := -I$(BREW_SSL)/include
    SSL_LDFLAGS := -L$(BREW_SSL)/lib -lssl -lcrypto
    PLATFORM_LDFLAGS := -ldl -lpthread -lz -lpcre2-8
    OS_DEF := -D__APPLE__
else
# musl/Alpine (non-Linux) or fallback
    ifeq ($(shell ldd --version 2>/dev/null | head -1 | grep -c musl || echo 0),1)
        SSL_CFLAGS := $(shell pkg-config --cflags openssl 2>/dev/null || echo "")
        SSL_LDFLAGS := $(shell pkg-config --libs openssl 2>/dev/null || echo "-lssl -lcrypto")
        PLATFORM_LDFLAGS := -lz
        OS_DEF := -D__MUSL__
    else
        SSL_CFLAGS := $(shell pkg-config --cflags openssl 2>/dev/null || echo "")
        SSL_LDFLAGS := $(shell pkg-config --libs openssl 2>/dev/null || echo "-lssl -lcrypto")
        PLATFORM_LDFLAGS := -ldl -lpthread -lz
        OS_DEF :=
    endif
endif

# Version strings for -D flags (no embedded quotes — CFLAGS adds shell-safe quoting)
HERMES_VERSION_STR := $(HERMES_VERSION)-slermes
HERMES_RELEASE_DATE_STR := $(HERMES_RELEASE_DATE)
DATADIR_STR := $(PREFIX)/share/slermes/docs

# ── Build profiles ──────────────────────────────────────────────────
#   make PROFILE=release   (default) -O3 + LTO. The shipped binary.
#   make PROFILE=dev       -O0 -g3. Fastest to build, easiest to debug and
#                           to diff behaviour against the Python oracle.
#   make PROFILE=strict    -O2 -g plus the full warning set. Use before
#                           committing new ports; catches what -Wall misses.
#   make PROFILE=analyzer  -O1 -g -fanalyzer. Static NULL-deref / OOB / leak
#                           analysis. GCC 10+; no runtime needed.
#   make PROFILE=sanitize  -O1 -g -fsanitize=address,undefined. Requires a
#                           toolchain that ships libasan/libubsan (clang or
#                           LLVM-MinGW do; the plain winlibs GCC does not) --
#                           the build says so instead of silently skipping.
PROFILE ?= release

ifeq ($(PROFILE),release)
  PROFILE_OPT := -O3
else ifeq ($(PROFILE),dev)
  PROFILE_OPT := -O0 -g3
else ifeq ($(PROFILE),strict)
  PROFILE_OPT := -O2 -g
else ifeq ($(PROFILE),analyzer)
  PROFILE_OPT := -O1 -g
else ifeq ($(PROFILE),sanitize)
  PROFILE_OPT := -O1 -g
else
  $(error unknown PROFILE '$(PROFILE)' -- use release, dev, strict, analyzer or sanitize)
endif

CFLAGS = $(PROFILE_OPT) -Wall -Werror=implicit-function-declaration -Wno-pedantic -Wno-attributes -Wno-unused-result -Wno-format-truncation -Wstringop-truncation -Wno-misleading-indentation -Wno-discarded-qualifiers -Wno-unused-parameter -Wno-missing-field-initializers -Wno-format-extra-args -Wno-comment -Wno-format-zero-length -Wno-address -Wno-maybe-uninitialized -Wno-unused-function -include hermes_win32.h -D_GNU_SOURCE -I include/compat -I include -I src -I src/cli -I src/hermes_cli -I src/cron -I src/gateway -I src/tools -I lib -I lib/libwubuoffice/src $(SSL_CFLAGS) $(OS_DEF) $(CFLAGS_EXTRA)
# PGO (Profile-Guided Optimization) support — two-phase build for max runtime speed.
# PGO typically yields 5-15% runtime speedup on C codebases (GCC docs: up to 2×).
# Slermes is heavy on JSON + DB + string processing hot paths — good PGO candidates.
# NOTE: Full PGO cycle requires 1500+ file rebuild (~3.5min) + representative workload.
# For now, -O3 + LTO gives the most practical gains without PGO overhead.
ifneq ($(PGO_INSTRUMENT),)
  CFLAGS += -fprofile-generate
endif
ifneq ($(PGO_OPTIMIZE),)
  CFLAGS += -fprofile-use -fprofile-correction
endif
CFLAGS += -DHERMES_VERSION=\"$(HERMES_VERSION_STR)\" -DHERMES_RELEASE_DATE=\"$(HERMES_RELEASE_DATE_STR)\" -DATADIR=\"$(DATADIR_STR)\"

# ── Profile-specific extras ─────────────────────────────────────────
ifeq ($(PROFILE),strict)
  CFLAGS += -Wextra -Wshadow -Wconversion -Wsign-conversion -Wnull-dereference \
            -Wdouble-promotion -Wformat=2 -Wcast-qual -Wstrict-prototypes \
            -Wmissing-prototypes -Wwrite-strings -Wundef -Wpointer-arith \
            -Wstrict-overflow=4 -Wswitch-default -Wstack-protector \
            -fstack-protector-strong
endif
ifeq ($(PROFILE),analyzer)
  CFLAGS += -fanalyzer -Wno-analyzer-malloc-leak
endif
ifeq ($(PROFILE),sanitize)
  # Refuse loudly rather than building an uninstrumented binary and letting a
  # test report CLEAN: a sanitizer that did not run is not a pass.
  SAN_RT := $(shell printf 'int main(void){return 0;}\n' > .sanprobe.c && \
             $(CC) -fsanitize=address,undefined .sanprobe.c -o .sanprobe.exe 2>/dev/null && \
             echo yes || echo no)
  ifneq ($(SAN_RT),yes)
    $(error PROFILE=sanitize needs a toolchain with ASan+UBSan runtimes. \
This one does not ship them (winlibs GCC does not). Install LLVM-MinGW \
(clang + lld, which ships both) or use PROFILE=analyzer for static checks.)
  endif
  CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer
  LDFLAGS += -fsanitize=address,undefined
endif
ifeq ($(PROFILE),dev)
  CFLAGS += -fstack-protector-strong
endif

# ── Profile guard ───────────────────────────────────────────────────
# PROFILE is not an object-file dependency, so `make PROFILE=dev` on a tree
# built with PROFILE=release would link a dev binary out of -O3 objects and
# report success. Refuse instead of silently producing the wrong thing.
PROFILE_STAMP := build/.profile

ifeq ($(PROFILE),sanitize)
  # sanitize writes the stamp only after the probe passed.
endif
ifneq ($(wildcard $(PROFILE_STAMP)),)
  PROFILE_LAST := $(shell cat $(PROFILE_STAMP) 2>/dev/null)
  ifneq ($(PROFILE_LAST),$(PROFILE))
    $(error PROFILE changed from '$(PROFILE_LAST)' to '$(PROFILE)'. \
The existing object files were compiled with the old profile, so linking them \
would silently produce a mislabelled binary. Run: make clean)
  endif
endif

$(PROFILE_STAMP):
	@mkdir -p build
	@echo $(PROFILE) > $@
LDFLAGS = $(SSL_LDFLAGS) $(PLATFORM_LDFLAGS)
LIBS = -lm
# LTO (Link-Time Optimization) — enabled for release builds:
#   make LTO=1   → adds -flto to CFLAGS + LDFLAGS, enables cross-TU inlining,
#                   dead-code elimination, and better code layout. Typically 2-5%
#                   runtime speedup + 10-30% binary size reduction on C projects.
#   GCC LTO caveat: sqlite3.o (10MB amalgamation) links slowly with LTO; the
#   linker may need more memory. If LTO link fails, drop -flto for sqlite3.o.
ifneq ($(LTO),)
  CFLAGS += -flto
  LDFLAGS += -flto
endif
# Strip binary by default (release). Debug builds disable: make STRIP=0
ifeq ($(STRIP),1)
  LDFLAGS += -s
endif

# Optional ncurses full-screen TUI (enabled when ncurses is available)
TUI_INC = -I lib/libncurses/include
TUI_LIB = lib/libncurses/lib/libncursesw.a lib/libncurses/lib/libpanelw.a
TUI_SRC = src/cli/tui_fullscreen.c src/cli/tui_eventpub.c src/cli/tui_slash_worker.c src/cli/tui_entry.c src/cli/tui_json_rpc.c src/cli/tui_transport.c src/cli/tui_layout.c src/cli/tui_render.c src/cli/tui_websocket.c
TUI_OBJ = src/cli/tui_fullscreen.o src/cli/tui_eventpub.o src/cli/tui_slash_worker.o src/cli/tui_entry.o src/cli/tui_json_rpc.o src/cli/tui_transport.o src/cli/tui_layout.o src/cli/tui_render.o src/cli/tui_websocket.o

# Auto-detect: check if local ncurses headers AND libs exist
ifneq ("$(wildcard lib/libncurses/include/curses.h)","")
    ifneq ("$(wildcard lib/libncurses/lib/libncursesw.a)","")
        HAS_NCURSES = 1
        CFLAGS += $(TUI_INC)
    else
        HAS_NCURSES = 0
    endif
else
    HAS_NCURSES = 0
endif

# ccache refuses to run without USERPROFILE. Exporting it from the shell is not
# reliable here (make/native-tool env handling drops it), so force it for every
# recipe make spawns.
export USERPROFILE ?= C:/Users/eman5
export LOCALAPPDATA ?= C:/Users/eman5/AppData/Local
