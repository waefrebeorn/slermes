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
