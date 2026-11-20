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