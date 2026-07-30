# Slermes — We SLERMEd Hermes Agent into C11

We took Hermes Agent — Nous Research's Python/Electron AI agent — and **SLERMed** it: started ripping the whole thing out and rebuilding it from scratch in C11. Not a wrapper. Not a thin translation layer. A ground-up reimplementation that runs as a single native binary with zero Python dependency. It's an in-progress port — see [Parity Summary](docs/parity-summary.md) for exactly how much is done.

**SLERM** (verb): to take someone's full work and make your own version from scratch. Not a fork — a ground-up reimplementation.

## What this is

<table>
<tr><td><b>A real terminal interface</b></td><td>Full TUI with multiline editing, slash-command autocomplete, conversation history, interrupt-and-redirect, and streaming tool output.</td></tr>
<tr><td><b>Lives where you do</b></td><td>Telegram, Discord, Slack, WhatsApp, Signal, and CLI — all from a single gateway process. Voice memo transcription, cross-platform conversation continuity.</td></tr>
<tr><td><b>A closed learning loop</b></td><td>Agent-curated memory with periodic nudges. Autonomous skill creation after complex tasks. Skills self-improve during use. FTS5 session search with LLM summarization for cross-session recall. <a href="https://github.com/plastic-labs/honcho">Honcho</a> dialectic user modeling. Compatible with the <a href="https://agentskills.io">agentskills.io</a> open standard.</td></tr>
<tr><td><b>Scheduled automations</b></td><td>Built-in cron scheduler with delivery to any platform. Daily reports, nightly backups, weekly audits — all in natural language, running unattended.</td></tr>
<tr><td><b>Delegates and parallelizes</b></td><td>Spawn isolated subagents for parallel workstreams. Write Python scripts that call tools via RPC, collapsing multi-step pipelines into zero-context-cost turns.</td></tr>
<tr><td><b>Runs anywhere, not just your laptop</b></td><td>Seven terminal backends — local, Docker, SSH, Singularity, Modal, Daytona, and Vercel Sandbox. Daytona and Modal offer serverless persistence — your agent's environment hibernates when idle and wakes on demand, costing nearly nothing between sessions. Run it on a $5 VPS or a GPU cluster.</td></tr>
<tr><td><b>Research-ready</b></td><td>Batch trajectory generation, trajectory compression for training the next generation of tool-calling models.</td></tr>
</table>

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

---

## License

This project is licensed under the **Waefrebeorn Umbrella License v3.0**.
See the [LICENSE](LICENSE) file for the full license text.

The Waefrebeorn Umbrella License is a custom source-available license.
It is not OSI-approved and not FSF-approved.
