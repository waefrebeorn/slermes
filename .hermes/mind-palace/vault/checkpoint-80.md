# Checkpoint 80 — CP80/v107: AG09 display: set/get_tool_preview_max_len ported (4/30, 13%)

## Changes
- `set_tool_preview_max_len(int n)` — sets global max preview length (0=unlimited)
- `get_tool_preview_max_len()` — returns configured max preview length
- Both ported from Python agent/display.py

## Evidence
- `src/cli/display_core.c:1417-1434` — function implementations
- `include/hermes_display.h:250-257` — header declarations

## Impact
- **AG09:** 2/30 → 4/30 (13%) — _trim_error, _detect_tool_failure, set/get_tool_preview_max_len
- **Build:** Clean, 0 errors. Tests: 4/4 pass
