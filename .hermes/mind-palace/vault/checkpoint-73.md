# Checkpoint 73 — CP73/v100: AG10 load_allowlist + save_allowlist ported

## Changes
- `shell_hooks_allowlist_load()` — loads allowlist JSON file, returns json_t* or NULL
- `shell_hooks_allowlist_save()` — serializes json_t* to allowlist file

## Evidence
- `src/agent/shell_hooks.c:515-548`
- `include/hermes_hooks.h:168-179`

## Impact
- **AG10:** 8/11 -> 10/11 public functions (91%)
- **Build:** Clean, 0 errors. Tests: 4/4 pass
- **Commit:** `58213e06a`
