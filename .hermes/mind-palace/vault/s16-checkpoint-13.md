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

# Checkpoint 13 — TD21/TD22 Reclassifications

**Changes:**
1. **TD21 N/A** — `tools/managed_tool_gateway.py`: Python-specific Nous gateway routing (OAuth token resolution, vendor gateway URL building). C uses direct API calls, no vendor gateway system.
2. **TD22 N/A** — `tools/neutts_synth.py`: Python TTS subprocess helper (calls `neutts` Python package). C has native TTS (`src/tools/tts.c`).

**Net gaps closed: 2 reclassifications**
