/*
 * jiter_preload.c — Name parity wrapper for Python agent/jiter_preload.py
 *
 * NOTE: The C implementation lives in src/hermes_tokenizer.c, not here.
 * This file exists ONLY for name parity so that every Python module
 * has a correspondingly-named C file.
 *
 * Port of Python agent/jiter_preload.py.
 * C implementation: src/hermes_tokenizer.c
 *
 * Key functions ported:
 *   Jiter preload / tokenizer init. C implementation in src/hermes_tokenizer.c: tokenizer_init, tokenizer_preload, tokenizer_count_tokens, tokenizer_free.
 */
