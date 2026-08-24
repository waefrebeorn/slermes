# Slermes — We SLERMEd Hermes Agent into C11

We took Hermes Agent — Nous Research's Python/Electron AI agent — and **SLERMed** it: started ripping the whole thing out and rebuilding it from scratch in C11. Not a wrapper. Not a thin translation layer. A ground-up reimplementation that runs as a single native binary with zero Python dependency. It's an in-progress port — see [Parity Summary](docs/parity-summary.md) for exactly how much is done.

**SLERM** (verb): to take someone's full work and make your own version from scratch. Not a fork — a ground-up reimplementation.

## What this is

<!-- PARITY:AUTO -->
**Version:** v671 (PORT phase)  
**Last updated:** 2026-08-24

> Live counts from `make parity-walkway` (sentinel PARITY:AUTO). Do not hand-edit — regenerated from the live scanner on every run.

**Parity:** 14,512 PORTED (87.2%) · 2,130 REAL_GAP (356 DA-4 stubs, 1,774 genuine) · 0 BOOTLEG

**Upstream sync checkpoint:** 1,897 ahead / 10 behind upstream/main (last merge 2026-08-24 (upstream fetched)). The behind-count is the staleness timer.

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
| [Real Gap List](docs/real-gap-list.md) | Honest REAL_GAPs across modules |
| [FAP](docs/fap.md) | Behavioral divergences the scanner can't see + the oracle harness |
| [Water-Cup Parity Oracle](docs/watercup-parity-oracle.md) | Trace-cup deep gate: right output, wrong path fails |
| [Session Prestige 2026-08-22](docs/session-prestige-2026-08-22.md) | Session wrap-up: status, in-flight work, next-session queue |
| [Bootleg Audit](docs/bootleg-audit-2026-08-22.md) | Empty-body audit: 497 found, fix campaign results, remaining queue |
| [Lib Audit](docs/lib_audit_2026-08-22.md) | All 79 dependency libs vs Python twins; 5 fixed with oracles |

## How to read the parity numbers

The live scanner (`tests/slermes_parity_battleground.py`) is the single source of truth for completion. Run `make parity-walkway` to refresh all walkway files. Any number not from the scanner is stale — trust the scanner, not prose.

## Fork sync

Slermes is an independent C11 fork of Hermes Agent. We pull Python source from upstream (`NousResearch/hermes-agent`) and port features to C. The fork-base merge keeps upstream as an ancestor (`behind=0`) while our C11 tree stays byte-identical. See `BANNER.md` for the canonical sync workflow.

## License

Waefrebeorn Umbrella License v3.0 — custom source-available license. Not OSI-approved, not FSF-approved. See the [LICENSE](LICENSE) file.
