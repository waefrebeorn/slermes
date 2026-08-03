# ── Build Targets ──────────────────────────────────────────────────
# Binary link targets: slermes, phase targets, desktop, tui, static, fuzz, desktop-gui
# Included by top-level Makefile. Expected vars: CC, CFLAGS, LDFLAGS, PLATFORM_LDFLAGS, LIBS

.PHONY: all phase1 phase2 phase3 phase4 phase5 libs tui desktop desktop-gui web_server static fuzz

all: phase5

# Phase targets enforce ordering for link-time dependencies only (objects compile in parallel via -j)
phase1: $(PHASE1_OBJ)
	@echo "Phase 1 complete: $(words $(PHASE1_OBJ)) objects"

phase2: $(PHASE2_OBJ)
	@echo "Phase 2 complete: $(words $(PHASE2_OBJ)) objects"

phase3: $(PHASE3_OBJ)
	@echo "Phase 3 complete: $(words $(PHASE3_OBJ)) objects"

phase4: $(PHASE4_OBJ)
	@echo "Phase 4 complete: $(words $(PHASE4_OBJ)) objects"

phase5: slermes
	@echo "Phase 5 complete: slermes binary built"

# Main binary link
WHISPER_LIBS := $(wildcard lib/whisper_cpp/lib/libwhisper.a lib/whisper_cpp/lib/libggml.a lib/whisper_cpp/lib/libggml-base.a lib/whisper_cpp/lib/libggml-cpu.a)
# When whisper is available: replace stubs with real wrapper; otherwise keep stubs
# C11 Whisper: pure C11 replacement for whisper.cpp C++ wrapper
C11_WHISPER_OBJ = lib/c11_whisper/c11_whisper_model.o lib/c11_whisper/c11_whisper_math.o lib/c11_whisper/c11_whisper_encoder.o lib/c11_whisper/c11_whisper_compat.o
WHISPER_EXTRA_OBJ := $(if $(WHISPER_LIBS),$(C11_WHISPER_OBJ))
LIB_OBJ_FILTERED := $(if $(WHISPER_LIBS),$(filter-out lib/whisper_cpp/whisper_stubs.o,$(LIB_OBJ)),$(LIB_OBJ))
# Detect system ncurses libs (full paths — app_desktop.o always needs them)
NCURSES_LIBS := $(wildcard /usr/lib/x86_64-linux-gnu/libncursesw.so /usr/lib/x86_64-linux-gnu/libncursesw.so.6)
PANEL_LIBS   := $(wildcard /usr/lib/x86_64-linux-gnu/libpanelw.so /usr/lib/x86_64-linux-gnu/libpanelw.so.6)
TINFO_LIBS   := $(wildcard /usr/lib/x86_64-linux-gnu/libtinfo.so /usr/lib/x86_64-linux-gnu/libtinfo.so.6)
slermes: $(PHASE5_OBJ) src/main.o $(HERMES_CLI_PORT_OBJ) $(HERMES_CLI_PORT_EXTRA_OBJ) $(PORT_OBJ) $(PET_OBJ) $(DESKTOP_CORE_OBJ) $(LIB_OBJ_FILTERED) $(WHISPER_EXTRA_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(PLATFORM_LDFLAGS) $(LIBS) -lasound -lstdc++ \
		$(if $(NCURSES_LIBS),$(NCURSES_LIBS)) $(if $(PANEL_LIBS),$(PANEL_LIBS)) $(if $(TINFO_LIBS),$(TINFO_LIBS)) \
		$(if $(WHISPER_LIBS),$(WHISPER_LIBS))
	@echo "  slermes binary: $@ $$(ls -lh $@ | awk '{print $$5}')$(if $(WHISPER_LIBS), with whisper, without whisper)"

# ncurses full-screen TUI build (shared libs from system)
TUI_LIB_OBJ = $(filter-out lib/libcurses_widget/curses_widget.o, $(LIB_OBJ_FILTERED)) lib/libcurses_widget/curses_widget_tui.o
tui: $(PHASE5_OBJ) $(TUI_OBJ) src/main.o $(HERMES_CLI_PORT_OBJ) $(HERMES_CLI_PORT_EXTRA_OBJ) $(PORT_OBJ) $(PET_OBJ) $(DESKTOP_CORE_OBJ) src/chat_render.o src/chat_composer.o $(TUI_LIB_OBJ) $(WHISPER_EXTRA_OBJ)
	$(CC) $(CFLAGS) -DHAS_NCURSES_TUI -o slermes-tui $^ $(LDFLAGS) $(PLATFORM_LDFLAGS) $(LIBS) -lasound \
		-L lib/syslib -L lib/libncurses/lib \
		$(if $(NCURSES_LIBS),$(NCURSES_LIBS),-lncursesw) $(if $(PANEL_LIBS),$(PANEL_LIBS)) $(if $(TINFO_LIBS),$(TINFO_LIBS),-ltinfo) -lstdc++ \
		$(if $(WHISPER_LIBS),$(WHISPER_LIBS))
	@echo "slermes-tui built with ncurses TUI support"

# C11 Desktop App build (ncurses-based, replaces Electron/TS shell)
desktop: $(DESKTOP_APP_OBJ) $(filter-out $(DESKTOP_LIBS_FILTER), $(LIB_OBJ_FILTERED)) $(WHISPER_EXTRA_OBJ)
	$(CC) $(CFLAGS) -o hermes-desktop $^ $(LDFLAGS) $(PLATFORM_LDFLAGS) $(LIBS) \
		/usr/lib/x86_64-linux-gnu/libncursesw.so.6 /usr/lib/x86_64-linux-gnu/libtinfo.so.6 -lpanelw -lstdc++ \
		$(if $(WHISPER_LIBS),$(WHISPER_LIBS))
	@echo "hermes-desktop built — C11 Desktop App (replaces Electron shell)"

# Custom GUI desktop (SDL2-based, our own framework)
# Mirrors the ncurses `desktop` target: full SDL GUI application object set +
# the shared lib objects, so all app_state/session_db/sidebar/chat_view/titlebar/
# event_handling/hud/desktop_controller/pet_ui symbols resolve.
# The pet subsystem's from-scratch PNG decoder (libpngdec) rides on wubuzip's
# DEFLATE inflate + CRC32 — those objects are in DEPS_OBJ, so the GUI links
# them explicitly.
WUBUZIP_OBJ = lib/libwubuoffice/src/wubuzip/inflate.o lib/libwubuoffice/src/wubuzip/huffman.o \
    lib/libwubuoffice/src/wubuzip/crc.o lib/libwubuoffice/src/wubuzip/bit.o \
    lib/libwubuoffice/src/wubuzip/block.o lib/libwubuoffice/src/wubuzip/fixed.o \
    lib/libwubuoffice/src/wubuzip/canon.o lib/libwubuoffice/src/wubuzip/fixedcode.o \
    lib/libwubuoffice/src/wubuzip/limitcode.o

desktop-gui: $(DESKTOP_GUI_OBJ) $(PET_OBJ) $(WUBUZIP_OBJ) $(filter-out $(DESKTOP_LIBS_FILTER), $(LIB_OBJ_FILTERED)) $(WHISPER_EXTRA_OBJ)
	$(CC) $(CFLAGS) $(DESKTOP_GUI_CFLAGS) -o slermes-desktop-gui $^ $(LDFLAGS) $(PLATFORM_LDFLAGS) $(LIBS) \
		$(DESKTOP_GUI_LIBS) -lstdc++ \
		$(if $(NCURSES_LIBS),$(NCURSES_LIBS),-lncursesw) $(if $(PANEL_LIBS),$(PANEL_LIBS)) $(if $(TINFO_LIBS),$(TINFO_LIBS),-ltinfo) \
		$(if $(WHISPER_LIBS),$(WHISPER_LIBS))
	@echo "slermes-desktop-gui built — custom graphical desktop app"

# Standalone web server binary (src/web_server.c — its own main()).
# Self-contained HTTP/WS implementation; pulls in sqlite3 + openssl + json +
# http + crypto + base64 + skills libs, plus a few slermes core helpers.
WEB_SERVER_OBJ := src/web_server.o src/slermes_home.o src/skills/skills_parser.o src/cli/paths.o
web_server: $(WEB_SERVER_OBJ) $(filter-out lib/libtranscribe/transcribe.o, $(LIB_OBJ_FILTERED)) $(WHISPER_EXTRA_OBJ)
	$(CC) $(CFLAGS) -o web_server $^ $(LDFLAGS) $(PLATFORM_LDFLAGS) $(LIBS) \
		-lssl -lcrypto -lz -lm -lpthread -lstdc++ \
		$(if $(WHISPER_LIBS),$(WHISPER_LIBS))
	@echo "web_server built — standalone Slermes web/dashboard server"

.PHONY: web_server

# Static linking target — single binary with no runtime deps beyond kernel
static: CFLAGS += -static -Os -s
static: LDFLAGS += -static
static: $(PHASE5_OBJ) src/main.o $(LIB_OBJ)
	$(CC) $(CFLAGS) -o slermes-static $^ $(LDFLAGS) -lssl -lcrypto -ldl -lpthread -lz -lm 2>/dev/null \
	  || (echo "NOTE: Static build requires static versions of libssl/libcrypto."; \
	      echo "On Debian/Ubuntu: apt-get install libssl-dev (provides .a files)"; \
	      echo "On Alpine: apk add openssl-dev (static by default)"; \
	      $(CC) $(CFLAGS) -o slermes-static $^ $(LDFLAGS) $(LIBS) -static 2>/dev/null \
	      && echo "Static build succeeded (dynamic openssl fallback)" \
	      || echo "Static build failed — ensure static libraries are installed")
	@test -f slermes-static && echo "Static binary: slermes-static ($(shell ls -lh slermes-static | awk '{print $$5}'))" || true

# Fuzz test harness
fuzz: CFLAGS += -DFUZZ_TESTING -O1 -g
fuzz: tests/fuzz_harness.o $(LIB_OBJ)
	$(CC) $(CFLAGS) $(LIB_INCS) -o slermes-fuzz tests/fuzz_harness.o $(LIB_OBJ) $(LDFLAGS) $(LIBS)
	@echo "Fuzz harness built — run with: ./slermes-fuzz"
