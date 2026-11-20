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
