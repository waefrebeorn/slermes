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
