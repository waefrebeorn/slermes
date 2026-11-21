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
