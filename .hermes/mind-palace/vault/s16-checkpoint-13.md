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

# Checkpoint 13 — TD21/TD22 Reclassifications

**Changes:**
1. **TD21 N/A** — `tools/managed_tool_gateway.py`: Python-specific Nous gateway routing (OAuth token resolution, vendor gateway URL building). C uses direct API calls, no vendor gateway system.
2. **TD22 N/A** — `tools/neutts_synth.py`: Python TTS subprocess helper (calls `neutts` Python package). C has native TTS (`src/tools/tts.c`).

**Net gaps closed: 2 reclassifications**
