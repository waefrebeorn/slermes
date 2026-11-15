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
