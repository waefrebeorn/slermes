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

# Checkpoint 77 — CP77/v104: AG10 shell_hooks: reset_for_tests + iter_configured_hooks ported — 10/11 (91%)

## Changes
- `shell_hooks_reset_for_tests()` — clears hooks config array without deregistering from hook registry (port of Python shell_hooks.py:reset_for_tests)
- `shell_hooks_iter_configured()` — parses hooks: block from a config JSON object (port of Python shell_hooks.py:iter_configured_hooks)

## Evidence
- `src/agent/shell_hooks.c:626-644` — both function implementations
- `include/hermes_hooks.h:196-205` — header declarations

## Impact
- **AG10:** 8/11 -> 10/11 (91%) — only `register_from_config` remains
- **Build:** Clean, 0 errors. Tests: 4/4 pass
