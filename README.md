# Slermes — We SLERMEd Hermes Agent into C11

We took Hermes Agent — Nous Research's Python/Electron AI agent — and **SLERMed** it: started ripping the whole thing out and rebuilding it from scratch in C11. Not a wrapper. Not a thin translation layer. A ground-up reimplementation that runs as a single native binary with zero Python dependency. It's an in-progress port — see [Parity Summary](docs/parity-summary.md) for exactly how much is done.

**SLERM** (verb): to take someone's full work and make your own version from scratch. Not a fork — a ground-up reimplementation.

## What this is

<!-- PARITY:AUTO -->
**Version:** v669 (PORT phase)  
**Last updated:** 2026-08-04

> Live counts from `make parity-walkway` (sentinel PARITY:AUTO). Do not hand-edit — regenerated from the live scanner on every run.

**Upstream sync checkpoint:** 1,210 ahead / 0 behind upstream/main (last merge 2026-08-04 (upstream fetched)). The repo is up to date with upstream.

<!-- /PARITY:AUTO -->

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
| [Real Gap List](docs/real-gap-list.md) | 1,958 honest REAL_GAPs across 230 modules |

## How to read the parity numbers

The live scanner (`tests/slermes_parity_battleground.py`) is the single source of truth for completion. Run `make parity-walkway` to refresh all walkway files. Any number not from the scanner is stale — trust the scanner, not prose.

**Current state (v669, post-sync):**

| Classification | Count | Percentage | Meaning |
|----------------|-------|------------|---------|
| PORTED | 12,085 | 86.0% | C11 implementation with PoP annotation |
| REAL_GAP | 1,958 | 13.9% | Honest gaps (not yet ported — IO/network/DB/logic; NOT faked) |
| PARTIAL | 2 | 0.0% | C fns carry PoP but need full implementation |
| BOOTLEG | 7 | — | No-work echo stubs (recursive_false_gap_hunter.py) |
| TOTAL | 14,045 | 100% | All Python functions/methods scanned |

## Fork sync

Slermes is an independent C11 fork of Hermes Agent. We pull Python source from upstream (`NousResearch/hermes-agent`) and port features to C. The fork-base merge keeps upstream as an ancestor (`behind=0`) while our C11 tree stays byte-identical. See `BANNER.md` for the canonical sync workflow.

## License

Waefrebeorn Umbrella License v3.0 — custom source-available license. Not OSI-approved, not FSF-approved. See the [LICENSE](LICENSE) file.
