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

# Checkpoint 57 — AG10 shell_hooks: allowlist check, record, revoke

**Battleship:** v83→v84

## What was done

### AG10: 3 allowlist functions ported

- `shell_hooks_allowlist_check()` — port of Python `_is_allowlisted()` / `load_allowlist()`
- `shell_hooks_allowlist_record()` — port of Python `save_allowlist()`
- `shell_hooks_revoke()` — port of Python `revoke()` — removes allowlist entries by command
- Previously these were static functions in shell_hooks.c; now publicly declared in hermes_hooks.h
- Functions: `src/agent/shell_hooks.c:140-166,169-203,463-510`
- Headers: `include/hermes_hooks.h:147-159`

### Build/Test
- Clean compile, 0 errors
- 4/4 tests passing

### Classification Changes
- AG10 shell_hooks: 1/11 → 4/11 public functions ported (9% → 36%)