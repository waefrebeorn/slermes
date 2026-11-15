# Slermes — We SLERMEd Hermes Agent into C11

We took Hermes Agent — Nous Research's Python/Electron AI agent — and **SLERMed** it: started ripping the whole thing out and rebuilding it from scratch in C11. Not a wrapper. Not a thin translation layer. A ground-up reimplementation that runs as a single native binary with zero Python dependency. It's an in-progress port — see [Parity Summary](docs/parity-summary.md) for exactly how much is done.

**SLERM** (verb): to take someone's full work and make your own version from scratch. Not a fork — a ground-up reimplementation.

## What this is

<!-- PARITY:AUTO -->| PORTED  | 8,778 / 34,262 (25.6%) |
| REAL_GAP| 25,451 (74.3%) — no N/A |
|  ├ DA-4 stubs | 43 |
|  └ Genuine gaps | 25,408 |
| PARTIAL | 33 (0.1%) |
| BOOTLEG | 0 |
| **Watercup** | not run — no results under tests/watercup/build/; the scanner counts above are current |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-15T07:30:31Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

## Quick Start

**Linux**

```bash
git clone https://github.com/waefrebeorn/slermes
cd slermes
make -j$(nproc)
./slermes
```

**Windows (MSYS2 — the primary development platform)**

```bash
git clone https://github.com/waefrebeorn/slermes
cd slermes
source build/winlibs-env.sh
make -j8 slermes USERPROFILE='C:\Users\<you>' LOCALAPPDATA='C:\Users\<you>\AppData\Local'
./slermes.exe
```

`build/winlibs-env.sh` must be sourced first: it sets the compiler and the
prebuilt third-party lib paths, and this bash drops exported env vars.

**Dependencies**

- Linux: `build-essential libssl-dev libwayland-dev libxkbcommon-dev`
- Windows: MSYS2 with `mingw-w64-x86_64-gcc`; the prebuilt libs live under
  `C:/winlibs` (see `build/winlibs-env.sh`). No Python runtime is needed to
  build or run.

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

Two audits run over the same tree and they do **not** share a denominator:

| Audit | Command | Total features | Counts |
|-------|---------|----------------|--------|
| Scanner (walkway) | `python scripts/parity_truth.py` | 27,877 | 6,248 PORTED · 21,596 REAL_GAP · 33 PARTIAL |
| Body-level reality | `python tests/parity_reality.py` | 35,858 | 12,209 implemented · 686 placeholder · 22,963 missing |

`scripts/parity_truth.py` is the only writer of parity numbers, and it is
fail-closed: it refuses to emit one unless the Python ground truth is present
and the scanner actually scanned it. `scripts/gen_parity_walkway.py` (or
`make parity`) refreshes every `` block from that output,
including this README.

`tests/parity_reality.py` is the stricter audit and extracts more features,
which is why its total is larger: a `(void)x; return 0;` body counts as
*placeholder*, not *implemented*, while the scanner credits a PoP annotation
even when the body is a stub. Quote the audit that matches the question, and
never hand-edit a number inside a `` block.

To find the next thing to port, rank by fan-in rather than file size:
`python scripts/parity_coverage.py --fanin`.

## Fork sync

Slermes is an independent C11 fork of Hermes Agent. We pull Python source from upstream (`NousResearch/hermes-agent`) and port features to C. The fork-base merge keeps upstream as an ancestor (`behind=0`) while our C11 tree stays byte-identical. See `BANNER.md` for the canonical sync workflow.

## License

Waefrebeorn Umbrella License v3.0 — custom source-available license. Not OSI-approved, not FSF-approved. See the [LICENSE](LICENSE) file.
