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

# Checkpoint 78 — CP78/v105: AG10 shell_hooks COMPLETE — register_from_config ported (11/11, 100%)

## Changes
- `shell_hooks_register_from_config()` — parse hooks from config JSON and register on hook registry
- Combines `shell_hooks_iter_configured()` + `shell_hooks_register_all()`
- Port of Python agent/shell_hooks.py:register_from_config()

## Evidence
- `src/agent/shell_hooks.c:646-661` — function implementation
- `include/hermes_hooks.h:203-207` — header declaration

## Impact
- **AG10:** 10/11 -> 11/11 COMPLETE (100%) — all shell_hooks.py public functions ported to C
- **AG10 now MOVED from REAL GAP to PORTED in battleship**
- **Build:** Clean, 0 errors. Tests: 4/4 pass
