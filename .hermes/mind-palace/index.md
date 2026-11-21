# Mind Palace — Slermes C Translation

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
