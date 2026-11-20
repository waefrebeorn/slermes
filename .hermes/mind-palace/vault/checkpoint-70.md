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
