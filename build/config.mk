# ── Build Configuration ─────────────────────────────────────────────
# Version detection, compiler selection, platform detection, CFLAGS/LDFLAGS
# Included by top-level Makefile. Expected vars: HERMES_VERSION, HERMES_RELEASE_DATE

# Dynamically detect Python Hermes version from upstream source
HERMES_VERSION := $(shell python3 -c "import sys; sys.path.insert(0, '..'); from hermes_cli import __version__; print(__version__)" 2>/dev/null || echo "0.15.1")
HERMES_RELEASE_DATE := $(shell python3 -c "import sys; sys.path.insert(0, '..'); from hermes_cli import __release_date__; print(__release_date__)" 2>/dev/null || echo "2026.5.29")

# ── Cross-distro build setup ─────────────────────────────────────────────
# Auto-detect compiler: prefer clang on macOS/FreeBSD, gcc on Linux
# Override with: make CC=clang (or CC=gcc)
ifeq ($(origin CC),default)
    CC := $(shell command -v clang 2>/dev/null || command -v gcc 2>/dev/null || echo cc)
endif
ifeq ($(origin CXX),default)
    CXX := $(shell command -v clang++ 2>/dev/null || command -v g++ 2>/dev/null || echo c++)
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

CFLAGS = -O2 -g -Wall -Werror=implicit-function-declaration -Wno-pedantic -Wno-attributes -Wno-unused-result -Wno-format-truncation -Wstringop-truncation -Wno-misleading-indentation -Wno-discarded-qualifiers -Wno-unused-parameter -Wno-missing-field-initializers -Wno-format-extra-args -Wno-comment -Wno-format-zero-length -Wno-address -Wno-maybe-uninitialized -Wno-unused-function -I include -I src $(SSL_CFLAGS) $(OS_DEF) $(CFLAGS_EXTRA)
CFLAGS += -DHERMES_VERSION=\"$(HERMES_VERSION_STR)\" -DHERMES_RELEASE_DATE=\"$(HERMES_RELEASE_DATE_STR)\" -DATADIR=\"$(DATADIR_STR)\"
LDFLAGS = $(SSL_LDFLAGS) $(PLATFORM_LDFLAGS)
LIBS = -lm

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
