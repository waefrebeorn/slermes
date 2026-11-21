<!-- PARITY:AUTO -->| PORTED  | 21,129 / 49,699 (42.5%) |
| REAL_GAP| 28,532 (57.4%) — no N/A |
|  ├ DA-4 stubs | 26 |
|  └ Genuine gaps | 28,506 |
| PARTIAL | 38 (0.1%) |
| BOOTLEG | 4 |
| **Watercup** | unavailable — partial sweep only: 24 of ~1685 port gates completed before the pipeline's 120s stage timeout; a full watercup sweep is required to refresh this number (results.jsonl is untracked build output) |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-21T21:16:55Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

## Next Actions

### 1. Close REAL_GAPs (the main front)
- Pure functions first (no I/O, no global state, no complex types) — they
  are the ones an oracle can differentially test.
- Pick the axis that matches the ask: fan-in for reuse, short-gap count for
  throughput, gaps×LOC for the largest hole.
- `python scripts/parity_coverage.py --by-module` for the ranked list;
  `--detail` on the scanner for per-function gaps.
- Grep before writing; naming gaps are the cheapest real fix.

### 2. Deepen the watercup net
- Add gates over the pure-function twin pool (3 files each; the engine does
  the rest). Fix ported-gate harnesses to read real C signatures.
- A gap function needs a real C body, not a constant-return sentinel.

### 3. SLERM third-party dependencies
- Finish c11_whisper; queue the SQLite engine, ncurses retirement (Lua UI),
  ctranslate2. See docs/third-party.md.

### 4. Upstream sync
- When the sentinel's behind-count is non-zero, run the BANNER sync flow;
  the new Python delta becomes REAL_GAP work.

## System Health (run, don't quote)

- `make parity` → refresh all walkway files from the live scanner
- `make watercup` / `wc_run_parallel.py` → trace-cup fleet
- `make test` → Mission 8 suite
- `make slermes` → the binary

## Documentation

- Walkway files carry generator-owned PARITY:AUTO sentinels.
- No hand-edited parity numbers anywhere.
