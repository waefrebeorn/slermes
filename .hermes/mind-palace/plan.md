# Plan — Slermes C Translation

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

## Next Actions

### 1. Watercup Pass Rate ↑
- Fix ported gates: read real C signatures from PoP files, emit correct harnesses (int/bool/char*/size_t/long)
- Fix gap sentinels: gap functions need real C stubs, not constant returns
- Fix twin.py: handle class methods, complex types, import failures
- Target: 10% pass rate (2,260 gates) by end of next session

### 2. Gap Closure Priority
- Focus on pure functions first (no I/O, no state, no complex types)
- Modules with highest pure-function yield: `tools/`, `agent/`, `hermes_cli/`
- Use `gap_triage.py --top 25` for ranked list

### 3. Upstream Sync
- 2,639 behind — need to rebase onto latest upstream/main
- New Python features to port after sync

## System Health

- `make parity` → refreshes all walkway files from live scanner
- `make watercup` → runs 22,604 trace-cup gates in parallel
- `make test` → Mission 8: 77 tests
- `make build` → Clean, 0 errors

## Documentation

- All walkway files in `.hermes/` are PARITY:AUTO sentinel-owned
- All docs in `docs/` organized by subsystem
- No hand-edited parity numbers anywhere
