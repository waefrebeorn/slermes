<!-- PARITY:AUTO -->| PORTED  | 18,327 / 49,699 (36.9%) |
| REAL_GAP| 31,328 (63.0%) — no N/A |
|  ├ DA-4 stubs | 15 |
|  └ Genuine gaps | 31,313 |
| PARTIAL | 44 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | 58 PASS / 1,685 (3.4%) · 58/1685 ported match · 0/0 gap mismatch |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-20T19:55:54Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

# Slermes — Work Dashboard (the walk-in doc)

> **This is the doc you read when you walk into the room.** Everything else
> is detail. The sentinel above is the live scanner verdict, owned by
> `make parity` — never hand-edit it, and never trust a prose number over a
> fresh run. Dated snapshots below are labeled; re-run the tools for live.

## §1 — Ground truth (what the detectors say)

### 1A. Function-level parity (scanner)

The sentinel above is regenerated from `live_parity_scan.json` by
`make parity` (fail-closed: it refuses to emit unless the Python quarry is
present and actually scanned). It is the only live count in this file —
no hand-typed snapshot is kept here; dated snapshots live in
[PARITY_LEDGER.md](../PARITY_LEDGER.md). Annotation integrity (orphan /
unattributed claims) is printed by `python3 scripts/parity_truth.py`.

### 1B. Body-level reality (`tests/parity_reality.py`)

Run `python tests/parity_reality.py` for live totals. This is the honest
"how much actually works" scoreboard — placeholder bodies do not count.
The scanner credits PoP claims; reality judges bodies.

### 1C. Differential oracle (`tests/run_all_oracles.sh`)

Run `bash tests/run_all_oracles.sh` for the live pair count and failures.
A green total is meaningless unless the cases actually computed expected
values. Historical FAP classes (stub-shadowed facades, stale copies,
path-shape false positives, dispatch-table false greens) are catalogued in
[fap.md](fap.md) — pattern memory, not current status.

### 1D. Watercup deep gate (`tests/watercup/`)

`make parity` runs the `--port-only` structural subset and stamps the
verdict into the sentinel's Watercup line. Full fleet: `wc_run_parallel.py`
from a plain shell (never inside an agent session — entry-point gates are
subprocess-isolated but the fleet is still heavy). Right output by the
wrong path fails here. See
[watercup-parity-oracle.md](watercup-parity-oracle.md).

### 1E. Port stages (`scripts/gen_port_stages.py` → port-stages.md)

Per-module funnel S0–S6; S5/S6 are measured from actual oracle runs under
`tests/oracle/build/`, never assumed. Generator-owned.

## §2 — The work, ranked

### P0 — Keep the gates honest
- Every commit: build health + port-object gate, oracle suite, slv suites,
  `make parity`. A red gate is the next task.
- Hunt stub-shadowed ports: `infrastructure_stubs.c` facades that disagree
  with real callers are binary-wide FAPs (two found and fixed at the latest sweep;
  grep for more before trusting any "works" claim).

### P1 — Close REAL_GAPs, one module at a time
Gap concentrations shift every wave: re-derive the current top gaps with
`python tests/slermes_parity_battleground.py --detail` and
`python scripts/parity_coverage.py --dirs` before picking a bite. Favor small,
self-contained, pure-function gaps — the ones an oracle can differentially
test and a watercup gate can pin. Rank by the axis that matches the ask:
`scripts/parity_coverage.py --fanin` for reuse, short-gap count for
throughput, gaps×LOC for the largest hole.

### P2 — SLERM the dependency front
c11_whisper (active, linked), SQLite engine, ncurses retirement via the Lua
UI substrate, ctranslate2, long-horizon crypto/inflate/regex/raster.
[third-party.md](third-party.md).

### P3 — Deepen verification
Watercup gates over the pure-function twin pool; oracle registry coverage;
annotation integrity (`scripts/audit_annotations.py`).

## §3 — How to find work when you walk in

```bash
make parity                                   # refresh sentinels (live)
python scripts/parity_coverage.py --dirs      # per-area rollup
python tests/slermes_parity_battleground.py --detail   # per-module gaps
python tests/parity_reality.py                # honest body-level scoreboard
bash tests/run_all_oracles.sh                 # behavioral regression gate
```

Read §2, take the top bite, close it with proof, commit, repeat. Never stop
between gaps.

## §4 — Architecture of the audit tooling (don't reinvent it)

- **Battleground scanner** (`tests/slermes_parity_battleground.py`) —
  function-level PoP join; static triage. Wrapped fail-closed by
  `scripts/parity_truth.py` → `live_parity_scan.json`.
- **Walkway generator** (`scripts/gen_parity_walkway.py`) — stamps every
  PARITY:AUTO sentinel + regenerates `docs/real-gap-list.md`. Pipeline:
  `scripts/run_parity_workflow.py` (`make parity`).
- **Diff oracle** (`tests/oracle/`, `tests/run_all_oracles.sh`,
  `tests/run_one_oracle.sh`) — output-vector equality vs LIVE Python. DEPS
  table is authoritative; `tests/ensure_oracle_deps.sh` builds missing
  dependency objects (incl. the SAC-safe `-O0` hash image).
- **Watercup** (`tests/watercup/`) — behavioral trace-cup (B/L/R/r/V →
  FNV-1a), language-agnostic, catches right-output-wrong-path.
- **Reality** (`tests/parity_reality.py`) — body-level; placeholders don't count.
- **Hunter** (`tests/recursive_false_gap_hunter.py`) — bootleg echo stubs.
- **Build health** (`tests/verify_build_health.py`) — per-file compile +
  port-object gate + cupboard check + link.

> Stale/contradictory historical docs are archived under `docs/archive/`
> and `.hermes/museum/` — don't trust their prose; trust the live detectors.
