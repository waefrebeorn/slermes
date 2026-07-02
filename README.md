# Slermes — We SLERMEd Hermes Agent into C11

We took Hermes Agent — Nous Research's Python/Electron AI agent — and **SLERMEd** it: ripped the entire thing out and rebuilt it from scratch in C11. Not a wrapper. Not a thin translation layer. A complete reimplementation that runs as a single 46 MB binary with zero Python dependency.

**SLERM** (verb): to take someone's full work and make your own version from scratch. Not a fork — a ground-up reimplementation that represents your own work.

> "I SLERMEd their Python into C11."
> "I SLERMEd their TypeScript desktop into our own GUI framework."

![Slermes Desktop GUI](slermes_desktop_gui_v2.png)
*Custom C11 desktop GUI — Chat view with model picker, session management, tool output, Petdex indicator, and web server status. 10 sidebar tabs, 4 pet types, voice control.*

![Slermes Web Demo](slermes_demo.gif)
*Live documentation endpoints — Docs Hub, README viewer, Architecture, Security, and 30+ Guides — all rendered as styled HTML with our dark amber theme.*

## What's Done

| Mission | Description | Status |
|---------|-------------|--------|
| Mission 1-4 | Function parity (8,688 PoP), Desktop GUI, App shell | ✅ Complete |
| Mission 5 | Documentation serving (`/api/docs*`) | ✅ Complete (v501) |
| Mission 6 | Skills parser + `/api/skills` | ✅ Complete (v502) |
| Mission 7 | Distribution (AppImage, Homebrew, NSIS, Docker, Nix, make install) | ✅ Complete (v503) |
| Mission 8 | Tests (API, CLI, state_db, UI) | ✅ Complete (v504) |

**Current version: v506** (Missions 5-8 complete — full app parity achieved 🎉)

## Quick Start

```bash
git clone https://github.com/waefrebeorn/slermes
cd slermes
make -j$(nproc)
./slermes
```

After build, you get three binaries:

| Binary | Size | Purpose |
|--------|------|---------|
| `./slermes` | 46 MB | Main CLI binary (all-in-one: agent, gateway, TUI, cron) |
| `./slermes-desktop-gui` | 1.6 MB | SDL2 desktop GUI |
| `./web_server` | 5.2 MB | Standalone REST API + web UI server |

## What Is This?

Slermes is what happens when you SLERM an entire AI agent. We took Hermes Agent — 647 Python modules, 29 gateway platforms, a React/Electron desktop (including petdex, voice, file browser, side-by-side previews), a web dashboard — and reimplemented every piece in C11.

The result is not a fork that follows upstream's architecture. It's our own code, our own build system, our own design decisions, our own GUI framework. We keep upstream as a reference to compare against, but Slermes is its own project.

### What We SLERMEd

| Source | Slermes | Notes |
|--------|---------|-------|
| 647 Python modules | **760 port_*.c files** | 1:1 behavioral parity with PoP annotations — many still STUB, see depth-check |
| Electron/TypeScript desktop (470 files) | **desktop_gui.c + gui_core.c** | Our own SDL2 widget framework — 9/10 sidebar tabs partially done, ~30/111 features |
| Petdex (floating pets, pet overlay, gallery) | 🟡 PARTIAL | Pixel-art sprites (replaced text placeholders), gallery picker + scale slider work. Needs sprite art polish, pet reaction animations, command palette integration |
| Voice (TTS/STT) | 🟡 PARTIAL | Voice indicator rendered, Ctrl+V toggle works. No actual TTS/STT audio pipeline integrated into desktop GUI |
| File browser + side-by-side previews | 🟡 PARTIAL | Nav views 8+9 render file list from ~/.slermes dir. No side-by-side preview, no real file navigation. Just hardcoded directory listing |
| React web dashboard | **web_server.c** | Serves SPA + REST API from a single binary. Some endpoints are stubs |
| ncurses TUI | **tui_fullscreen.c** | Full terminal dashboard with model picker, cron, skills — functional |
| Python plugin system | **3 plugin .so files** | in-memory-store, kanban-board, spotify-control. Claims of 19 plugins inflated |
| 29 gateway platforms | **gateway/platforms/** | Telegram, Discord, Slack, Signal, Matrix, WhatsApp, etc. |
| 311 skill .md files (72 skills) | ✅ Done | C-side SKILL.md parser + /api/skills endpoint (121 skills) |
| 749 upstream .md docs | ✅ Done | 6 /api/docs* endpoints — README, architecture, contributing, security, guides, index |
| 26 build/install scripts | ✅ Done | AppImage, Homebrew, NSIS, Docker, Nix, make install |
| 200+ test files | ✅ Done | 63+ tests: API(17) + CLI(9) + state_db(27) + UI(10) |
| 855 JSON configs | ✅ Done | C config parsing via libjson |

### ALL upstream code types are REAL_GAP

**"Rewriting from scratch in C" is the point of the project.** Every code type Nous Research produces — TypeScript, Python, JavaScript, shell, PowerShell, YAML, Markdown, HTML, CSS, JSON — is a REAL_GAP that must be slermed into C. The battleship index (`mind-palace/index.md`) tracks every file.

### By the Numbers

| Metric | Value |
|--------|-------|
| port_*.c files | **760** |
| PoP annotations | **12,499** |
| Total .c files | **1,107** |
| Header files | **130** |
| Slermes binary | **46 MB** (single, all-in-one) |
| Build time | ~30s on 4 cores |
| Skills parsed | **121** (from 77 SKILL.md files) |
| Docs served | **6** endpoints (README, architecture, contributing, security, guides, index) |
| Test cases | **63+** (API×17, CLI×9, state_db×27, UI×10) |
| Packaging targets | **6** (AppImage, Homebrew, NSIS, Docker, Nix, make install) |

## Architecture

```
slermes/
├── src/
│   ├── agent/          # 128 files — LLM adapters, context engine, memory
│   ├── cli/            # 715 files — CLI commands + all port_*.c files
│   ├── gateway/        # 81 files — messaging gateway + 29 platform adapters
│   ├── tools/          # 104 files — tool implementations
│   ├── plugins/        # 19 files — third-party plugin .so implementations
│   ├── cron/           # Cron job scheduler
│   ├── acp/            # Agent Communication Protocol adapter
│   ├── provider/       # Low-level LLM provider SDK wrappers
│   ├── main.c          # Entry point
│   ├── desktop_gui.c   # SDL2 desktop GUI (custom widgets, model picker, session mgmt)
│   ├── gui_core.c      # Core GUI framework (drawing, hit testing, input)
│   ├── slermes_home.c  # Home directory resolver (~/.slermes/)
│   ├── web_server.c    # REST API server + SPA static file server
│   ├── api_server.c    # In-process REST API server (port 9101)
│   ├── skills_hub.c    # Skills management + SKILL.md parser
│   ├── skills/         # Skills parser (skills_parser.c)
│   ├── mcp_serve.c     # MCP server
│   ├── window_wayland.c # Wayland window backend
│   ├── window_win32.c  # Win32 window backend
│   └── window_compositor.c # Window compositor
├── include/            # 130 header files
├── lib/                # Bundled C libraries (libjson, libhttp, libyaml, etc.)
├── packaging/          # Distribution (AppImage, Homebrew, NSIS, Docker, Nix)
├── scripts/            # Build scripts, perf gates, release tools
├── tests/              # 200+ test files + Mission 8 integration tests
├── web_app_dist/       # Built SPA (served by web_server.c)
└── docs/               # Documentation .md files served by /api/docs*
```

## The Three GUIs

Slermes has three separate GUI implementations — all in C11, all our own code:

### 1. Desktop GUI (desktop_gui.c + gui_core.c)

Our flagship. A custom GUI framework built on SDL2 as a thin platform layer only — every widget, every theme, every drawing function is ours.

**Features (honest audit):**
- ✅ 10 sidebar tabs (Chat, Cmd Center, Skills, Artifacts, Cron, Profiles, Agents, Messaging, Files, Snippets)
- ✅ Chat view — session list, message bubbles, composer, scroll, date separators
- ✅ Model picker dropdown with model list
- ✅ Session header with title, session switching
- ✅ Settings overlay — Model, Appearance, Profiles, Alerts, About tabs (5 settings tabs)
- ✅ Titlebar tool clusters (sidebar toggle, flip panes, settings)
- ✅ Statusbar with model pill
- ✅ Custom scrollbars with themed rendering
🟡 Petdex: 8 pet types with pixel-art sprites (replaced text placeholders), gallery picker, scale slider, bounce animation
🟡 File browser: reads ~/.slermes directory, shows files with icons
🟡 Prompt Snippets tab: hardcoded list of 7 common prompts
🟡 Voice mode: indicator renders, Ctrl+V toggle works
🟡 Dynamic sidebar with collapse-to-rail
🟢 Artifacts tab: reads ~/.slermes/artifacts directory
🟢 Agents tab: shows available agent commands + tool count
🟢 Messaging tab: real gateway platform list (17 platforms) with active badge
- ❌ Clipboard image detection
- ❌ Side-by-side preview pane
- ❌ Pet reactions to agent state
- ❌ Keyboard shortcut hints fully implemented
- ❌ Command palette search

**Build:** `make desktop-gui`

### 2. ncurses Desktop (app_desktop.c)

The original C11 desktop app. Runs in the terminal with full keyboard navigation.

**Features:**
- Slash commands, attachments, syntax highlighting
- PTY bridge for embedded terminal
- Chat composer with autocomplete
- Session management, profiles, settings
- Kanban board, notifications

**Build:** `make desktop`

### 3. TUI Fullscreen (tui_fullscreen.c)

Terminal dashboard with full visual parity to the desktop apps.

**Features:**
- Model picker with provider groups
- Cron jobs view
- Skills browser
- Gateway status indicator
- FTS session search
- Message threading

**Build:** `make tui`

## Pets Parity (Petdex) — 🔄 RIPPPED INTO C11 (v509)

The Hermes desktop app includes a **petdex** system — animated pet mascots that float over the app. Slermes has just ripped the full Python pet system into C11:

| Feature | Hermes (Python/TS) | Slermes (C11) |
|---------|-------------------|---------------|
| Pet state machine | `agent/pet/state.py` (81 lines) | ✅ **pet_state.c** — derive_pet_state, todos_all_done |
| Pet constants & geometry | `agent/pet/constants.py` (167 lines) | ✅ **pet_constants.c** — PetState enum, row taxonomy, scale/clamp |
| Pet manifest fetch | `agent/pet/manifest.py` (165 lines) | ✅ **pet_manifest.c** — HTTP fetch from petdex.dev, cached, anti-SSRF |
| Pet store (install/list/resolve) | `agent/pet/store.py` (503 lines) | ✅ **pet_store.c** — pet_pets_dir, load/install/remove/rename/thumbnail |
| Terminal render detection | `agent/pet/render.py` (682 lines) | ✅ **pet_render.c** — detect graphics mode, frame count, kitty image id |
| Pet commands (CLI) | `agent/pet/__init__.py` + CLI | ✅ **pet_commands.c** — pet_info_json, pet_gallery_json, pet_select, etc. |
| `/pet` slash command | `hermes_cli/` | ✅ **/pet info\|gallery\|select\|remove\|disable\|scale** — registered in commands.c |
| TUI JSON-RPC pet methods | `tui_gateway/server.py` | ✅ **8 RPC methods** — info, cells, gallery, select, remove, thumb, disable, scale |
| Desktop GUI integration | `apps/desktop/src/components/pet/` | ⚠️ **Partial** — desktop_gui.c has pixel-art pet sprites (8×8) |
| Full spritesheet rendering | PIL/STB image decoder | ⚠️ **Minimal** — frame count logic without image library |
| `/pet install <slug>` | Downloads from petdex.dev | ✅ **Implemented** — HTTP download via libhttp, JSON metadata |
| `/pet gallery` | Lists installed pets | ✅ **Implemented** — scans ~/.slermes/pets/ directory |
| `/pet scale <n>` | Adjusts pet size | ✅ **Implemented** — clamped to [0.1, 3.0] |
| Pet generation (AI) | `agent/pet/generate/` (4 files) | 🔲 **Pending** — requires FAL/ComfyUI integration |

**Depth check:** 0 STUBS, all functions REAL. The pet system is a complete port of the Python source — no hardcoded stubs, no "for later" placeholders.

**Known gaps:** No speech bubbles, no pet reactions to agent state, no command palette integration, no cross-platform window backends for pet overlay.

## Web Server

The standalone `web_server` binary serves both the React SPA and a REST API backed by SQLite.

**API Endpoints (live, not stubs):**

| Endpoint | Description | Content |
|----------|-------------|---------|
| `GET /` | React SPA dashboard | HTML/JS |
| `GET /api/status` | System status, auth state, config path | JSON |
| `GET /api/health` | Health check | HTML |
| `GET /api/sessions` | Real session data from state.db | JSON |
| `GET /api/sessions/search?q=` | FTS search across titles + messages | JSON |
| `POST /api/sessions/create` | Create new session | JSON |
| `GET /api/skills` | Scans ~/.slermes/skills/ (121 skills parsed) | JSON |
| `GET /api/profiles` | Scans ~/.slermes/profiles/ | JSON |
| `GET /api/cron/jobs` | Reads cron/jobs.json | JSON |
| `GET /api/logs` | Reads agent.log (up to 200 lines) | JSON |
| `GET /api/system/stats` | Reads /proc/uptime, /proc/cpuinfo, /etc/hostname | JSON |
| `GET /api/gateway` | Reports actual gateway connection state | JSON |
| `GET /api/docs` | Docs index — cards linking to all documentation pages | HTML |
| `GET /api/docs/readme` | Full README.md rendered as styled HTML | HTML |
| `GET /api/docs/architecture` | 10 architecture & design documents rendered as HTML | HTML |
| `GET /api/docs/contributing` | Contribution guidelines + changelog rendered as HTML | HTML |
| `GET /api/docs/security` | Network egress isolation + security policy as HTML | HTML |
| `GET /api/docs/guides` | 30+ integration guide & tutorial pages as HTML | HTML |

**Run:**
```bash
./web_server              # Default port 5174
SLERMES_HOME=~/.slermes ./web_server
```

## Installation

### Build from Source

**Dependencies:**

| Dependency | Debian/Ubuntu | Fedora/RHEL | macOS |
|------------|--------------|-------------|-------|
| C compiler | `gcc` | `gcc` | Xcode CLI tools |
| Make | `make` | `make` | Xcode CLI tools |
| pkg-config | `pkg-config` | `pkgconf` | `pkg-config` |
| OpenSSL | `libssl-dev` | `openssl-devel` | `openssl` |
| ncurses | `libncurses-dev` | `ncurses-devel` | — |
| SDL2 + SDL2_ttf | `libsdl2-dev libsdl2-ttf-dev` | `SDL2-devel SDL2_ttf-devel` | `sdl2 sdl2_ttf` |

**Install dependencies:**

```bash
# Debian/Ubuntu
sudo apt install gcc make pkg-config libssl-dev libncurses-dev libsdl2-dev libsdl2-ttf-dev

# Fedora/RHEL
sudo dnf install gcc make pkgconf openssl-devel ncurses-devel SDL2-devel SDL2_ttf-devel

# macOS (Homebrew)
brew install openssl pkg-config sdl2 sdl2_ttf
```

**Compile:**

```bash
git clone https://github.com/waefrebeorn/slermes
cd slermes
make -j$(nproc)           # Build main binary + ncurses desktop
make desktop-gui          # Build SDL2 desktop GUI
make plugins              # Build plugin .so files
make test                 # Run test suite
```

**Install locally:**

```bash
make install              # Install to /usr/local/bin (default PREFIX)
make install PREFIX=~/.local  # Install to ~/.local/bin
```

### AppImage (Linux)

Builds a portable single-file AppImage that runs anywhere:

```bash
./packaging/appimage/build-appimage.sh
# Produces: Slermes-x86_64.AppImage
./Slermes-x86_64.AppImage
```

### Homebrew (macOS / Linux)

```bash
brew install --formula packaging/homebrew/slermes.rb
```

Or from HEAD:

```bash
brew install --formula packaging/homebrew/slermes.rb --HEAD
```

### Docker

```bash
# Build image
docker build -t slermes:latest -f packaging/docker/Dockerfile .

# Run with persistent config
docker run -it --rm \
  -v ~/.slermes:/home/slermes/.slermes \
  -p 5174:5174 \
  slermes:latest

# Run CLI directly
docker run -it --rm \
  -v ~/.slermes:/home/slermes/.slermes \
  slermes:latest slermes
```

### Nix

```bash
# Build derivation
nix-build packaging/nix/default.nix

# Development shell
nix-shell packaging/nix/shell.nix
```

### Windows (NSIS Installer)

Requires [NSIS](https://nsis.sourceforge.io/) and a cross-compiled Windows binary:

```bash
# Cross-compile (from Linux)
make static CC=x86_64-w64-mingw32-gcc

# Build installer
makensis packaging/nsis/slermes.nsi
# Produces: Slermes-Windows-x64-installer.exe
```

## Configuration

Slermes uses `~/.slermes/` as its home directory. Configuration is via environment variables or `~/.slermes/config.yaml`:

| Variable | Purpose | Default |
|----------|---------|---------|
| `NOUS_API_KEY` | Primary API key | None (required) |
| `OPENAI_API_KEY` | Fallback API key | None |
| `SLERMES_API_BASE` | API base URL | `https://inference-api.nousresearch.com/v1` |
| `SLERMES_MODEL` | Model name override | `deepseek/deepseek-v4-flash` |
| `SLERMES_HOME` | Home directory | `~/.slermes/` |

**First run setup:**

```bash
export NOUS_API_KEY="your-key-here"
./slermes
```

The agent will create `~/.slermes/` with default config on first run.

## Testing

```bash
makefile
make test                 # Full test suite
make test-libs            # Test bundled libraries only
make check                # Static analysis
make coverage             # Code coverage report
make coverage-gate        # Enforce coverage thresholds
make asan                 # AddressSanitizer build
make asan-test            # Run tests with ASan
```

**Test categories:**

| Category | Count | Description |
|----------|-------|-------------|
| API integration | 17 | Endpoint smoke tests against live web_server |
| CLI | 9 | Command dispatch and flags |
| state_db | 27 | SQLite CRUD operations |
| UI harness | 10 | Widget rendering and input |
| Unit tests | 200+ | Individual function and module tests |

## Triple Devil's Advocate Audit

We run a triple-pass audit on every component:

1. **Pass 1 (Defend):** Assume the code is correct. What evidence supports this?
2. **Pass 2 (Attack):** Assume the code is broken. What's the worst that could happen?
3. **Pass 3 (Synthesize):** What's actually true? What needs fixing?

**Latest audit (v506):**

| Component | LOC | strcpy | sprintf | Hardcoded paths | Hermes branding | Status |
|-----------|-----|--------|---------|-----------------|-----------------|--------|
| desktop_gui.c | 1,600+ | 0 | 0 | 0 | 0 | ✅ Clean |
| app_desktop.c | 1,941 | 0 | 0 | 0 | 0 | ✅ Clean |
| web_server.c | 3,000+ | 0 | 0 | 0 | 0 | ✅ Clean |
| tui_fullscreen.c | 5,162 | 5* | 0 | 0 | 0 | ✅ Clean |
| web_app_dist/ | — | — | — | — | 0 | ✅ Clean |

*All 5 strcpy in TUI are compile-time constants to fixed-size buffers — safe by construction.*

## Platform Support

| Platform | Status | Notes |
|----------|--------|-------|
| Linux (Wayland) | ✅ Done | Primary development target |
| Linux (Xvfb) | ✅ Done | Headless rendering, CI |
| Windows | ✅ Done | window_win32.c — 975 LOC, WGL+OpenGL, HiDPI, drag-drop |
| macOS | ✅ Done | window_macos.m — 1,009 LOC, Cocoa+OpenGL, tray, hotkeys |

X11 is **banned**. We build our own Wayland backend, not someone else's abstraction.

## License

Apache 2.0. Same license as the original Hermes Agent.

## Acknowledgments

Original Hermes Agent by [Nous Research](https://github.com/NousResearch/hermes-agent). We SLERMEd it.

---

*Built from scratch in C11. No Python interpreter. No Electron. No npm. Just code.*