# Slermes — We SLERMEd Hermes Agent into C11

We took Hermes Agent — Nous Research's Python/Electron AI agent — and **SLERMed** it: started ripping the whole thing out and rebuilding it from scratch in C11. Not a wrapper. Not a thin translation layer. A ground-up reimplementation that runs as a single native binary with zero Python dependency. It's an in-progress port — see [Parity Summary](docs/parity-summary.md) for exactly how much is done.

**SLERM** (verb): to take someone's full work and make your own version from scratch. Not a fork — a ground-up reimplementation.

## What this is

A ground-up C11 reimplementation of Hermes Agent — no Python, no C++, no
interpreter. It builds to a single native binary. It is a **work in progress**:
large parts of the agent are ported and run, and large parts are not yet ported.
For the exact, live breakdown of what is and isn't done, see
**[Parity Summary](docs/parity-summary.md)**, which is regenerated from the
scanner (`tests/slermes_parity_battleground.py`) and is the single source of
truth for completeness. Do not trust any completion percentage that isn't in
that file.

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
| **[CLI Commands](docs/cli/index.md)** | Slash commands with examples |
| **[TUI Reference](docs/tui/index.md)** | Terminal UI layout, shortcuts, RPC methods |
| **[Pet System](docs/pet/index.md)** | Petdex mascot install, configure, animate |
| **[Build System](docs/architecture/build-system.md)** | Make targets, dependencies, packaging |
| **[Development](docs/dev/index.md)** | Building, testing, contributing |
| **[Parity Summary](docs/parity-summary.md)** | Live parity scan results (what's ported vs. not) |

## Features

- **Pure C11** — Zero Python, zero C++, zero interpreter
- **CLI + TUI** — Full terminal interface with split-pane layout
- **Desktop GUI** — Custom C11 Wayland/EGL desktop app
- **14 Messaging Platforms** — Telegram, Discord, Slack, Signal, WhatsApp, Matrix, etc.
- **40+ Tools** — Terminal, file, web search, browser, image gen, voice, code exec
- **Slash Commands** — Session management, config, tools, security, gateway
- **Petdex Mascots** — Animated pixel-art pets that react to agent state
- **Skills System** — Markdown-based skill definitions
- **MCP Support** — Model Context Protocol servers
- **Cron Scheduler** — Recurring task execution
- **Plugin System** — Loadable C plugins
- **Web Dashboard** — Built-in HTTP dashboard
- **Multi-Platform** — Linux, macOS, Windows (cross-compile)

> Feature coverage is partial and evolving. Some features above are fully
> ported, others are in progress — check [Parity Summary](docs/parity-summary.md)
> for the current per-module state before relying on any one of them.

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
