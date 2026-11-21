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
