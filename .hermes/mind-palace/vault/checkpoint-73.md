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
