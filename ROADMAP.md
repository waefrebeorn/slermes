# Slermes Roadmap — SLERM the Whole Ecosystem

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

> Rewriting from scratch in C11 is the point. There is no N/A, no "out of
> scope", no "not doable" — every feature of the Python ecosystem, and every
> third-party dependency under it, is work to SLERM. We don't give up; we do
> it because we are doing it in C11.

The sentinel above is the live scanner verdict, owned by `make parity`. The
fronts below are the battle order; per-module detail lives in
[docs/real-gap-list.md](docs/real-gap-list.md) (generator-owned) and the
ranked work list in [docs/WORK_DASHBOARD.md](docs/WORK_DASHBOARD.md).

## Front 1 — Close the REAL_GAPs (the main front)

Per-area coverage measured by `python scripts/parity_coverage.py --dirs`
(snapshot below — re-run for live numbers; never quote these from memory):

| Area | Covered | Total | % |
|---|---:|---:|---:|
| hermes_cli | 465 | 810 | 57% |
| agent | 247 | 412 | 60% |
| tools | 257 | 366 | 70% |
| gateway | 145 | 340 | 43% |
| plugins | 44 | 304 | 14% |
| tui_gateway | 7 | 122 | 6% |
| pm | 4 | 53 | 8% |
| cron | 23 | 52 | 44% |
| acp_adapter | 6 | 22 | 27% |
| hermes_platform | 0 | 15 | 0% |

**Pick the axis that matches the ask:** fan-in for reuse
(`parity_coverage.py --fanin`), short-gap count among low-ported modules for
throughput, `real_gaps × LOC` for the largest hole. Per-area gap order:
`hermes_cli` > `agent` > `gateway` > `tools` > `cron`.

**The loop (one module at a time, finish it):**

1. `python tests/slermes_parity_battleground.py --detail` filtered to the module.
2. Grep `src/ include/ lib/` for each symbol **before writing anything** —
   naming gaps (behavior exists under another name) are the cheapest real fix.
3. Implement the real behavior, or expose the existing one under the name
   callers use. Never an empty body with a PoP comment.
4. Prove it: differential oracle (`tests/run_one_oracle.sh`) and, for pure
   helpers, a watercup gate. Wire new suites into the standing gates.
5. Build, run the gates, commit the bite to `integrate`.

A module is done when its tests pass — not when its gap count hits zero.

## Front 2 — SLERM the third-party dependencies

The binary must stand alone: every vendored dependency is a SLERM target, not
a permanent resident. Status and targets in
[docs/third-party.md](docs/third-party.md). Active examples:

- **whisper.cpp → `lib/c11_whisper/`** — pure-C11 speech-to-text port
  (model/math/encoder/compat units exist; the vendored `lib/whisper_cpp/`
  headers are the reference, not the destination).
- **SQLite amalgamation (`lib/libdb/sqlite3.c`, 8.7 MB vendored)** — the db
  layer (`db.c`) is ours; the engine underneath is a SLERM target.
- **ncurses (`lib/libncurses/`)** — vendored; the UI direction is the
  slerm-Lua substrate (Front 3), which retires the ncurses dependency on
  Windows entirely.
- **ctranslate2 (`lib/ctranslate2_src/`)** — translation backend slot; the
  C11 implementation is the work.

The rule is the same as for Python features: reference in, from-scratch C11
out, oracle/watercup proof, no escape hatches.

## Front 3 — Lua is the UI substrate (and it serves parity)

Slermes follows WWW: **C11 owns memory, the ECS, physics and the spatial
index; Lua owns game logic, GUI and mods.** The ncurses TUI is a dead end on
Windows — build on the `src/lv/` slerm-Lua interpreter (`slv_*`, upstream is
[Wua](https://github.com/waefrebeorn/Wua)) with the Lua-driven terminal host
at `src/lua/slv_ui.c`.

**Lua/Wua is a MEANS, not the mission** — when the gap list and the Lua
roadmap compete, the gap list wins.

## Front 4 — Deepen the verification net

Parity is only as honest as its detectors:

- Expand watercup coverage across the pure-function twin pool (each gate is
  3 files: `gate.json` + `harness.c` + `twin.py`; `wc_run.py` does the rest).
- Keep the oracle registry covering every committed port; a FAP that never
  runs is still a FAP.
- Keep the gates fail-closed: empty selection ≠ success, a suite that cannot
  build is a failure, a sanitizer that did not run is not a pass.

## Front 5 — Distribution (when the binary earns it)

Packaging scaffolding exists (`packaging/appimage|docker|homebrew|nix|nsis`,
`.github/workflows/release.yml`). Release engineering is downstream of
parity: ship binaries when the gates are green on all three platforms, not
before. **Windows, Linux and macOS must all build** — a change that compiles
on one only is not done.

## Definition of done

- Every Python feature has a C11 body that does real work (no stubs, no
  placeholder credit — `tests/parity_reality.py` is the honest scoreboard).
- Every ported function passes its differential oracle **and** its watercup
  gate where one exists.
- Every third-party dependency is SLERMed or actively being SLERMed — none is
  declared "not doable".
- `make parity` reports REAL_GAP 0 and BOOTLEG 0; the phase flips to COMPLETE
  on its own. No hand-edited victory.
