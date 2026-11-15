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
