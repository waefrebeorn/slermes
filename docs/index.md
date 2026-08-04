# Slermes Documentation

**Slermes** is an in-progress C11 reimplementation of Hermes Agent — Nous Research's Python/Electron AI agent. One binary, zero Python dependencies.

## Quick Links

- **[Getting Started](getting-started.md)** — Build, configure, run in 2 minutes
- **[Architecture](architecture/overview.md)** — System design, data flow, directory layout
- **[CLI Commands](cli/index.md)** — Slash commands with examples
- **[TUI Reference](tui/index.md)** — Terminal UI features and RPC methods
- **[Pet System](pet/index.md)** — Petdex mascot install, configure, animate
- **[Development](dev/index.md)** — Building, testing, contributing
- **[Parity Summary](parity-summary.md)** — Live parity scan results (what's ported vs. not)
- **[Real Gap List](real-gap-list.md)** — 1,958 honest REAL_GAPs across 230 modules

## How to read the parity numbers

The live scanner (`tests/slermes_parity_battleground.py`) is the single source of truth for completion. Run `make parity-walkway` to refresh all walkway files. Any number not from the scanner is stale — trust the scanner, not prose.

**Current state (v669, post-sync):** 12,085 PORTED / 1,958 REAL_GAP / 2 PARTIAL / 7 BOOTLEG (14,045 total).

## Project structure

```
src/
├── cli/          # CLI command dispatch, slash commands, command registry
├── gateway/      # Platform adapters (Telegram, Discord, Slack, etc.)
├── agent/        # Core agent logic (memory, tools, skills, planning)
├── tools/        # Tool implementations (terminal, file, web, browser, etc.)
├── pet/          # Petdex mascot system
├── hermes_cli/   # CLI command modules (config, session, model, etc.)
├── provider/     # LLM provider integrations
├── skills/       # Skill loader and registry
├── plugins/      # Plugin system
├── cron/         # Built-in scheduler
├── tui/          # Terminal UI (ncurses-based)
├── web_server/   # Built-in HTTP server
├── desktop_app/  # Custom C11 desktop (SDL2/Wayland)
└── ...
```

The docs directory mirrors this layout — each subsystem has its own subdirectory.

## Build

```bash
make              # Build slermes binary (~37 MB)
make test         # Run Mission8 test suite
make parity-walkway  # Refresh all parity docs from live scanner
```

## How to contribute

See [Development](dev/index.md) for building, testing, and contributing.

## About this project

Slermes is a fork of `NousResearch/hermes-agent`. We pull Python source from upstream and port features to C11. We never push C code back to upstream. The two repos have separate git histories by design.

See `BANNER.md` for the full project workflow, sync instructions, and fork reconciliation procedure.

## Project Status

| Metric | Value |
|--------|-------|
| **Version** | v669 (PORT phase) |
| **PORTED** | 12,085 (86.0% of 14,045) |
| **REAL_GAP** | 1,958 (13.9%) |
| **PARTIAL** | 2 |
| **BOOTLEG** | 7 |
| **Upstream Sync** | 1,209 ahead / 0 behind |

> Live counts: `make parity-walkway` (sentinel PARITY:AUTO). Do not hand-edit — regenerated from the live scanner on every run.
