# Plan — Slermes C Translation

<!-- PARITY:AUTO -->| PORTED  | 6,466 / 18,219 (35.5%) |
| REAL_GAP| 11,715 (64.3%) — no N/A |
|  ├ DA-4 stubs | 11 |
|  └ Genuine gaps | 11,704 |
| PARTIAL | 38 (0.2%) |
| BOOTLEG | 0 |
| **Watercup** | unavailable — interactive hermes CLI flows (approvals/kanban/curator/hello) block the oracle; sync counts below are from the live scanner and are current |\n**Phase (v673):** PORT phase

**Upstream sync:** 2,082 ahead / 1,056 behind (last merge 2026-10-06 (upstream fetched))

_Generated 2026-10-07T05:29:39Z from live scanner + watercup oracle — do not edit by hand._<!-- /PARITY:AUTO -->

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
