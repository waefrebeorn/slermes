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

# Checkpoint 84 — Battleship 

## Implementation: Edit diff path helpers

### Ported Functions
1. **`display_resolved_path()`** — `src/cli/display_core.c:2010-2062`
   Port of Python `_resolved_path()`. Resolves ~ expansion, prepends cwd for relative paths,
   calls realpath() for canonicalization. Falls back gracefully when file doesn't exist.

2. **`display_snapshot_text()`** — `src/cli/display_core.c:2066-2082`
   Port of Python `_snapshot_text()`. Reads file content in binary mode, returns malloc'd string.
   Caps at 1MB to avoid OOM. Returns NULL on any error.

3. **`display_diff_path()`** — `src/cli/display_core.c:2086-2098`
   Port of Python `_display_diff_path()`. Strips cwd prefix to show relative paths in diffs.

### Declared in `include/hermes_display.h:289-302`

### Build: clean, 0 errors, 0 warnings. Tests: 4/4 pass.
