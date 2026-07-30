# Checkpoint 79 — CP79/v106: AG09 display.py: _trim_error + _detect_tool_failure ported (2/30)

## Changes
- `display_trim_error()` — shrinks error messages for inline display, strips long absolute paths to just filename, truncates to 60 chars
- `display_detect_tool_failure()` — inspects tool result string for failure indicators (terminal exit codes, memory store-full, JSON error fields, generic error heuristics)
- Both ported from Python agent/display.py

## Evidence
- `src/cli/display_core.c:1291-1416` — function implementations
- `include/hermes_display.h:234-247` — header declarations
- Depends on `file_mutation_result_landed()` from `include/hermes_tool_result.h`

## Impact
- **AG09:** 0/30 → 2/30 (7%) — first display.py functions ported to C
- **Build:** Clean, 0 errors. Tests: 4/4 pass
