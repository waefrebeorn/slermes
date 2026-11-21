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

# Checkpoint 13 — TD21/TD22 Reclassifications

**Changes:**
1. **TD21 N/A** — `tools/managed_tool_gateway.py`: Python-specific Nous gateway routing (OAuth token resolution, vendor gateway URL building). C uses direct API calls, no vendor gateway system.
2. **TD22 N/A** — `tools/neutts_synth.py`: Python TTS subprocess helper (calls `neutts` Python package). C has native TTS (`src/tools/tts.c`).

**Net gaps closed: 2 reclassifications**
