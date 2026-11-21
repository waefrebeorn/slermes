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
