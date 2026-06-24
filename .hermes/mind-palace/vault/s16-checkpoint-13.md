# Checkpoint 13 — TD21/TD22 Reclassifications

**Changes:**
1. **TD21 N/A** — `tools/managed_tool_gateway.py`: Python-specific Nous gateway routing (OAuth token resolution, vendor gateway URL building). C uses direct API calls, no vendor gateway system.
2. **TD22 N/A** — `tools/neutts_synth.py`: Python TTS subprocess helper (calls `neutts` Python package). C has native TTS (`src/tools/tts.c`).

**Net gaps closed: 2 reclassifications**
