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
