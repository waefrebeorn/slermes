# Checkpoint 81 — Battleship v108

## Implementation: AG09 display.py — 4 new functions (8/30, 27%)

### Ported Functions

1. **`display_oneline()`** — `src/cli/display_core.c:1432-1448`
   Port of Python `display.py:_oneline()`. Collapses whitespace (newlines, tabs) to single spaces.
   Simple byte-scanning loop with last-space tracking. Writes into caller-provided buffer.

2. **`display_get_skin_tool_prefix()`** — `src/cli/display_core.c:1452-1458`
   Port of Python `display.py:get_skin_tool_prefix()`. Returns "┊" (U+250A) from active skin or default.
   Queries `g_display_skin` for `skin.tool_prefix`.

3. **`display_get_tool_emoji()`** — `src/cli/display_core.c:1462-1512`
   Port of Python `display.py:get_tool_emoji()`. Three-step resolution:
   (1) Check skin's `tool_emojis.<name>` override, (2) hardcoded map of ~28 tool→emoji entries,
   (3) "⚡" fallback. Uses UTF-8 byte sequences for emoji strings.

4. **`display_result_succeeded()`** — `src/cli/display_core.c:1516-1534`
   Port of Python `display.py:_result_succeeded()`. Parses JSON result, checks for `error` field
   (failure), checks `success` field (boolean). Returns false for non-object or null results.

### Declarations added to `include/hermes_display.h:259-282`

### Build: clean, 0 errors. Tests: 4/4 pass.
