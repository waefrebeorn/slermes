# Slermes — We SLERMEd Hermes Agent into C11

We took Hermes Agent — Nous Research's Python/Electron AI agent — and **SLERMEd** it: ripped the entire thing out and rebuilt it from scratch in C11. Not a wrapper. Not a thin translation layer. A complete reimplementation that runs as a single 47 MB binary with zero Python dependency.

**SLERM** (verb): to take someone's full work and make your own version from scratch. Not a fork — a ground-up reimplementation.

## Project Status (v510)

| Metric | Value |
|--------|-------|
| **PORTED** | 8,701 / 9,731 (89.4%) |
| **REAL_GAP** | **0** — full parity |
| **CLI Commands** | 95 (all real C11) |
| **TUI RPC Methods** | 92 registered |
| **Binary Size** | ~47 MB single binary |
| **Source Files** | 352 C + 127 headers |
| **Libraries** | 73 internal |
| **Platforms** | 14 messaging gateways |

## Quick Start

```bash
git clone https://github.com/waefrebeorn/slermes
cd slermes
make -j$(nproc)
./slermes
```

**Dependencies:** `build-essential libssl-dev libwayland-dev libxkbcommon-dev`

## Documentation

| Docs | Description |
|------|-------------|
| **[Getting Started](docs/getting-started.md)** | Build, configure, run in 2 minutes |
| **[Architecture](docs/architecture/overview.md)** | System design, data flow, directory layout |
| **[CLI Commands](docs/cli/index.md)** | All 95 slash commands with examples |
| **[TUI Reference](docs/tui/index.md)** | Terminal UI layout, shortcuts, 92 RPC methods |
| **[Pet System](docs/pet/index.md)** | Petdex mascot install, configure, animate |
| **[Build System](docs/architecture/build-system.md)** | Make targets, dependencies, packaging |
| **[Development](docs/dev/index.md)** | Building, testing, contributing |
| **[Parity Summary](docs/parity-summary.md)** | Full parity scan results |

## Features

- **Pure C11** — Zero Python, zero C++, zero interpreter
- **CLI + TUI** — Full terminal interface with split-pane layout
- **Desktop GUI** — Custom C11 Wayland/EGL desktop app
- **14 Messaging Platforms** — Telegram, Discord, Slack, Signal, WhatsApp, Matrix, etc.
- **40+ Tools** — Terminal, file, web search, browser, image gen, voice, code exec
- **95 Slash Commands** — Session management, config, tools, security, gateway
- **Petdex Mascots** — Animated pixel-art pets that react to agent state
- **Skills System** — Markdown-based skill definitions
- **MCP Support** — Model Context Protocol servers
- **Cron Scheduler** — Recurring task execution
- **Plugin System** — Loadable C plugins
- **Web Dashboard** — Built-in HTTP dashboard
- **Multi-Platform** — Linux, macOS, Windows (cross-compile)

## Build Targets

```bash
make              # Build slermes binary
make install      # Install to /usr/local
make clean        # Clean artifacts
make test         # Run tests
make packaging    # Create distribution packages
```

## Quick Examples

```bash
# Session management
./slermes
/new
/model list
/model set gpt-4

# Pet mascot
/pet gallery
/pet select niko

# Configuration
/config display.pet.scale 0.5
/config display.pet.enabled true
```

## License

See [THIRD_PARTY.md](THIRD_PARTY.md) for third-party attributions.
