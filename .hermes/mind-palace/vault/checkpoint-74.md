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
