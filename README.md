# Slermes — We SLERMEd Hermes Agent into C11

We took Hermes Agent — Nous Research's Python/Electron AI agent — and **SLERMed**
it: ripping the whole thing out and rebuilding it from scratch in pure C11. Not a
wrapper. Not a thin translation layer. A ground-up reimplementation that runs as
a single native binary with zero Python dependency at runtime.

**SLERM** (verb): to take someone's full work and make your own version from
scratch. Not a fork — a ground-up reimplementation.

**The mission is the whole ecosystem.** Every agent feature, every tool, every
gateway platform, every provider, every plugin — and the third-party
dependencies underneath them — gets its own from-scratch C11 implementation.
Nothing is "out of scope", nothing is "not doable", nothing stays N/A. If it
runs in the Python ecosystem, it is work to port, and we do it because we are
doing it in C11. See [Third-Party SLERMing](docs/third-party.md) for the
dependency front and [Roadmap](ROADMAP.md) for the battle order.

## Where the port stands (live)

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

The block above is generator-owned: `make parity` recomputes it from the live
scanner and the watercup oracle and stamps every copy in the repo. Never
hand-edit a number inside a `` sentinel.

## How we know a port is real — the verification stack

A PoP comment is a claim, not a proof. Four detectors sit on top of each other,
each catching what the one below it cannot see:

1. **Battleground scanner** (`tests/slermes_parity_battleground.py`) — static,
   function-level. Joins every Python function to the C tree by name and by
   `/* PoP: <c_fn> @ <module.py>:<py_fn> */` annotation. Fast triage; it never
   executes anything, so it cannot tell a real body from a stub.
2. **Differential oracle** (`tests/oracle/`, `tests/run_all_oracles.sh`) —
   behavioral. Compiles the C harness, runs it and the LIVE Python reference
   over the same fixtures, diffs the outputs. Any `cases: MISMATCH` is a **FAP**
   (Functional Alignment Problem) — see [docs/fap.md](docs/fap.md).
3. **Water-cup deep gate** (`tests/watercup/`) — path parity. Folds the ordered
   trace of branches, loops, recursion and intermediate values into a cup hash
   (FNV-1a) on both sides. A stub that returns the right output by the wrong
   path carves a different shape and fails. Language-agnostic: 22,603 gates.
   See [docs/watercup-parity-oracle.md](docs/watercup-parity-oracle.md).
4. **Build health + port-object gates** (`tests/verify_build_health.py`) —
   every source the platform build compiles must parse clean, every
   PoP-credited port must be linked into the binary, and the tree must link.

**A port is parity-closed only when the oracle AND the watercup say MATCH.**
The scanner's PORTED count is a pointer to work, never a verdict — the honest
body-level number comes from `tests/parity_reality.py`, which scores
`(void)x; return 0;` bodies as placeholders, not implementations.

## Quick Start

**Linux**

```bash
git clone https://github.com/waefrebeorn/slermes
cd slermes
make -j$(nproc) slermes
./slermes
```

**Windows (MSYS2 + winlibs — the primary development platform)**

```bash
git clone https://github.com/waefrebeorn/slermes
cd slermes
# inside MSYS2 bash:
source build/winlibs-env.sh
make -j8 slermes USERPROFILE='C:\Users\<you>' LOCALAPPDATA='C:\Users\<you>\AppData\Local'
./slermes.exe
```

`build/winlibs-env.sh` must be sourced inside the same shell that runs make:
it puts the winlibs GCC toolchain first and sets the prebuilt third-party lib
paths. `USERPROFILE`/`LOCALAPPDATA` are passed as **make variables** — this
box's shell drops exported env vars before a native child sees them, and
ccache refuses to run without them. Full toolchain doctrine:
[docs/dev/gates.md](docs/dev/gates.md).

**Dependencies**

- Linux: `build-essential libssl-dev libwayland-dev libxkbcommon-dev`
- Windows: MSYS2 (bash, make, python) + the winlibs GCC bundle under
  `C:/winlibs` (see `build/winlibs-env.sh`). No Python runtime is needed to
  build or run the binary; Python is only used by the verification tooling.

**First run** resolves its home as `$SLERMES_HOME` → `~/.slermes` → `~/.hermes`,
loads `.env` from there, and dispatches on argv: interactive CLI by default,
plus `gateway`, `cron`, `setup`, `init`, `doctor`, `update`, `acp`,
`mcp-serve [port]`, `api-server [port]`, `completions`, `--tui`, `--json`,
`--session`, `--profile`, `--eval`. Entry point: `src/main.c`.

## Documentation

| Doc | Description |
|-----|-------------|
| [Getting Started](docs/getting-started.md) | Build, configure, run |
| [Work Dashboard](docs/WORK_DASHBOARD.md) | The single walk-in doc: live detectors + ranked work |
| [Architecture](docs/architecture/overview.md) | System design, data flow, directory layout |
| [Build System](docs/architecture/build-system.md) | Make targets, profiles, dependencies |
| [Development](docs/dev/index.md) | Building, testing, contributing |
| [Gates](docs/dev/gates.md) | Every verification gate and how to run it |
| [C Standards](docs/dev/c-standards.md) | The enforced C11 standard |
| [Naming Standard](docs/dev/naming.md) | Unit/file naming: parity joins on the module name |
| [Parity Summary](docs/parity-summary.md) | Live parity scan results |
| [Port Stages](docs/port-stages.md) | Per-module funnel S0–S6 (generator-owned) |
| [Real Gap List](docs/real-gap-list.md) | Every REAL_GAP, function level (generator-owned) |
| [FAP](docs/fap.md) | Behavioral divergences the scanner can't see |
| [Water-Cup Parity Oracle](docs/watercup-parity-oracle.md) | Trace-cup deep gate: right output, wrong path fails |
| [Third-Party SLERMing](docs/third-party.md) | Plugins, providers, and the dependency front |
| [CLI Commands](docs/cli/index.md) | Slash commands with examples |
| [TUI Reference](docs/tui/index.md) | Terminal UI layout and shortcuts |
| [Full Documentation](docs/index.md) | Complete documentation index |

## Staged progress

A single percentage cannot answer "what next". Progress is therefore also
reported as a funnel of stages every Python module advances through; a module
counts at the highest stage it **fully** satisfies, so a port with a broken
oracle is not counted as parity:

| Stage | Meaning |
|---|---|
| S0 | no C unit |
| S1 | `src/**/port_<stem>.c` exists |
| S2 | `include/port_<stem>.h` exists and is included |
| S3 | harness `tests/t_port_<stem>.c` exists |
| S4 | oracle `tests/sta_oracle_<stem>.py` exists |
| S5 | instrument green: harness compiles, emits output, oracle runs |
| S6 | parity: the oracle's cases MATCH |

Full table and per-package rollup: **[Port Stages](docs/port-stages.md)** ·
`python scripts/gen_port_stages.py` (add `--json`).

## How to read the parity numbers

Three audits run over the same tree and they do **not** share a denominator:

| Audit | Command | What it counts |
|-------|---------|----------------|
| Scanner (battleground) | `python scripts/parity_truth.py` | feature-level PoP claims → `live_parity_scan.json` |
| Body-level reality | `python tests/parity_reality.py` | real bodies vs placeholder bodies (stricter) |
| Port stages | `python scripts/gen_port_stages.py` | per-module funnel S0–S6 |

Quote the audit that matches the question; a number copied by hand is a number
that has already gone stale. `scripts/parity_truth.py` is fail-closed: it
refuses to emit a number unless the Python ground truth is present and the
scanner actually scanned it. `make parity` runs the whole pipeline — scan →
watercup → walkway — and regenerates every sentinel block and
`docs/real-gap-list.md` from live sources. Nothing in a derived document is
hand-typed.

To find the next thing to port: `python scripts/parity_coverage.py --dirs`
(per-area rollup), `--by-module` (every module with def and importer counts),
`--fanin` (rank by reuse). Match the axis to the ask — fan-in for reuse,
short-gap count for throughput, `real_gaps × LOC` for the largest hole.

## The loop

One module at a time, finish it: read the scanner detail → grep
`src/ include/ lib/` for the symbol before writing anything → implement or
alias the real behavior → prove it with an oracle (and a watercup gate for
pure helpers) → build → commit. Behavior first, annotations last. A stub that
satisfies the scanner but does no work is not progress — it is a lie.
The full contract lives in [soul.md](soul.md).

## Fork sync

Slermes pulls Python source from upstream (`NousResearch/hermes-agent`) into
the quarry and ports the delta to C11; C work is never pushed back upstream.
Port commits land on the `integrate` branch and push to `origin integrate`.
The canonical sync workflow is in [BANNER.md](BANNER.md).

## License

Waefrebeorn Umbrella License v3.0 — custom source-available license. Not
OSI-approved, not FSF-approved. See the [LICENSE](LICENSE) file.
