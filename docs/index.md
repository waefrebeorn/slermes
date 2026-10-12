<!-- PARITY:AUTO -->| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
|  ├ DA-4 stubs | 11 |
|  └ Genuine gaps | 11,704 |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 |
| **Watercup** | unavailable — interactive hermes CLI flows (approvals/kanban/curator/hello) block the oracle; sync counts below are from the live scanner and are current |\n**Phase (v673):** PORT phase

**Upstream sync:** 2,082 ahead / 1,056 behind (last merge 2026-10-06 (upstream fetched))

_Generated 2026-10-07T05:29:39Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Slermes Documentation

**Slermes** is an in-progress C11 reimplementation of Hermes Agent — Nous Research's Python/Electron AI agent. One binary, zero Python dependencies.

## Quick Links

| Category | Doc | Description |
|----------|-----|-------------|
| Getting Started | [getting-started.md](getting-started.md) | Build, configure, run in 2 minutes |
| Architecture | [overview.md](architecture/overview.md) | System design, data flow, directory layout |
| Build System | [build-system.md](architecture/build-system.md) | Make targets, dependencies, packaging |
| CLI | [index.md](cli/index.md) | Slash commands with examples |
| TUI | [index.md](tui/index.md) | Terminal UI features, RPC methods |
| Pet System | [index.md](pet/index.md) | Petdex mascot install, configure, animate |
| Development | [index.md](dev/index.md) | Building, testing, contributing |
| Parity | [parity-summary.md](parity-summary.md) | Live parity scan results |
| Gaps | [real-gap-list.md](real-gap-list.md) | Live REAL_GAPs across modules |
| Contracts | [relay-connector-contract.md](architecture/relay-connector-contract.md) | Platform relay protocol |
| Contracts | [chronos-managed-cron-contract.md](architecture/chronos-managed-cron-contract.md) | Cron scheduler interface |
| Operations | [streaming-tts.md](operations/streaming-tts.md) | TTS pipeline docs |
| Security | [profile-routing.md](security/profile-routing.md) | Auth and routing |
| Security | [network-egress-isolation.md](security/network-egress-isolation.md) | Network isolation |
| FAP | [fap.md](fap.md) | Behavioral divergences + oracle harness |
| Watercup | [watercup-parity-oracle.md](watercup-parity-oracle.md) | Trace-cup deep gate |
| Sessions | [session-lifecycle.md](session-lifecycle.md) | Session lifecycle |
| State | [state.md](state.md) | Project state and history |
| Organization | [organization-parity.md](organization-parity.md) | Module ↔ C file mapping |
| Module Map | [module-map.md](module-map.md) | Complete module map |
| Changelog | [CHANGELOG-SLERMES.md](CHANGELOG-SLERMES.md) | Version history |

## How to read the parity numbers

The live scanner (`tests/slermes_parity_battleground.py`) is the single source of truth for completion. Run `make parity-walkway` to refresh all walkway files. Any number not from the scanner is stale — trust the scanner, not prose.

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
