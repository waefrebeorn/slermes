/*
 * tts_registry.c — Name parity wrapper for Python agent/tts_registry.py
 *
 * NOTE: The C implementation lives in src/tools/tts.c, not here.
 * This file exists ONLY for name parity so that every Python module
 * has a correspondingly-named C file.
 *
 * Port of Python agent/tts_registry.py.
 * C implementation: src/tools/tts.c
 *
 * Key functions ported:
 *   TTS provider registry. C implementation in src/tools/tts.c: tts_register_provider, tts_get_provider, tts_list_providers, tts_unregister_provider.
 */
