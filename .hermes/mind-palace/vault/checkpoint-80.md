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
