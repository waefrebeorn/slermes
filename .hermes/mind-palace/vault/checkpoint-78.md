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
