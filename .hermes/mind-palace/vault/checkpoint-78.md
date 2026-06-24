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
