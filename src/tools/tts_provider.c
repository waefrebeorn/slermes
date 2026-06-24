/*
 * tts_provider.c — Name parity wrapper for Python agent/tts_provider.py
 *
 * NOTE: The C implementation lives in src/tools/tts.c, not here.
 * This file exists ONLY for name parity so that every Python module
 * has a correspondingly-named C file.
 *
 * Port of Python agent/tts_provider.py.
 * C implementation: src/tools/tts.c
 *
 * Key functions ported:
 *   Text-to-speech provider tool. C implementation in src/tools/tts.c: tts_synthesize, tts_list_voices, tts_get_provider, tts_is_available, tts_save_audio.
 */
