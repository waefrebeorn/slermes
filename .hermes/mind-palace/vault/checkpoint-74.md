<!-- PARITY:AUTO -->
| PORTED  | 8,778 / 34,262 (25.6%) |
| REAL_GAP| 25,451 (74.3%) — no N/A |
|  ├ DA-4 stubs | 43 |
|  └ Genuine gaps | 25,408 |
| PARTIAL | 33 (0.1%) |
| BOOTLEG | 0 |
| **Watercup** | not run — no results under tests/watercup/build/; the scanner counts above are current |
**Phase (v673):** PORT phase

**Upstream sync:** 0 ahead / 0 behind (last merge unknown)

_Generated 2026-11-15T07:30:31Z from live scanner + watercup oracle — do not edit by hand._
<!-- /PARITY:AUTO -->

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
