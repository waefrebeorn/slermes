# Checkpoint 84 — Battleship v111

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
