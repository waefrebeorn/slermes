<!-- PARITY:AUTO -->| PORTED  | 8,377 / 34,262 (24.4%) |
| REAL_GAP| 25,840 (75.4%) — no N/A |
|  ├ DA-4 stubs | 13 |
|  └ Genuine gaps | 25,827 |
| PARTIAL | 45 (0.1%) |
| BOOTLEG | 2 |
| **Watercup** | unavailable — oracle produced no verdicts |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-18T22:16:00Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Slermes Documentation

**Slermes** is a from-scratch C11 SLERM of Hermes Agent — Nous Research's
Python/Electron AI agent. One native binary, zero Python at runtime. The
mission covers the whole ecosystem: agent, tools, platforms, plugins, and
every third-party dependency. Nothing is N/A; nothing is "not doable".

## Start here

| Doc | Description |
|----------|-------------|
| [Work Dashboard](WORK_DASHBOARD.md) | **The walk-in doc**: live detectors + ranked work |
| [Getting Started](getting-started.md) | Build, configure, run |
| [Roadmap](../ROADMAP.md) | The fronts: gaps, dependencies, Lua UI, verification |
| [Gates](dev/gates.md) | Every verification gate and how to run it |
| [C Standards](dev/c-standards.md) | The enforced C11 standard |
| [soul.md](../soul.md) | The operating contract (angel coder) |

## Architecture & subsystems

| Doc | Description |
|----------|-------------|
| [Overview](architecture/overview.md) | System design, data flow, directory layout |
| [Build System](architecture/build-system.md) | Make targets, profiles, dependencies |
| [Directory Layout](architecture/directory-layout.md) | Where everything lives |
| [How It Works](how-it-works.md) | CLI entry, loop, line editor depth |
| [CLI Commands](cli/index.md) | Slash command reference |
| [TUI Reference](tui/index.md) | Terminal UI (and the Lua direction) |
| [Pet System](pet/index.md) | Petdex mascots |
| [Session Lifecycle](session-lifecycle.md) | Gateway session lifecycle |
| [Relay Connector Contract](architecture/relay-connector-contract.md) | Platform relay protocol |
| [Chronos Managed Cron Contract](architecture/chronos-managed-cron-contract.md) | Cron scheduler interface |
| [Billing Lifecycle](architecture/billing-lifecycle.md) | Usage/billing flow |
| [Streaming TTS](operations/streaming-tts.md) | TTS pipeline |
| [Profile Routing](security/profile-routing.md) | Auth and routing |
| [Network Egress Isolation](security/network-egress-isolation.md) | Network isolation |

## Parity & verification

| Doc | Description |
|----------|-------------|
| [Parity Summary](parity-summary.md) | Live sentinel + how to read the numbers |
| [Parity Ledger](../PARITY_LEDGER.md) | Dated snapshot of all three audits |
| [Port Stages](port-stages.md) | Per-module funnel S0–S6 (generator-owned) |
| [Real Gap List](real-gap-list.md) | Every REAL_GAP, function level (generator-owned) |
| [FAP](fap.md) | Behavioral divergences the scanner can't see |
| [Water-Cup Parity Oracle](watercup-parity-oracle.md) | Trace-cup deep gate |
| [Third-Party SLERMing](third-party.md) | Plugins, providers, dependency front |
| [Organization Parity](organization-parity.md) | Module ↔ C home structural scan |
| [Module Map](module-map.md) | Python agent/ ↔ C mapping |
| [Naming Standard](dev/naming.md) | Parity joins on the module name |
| [Changelog](CHANGELOG-SLERMES.md) | Deliberate C-vs-Python deltas (historical log) |

## Project structure

```
src/
├── main.c        # entry point: argv dispatch (CLI, gateway, cron, acp, mcp-serve, api-server…)
├── cli/          # CLI loop, slash commands, config, display, TUI units
├── hermes_cli/   # ports of the hermes_cli Python package
├── agent/        # agent loop, providers, credentials, memory, context
├── gateway/      # platform adapters + relay + gateway runtime
│   └── platforms/
├── tools/        # tool implementations
├── plugins/      # native C plugins (plugin_*.c → .so)
├── cron/         # scheduler
├── pet/          # Petdex mascots
├── lv/           # slerm-Lua interpreter (slv_*, upstream Wua)
├── lua/          # Lua-driven terminal host (slv_ui.c)
└── ...
include/          # headers incl. port_<stem>.h unit headers + compat/ shims
lib/              # our C libraries + vendored third-party SLERM targets
tests/            # oracles (sta_oracle_*.py + t_port_*.c), watercup/, gates
scripts/          # parity pipeline, audits, generators
.hermes/          # project navigation: battleship, state, mantra, mind-palace
```

## Build (quick reference)

```bash
# Linux
make -j$(nproc) slermes

# Windows (inside MSYS2 bash)
source build/winlibs-env.sh
make -j8 slermes USERPROFILE='C:\Users\<you>' LOCALAPPDATA='C:\Users\<you>\AppData\Local'

make test            # Mission8 test suite
make parity          # scan -> watercup -> walkway: refresh every parity doc
```

Full toolchain doctrine and gates: [dev/gates.md](dev/gates.md).

## How to read the parity numbers

The live scanner (`tests/slermes_parity_battleground.py`, wrapped by
`scripts/parity_truth.py`) is the single source of the sentinel counts;
`make parity` refreshes every walkway file. Any number not from a live run
is stale — trust the generator, not prose. Three audits exist with different
denominators (scanner / body-level reality / port stages); see
[Parity Summary](parity-summary.md) and [Parity Ledger](../PARITY_LEDGER.md).

## About this project

Slermes pulls Python source from upstream (`NousResearch/hermes-agent`)
into the quarry and ports the delta to C11. C work is never pushed back
upstream; port commits land on `integrate`. See [BANNER](../BANNER.md) for
the sync workflow.
