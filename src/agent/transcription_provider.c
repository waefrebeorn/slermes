/*
 * transcription_provider.c — Transcription Provider ABC (N/A stub).
 *
 * Port of Python agent/transcription_provider.py (193 lines).
 * This is a Python ABC for plugin-based STT backends. C has its own
 * transcription system via lib/libtranscribe/ and src/tools/transcribe.c.
 * All methods are N/A — Python ABC/plugin interface, not portable to C.
 *
 * N/A: TranscriptionProvider — Python ABC (6 abstract methods)
 * N/A: __init__() — SDK/property initialization
 * N/A: transcribe() — plugin dispatch, async
 * N/A: is_available() — plugin state check
 * N/A: name() — property
 * N/A: display_name() — property
 * N/A: get_setup_schema() — plugin config schema
 */

#include "hermes_core_types.h"

/* ── PoP declaration-only placeholders (no bodies) ── */
/* PoP: streaming_capable @ agent/transcription_provider.py:streaming_capable */
int agent_transcription_provider_streaming_capable(void) { return 0; }
/* PoP: open_stream_session @ agent/transcription_provider.py:open_stream_session */
void *agent_transcription_provider_open_stream_session(void) { return NULL; }
/* PoP: push_audio @ agent/transcription_provider.py:push_audio */
void agent_transcription_provider_push_audio(void) { }
/* PoP: partial_transcript @ agent/transcription_provider.py:partial_transcript */
void *agent_transcription_provider_partial_transcript(void) { return NULL; }
