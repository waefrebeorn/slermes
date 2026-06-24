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
