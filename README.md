# Slermes — We SLERMEd Hermes Agent into C11

We took Hermes Agent — Nous Research's Python/Electron AI agent — and **SLERMed** it: started ripping the whole thing out and rebuilding it from scratch in C11. Not a wrapper. Not a thin translation layer. A ground-up reimplementation that runs as a single native binary with zero Python dependency. It's an in-progress port — see [Parity Summary](docs/parity-summary.md) for exactly how much is done.

**SLERM** (verb): to take someone's full work and make your own version from scratch. Not a fork — a ground-up reimplementation.

## What this is

<!-- PARITY:AUTO -->| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
|  ├ DA-4 stubs | 11 |
|  └ Genuine gaps | 11,704 |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 |
| **Watercup** | unavailable — interactive hermes CLI flows (approvals/kanban/curator/hello) block the oracle; sync counts below are from the live scanner and are current |\n**Phase (v672):** PORT phase

**Upstream sync:** 2,081 ahead / 0 behind (last merge 2026-10-03 (upstream fetched))

_Generated 2026-10-06T02:44:06Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

## Quick Start

```bash
git clone https://github.com/waefrebeorn/slermes
cd slermes
make -j$(nproc)
./slermes
```

**Dependencies:** `build-essential libssl-dev libwayland-dev libxkbcommon-dev`

## Documentation

| Doc | Description |
|-----|-------------|
| [Getting Started](docs/getting-started.md) | Build, configure, run in 2 minutes |
| [Architecture](docs/architecture/overview.md) | System design, data flow, directory layout |
| [CLI Commands](docs/cli/index.md) | Slash commands with examples |
| [TUI Reference](docs/tui/index.md) | Terminal UI layout, shortcuts, RPC methods |
| [Pet System](docs/pet/index.md) | Petdex mascot install, configure, animate |
| [Build System](docs/architecture/build-system.md) | Make targets, dependencies, packaging |
| [Development](docs/dev/index.md) | Building, testing, contributing |
| [Parity Summary](docs/parity-summary.md) | Live parity scan results (what's ported vs. not) |
| [Real Gap List](docs/real-gap-list.md) | Honest REAL_GAPs across modules |
| [FAP](docs/fap.md) | Behavioral divergences the scanner can't see + the oracle harness |
| [Water-Cup Parity Oracle](docs/watercup-parity-oracle.md) | Trace-cup deep gate: right output, wrong path fails |
| [Bootleg Audit](docs/archive/bootleg-audit-.md) | Empty-body audit: findings and fix campaign |
| [Lib Audit](docs/archive/lib_audit_2026-08-22.md) | Dependency libs vs Python twins |
| [Full Documentation](docs/index.md) | Complete documentation index |

## How to read the parity numbers

The live scanner (`tests/slermes_parity_battleground.py`) is the single source of truth for completion. Run `make parity-walkway` to refresh all walkway files. Any number not from the scanner is stale — trust the scanner, not prose.

## Fork sync

Slermes is an independent C11 fork of Hermes Agent. We pull Python source from upstream (`NousResearch/hermes-agent`) and port features to C. The fork-base merge keeps upstream as an ancestor (`behind=0`) while our C11 tree stays byte-identical. See `BANNER.md` for the canonical sync workflow.

## License

Waefrebeorn Umbrella License v3.0 — custom source-available license. Not OSI-approved, not FSF-approved. See the [LICENSE](LICENSE) file.
