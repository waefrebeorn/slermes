# Mind Palace — Slermes C Translation

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

## Active Files

| File | Purpose |
|------|---------|
| `index.md` | This file — central navigation |
| `state.md` | How to re-establish current state |
| `plan.md` | Next actions |
| `goal-mantra.md` | Project goal |
| `prestige.md` | Session wrap-up ritual |
| `next-session-prompt.md` | Handoff to the next session |

Historical artifacts: `archive/` and `vault/` (history, not state).

## System Components (the detectors)

- **Scanner:** `tests/slermes_parity_battleground.py` — function-level PoP scan
- **Truth wrapper:** `scripts/parity_truth.py` — fail-closed → `live_parity_scan.json`
- **Walkway:** `scripts/gen_parity_walkway.py` — stamps every PARITY:AUTO sentinel
- **Pipeline:** `scripts/run_parity_workflow.py` (`make parity`) — scan → watercup → walkway
- **Diff oracle:** `tests/run_all_oracles.sh` — C vs LIVE Python outputs
- **Watercup:** `tests/watercup/wc_run_parallel.py` — trace-cup deep gate (22,603 gates)
- **Hunter:** `tests/recursive_false_gap_hunter.py` — bootleg detection
- **Reality:** `tests/parity_reality.py` — body-level honest scoreboard

## How to Read

All live numbers come from `live_parity_scan.json` via `make parity`. The
sentinel block is generator-owned — never hand-edit. Key metrics are NOT
hand-typed in this file; run the tools:

```bash
make parity
python scripts/parity_coverage.py --dirs
python tests/parity_reality.py
```
