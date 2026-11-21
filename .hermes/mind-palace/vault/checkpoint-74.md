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

# Checkpoint 74 — CP74/v101: AG10 run_once ported — AG10 COMPLETE

## Changes
- `shell_hooks_run_once()` — builds a hook spec and routes through shell_hook_callback()
  for synthetic single-hook invocation (hermes hooks test pathway)

## Evidence
- `src/agent/shell_hooks.c:591-611`, `include/hermes_hooks.h:178-187`

## Impact
- **AG10:** 10/11 -> 11/11 — COMPLETE (100%)
- **Build:** Clean, 0 errors. Tests: 4/4 pass
- **Commit:** `da14c49af`
