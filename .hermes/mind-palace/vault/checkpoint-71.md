# Checkpoint 71 — CP71/v98: AG10 allowlist_entry_for + script_is_executable ported

## Changes
- `shell_hooks_allowlist_entry_for()` — loads allowlist JSON, iterates entries,
  returns malloc'd JSON string for matching event+command pair (or NULL)
- `shell_hooks_script_is_executable()` — stat(S_IXUSR) check on command path

## Evidence
- `src/agent/shell_hooks.c:517-563`
- `include/hermes_hooks.h:163-172`

## Impact
- **AG10:** 5/11 -> 7/11 public functions (63%)
- **Build:** Clean, 0 errors. Tests: 4/4 pass
- **Commit:** `17c8dc8e6`
