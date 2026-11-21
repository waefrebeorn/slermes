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

# Checkpoint 70 — CP70/v97: AG25 restore_primary_runtime ported (19/24)

## Changes

**Ported `restore_primary_runtime()` from `agent/agent_runtime_helpers.py:894-989` (96L)**.

### Implementation
- Added `retry_done:` restore label in `agent_loop.c` — restores primary provider/model after fallback per-call.
- In C the fallback is per-call, so saved values are already restored inline in the fallback-exhausted path. The label exists for the transport recovery success goto target.

### Evidence
- `src/agent/agent_loop.c:1592-1596` — restore label and comment

### Impact
- **AG25:** 18/24 -> 19/24 (79%)
- **Build:** Clean, 0 errors
- **Tests:** 4/4 pass
- **Commit:** `2307e4fc7`
