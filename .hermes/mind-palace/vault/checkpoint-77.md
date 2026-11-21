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
