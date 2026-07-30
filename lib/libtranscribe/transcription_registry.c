/*
 * transcription_registry.c — Name parity wrapper for Python agent/transcription_registry.py
 *
 * NOTE: The C implementation lives in lib/libtranscribe/transcribe.c, not here.
 * This file exists ONLY for name parity so that every Python module
 * has a correspondingly-named C file.
 *
 * Port of Python agent/transcription_registry.py.
 * C implementation: lib/libtranscribe/transcribe.c
 *
 * Key functions ported:
 *   Transcription provider registry. C implementation in lib/libtranscribe/transcribe.c: transcription_register_provider, transcription_get_provider, transcription_list_providers, transcribe_audio_file, transcription_is_available.
 */
