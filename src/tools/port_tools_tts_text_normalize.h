/*
 * port_tools_tts_text_normalize.h — TTS text normalization helpers.
 *
 * C11 port of tools/tts_text_normalize.py.
 * Pure deterministic text transformations for speech synthesis.
 * No I/O, no network — just string processing.
 *
 * Each function returns the result via two out-parameters so that results
 * containing the heading sentinel NUL byte (\x00, Python's _HEAD) are
 * preserved byte-for-byte — strlen() would truncate them. The caller frees
 * `*out`. Pass 0 for max_chars on prepare_spoken_text for "no limit" (the
 * Python default is 4000; callers passing a positive int get truncation).
 */

#ifndef PORT_TOOLS_TTS_TEXT_NORMALIZE_H
#define PORT_TOOLS_TTS_TEXT_NORMALIZE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Strip Markdown/Telegram formatting while preserving readable words.
 * Python: strip_markdown_for_tts(text)
 */
char *tts_strip_markdown(const char *text, size_t *out_len);

/**
 * Expand common symbols/shorthand into words a TTS engine reads well.
 * Python: normalize_symbols_for_tts(text)
 */
char *tts_normalize_symbols(const char *text, size_t *out_len);

/**
 * Collapse visual formatting into calm spoken paragraphs.
 * Python: smooth_whitespace_for_tts(text)
 */
char *tts_smooth_whitespace(const char *text, size_t *out_len);

/**
 * Remove blocks that must never reach a speech provider.
 * Python: strip_nonspoken_blocks(text)
 */
char *tts_strip_nonspoken_blocks(const char *text, size_t *out_len);

/**
 * Collapse newlines into sentence breaks for single-line TTS payloads.
 * Python: flatten_newlines_for_payload(text)
 */
char *tts_flatten_newlines(const char *text, size_t *out_len);

/**
 * Return a TTS-friendly script from assistant text.
 * Python: prepare_spoken_text(text, max_chars)
 * max_chars <= 0 => no limit.
 */
char *tts_prepare_spoken_text(const char *text, int max_chars, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* PORT_TOOLS_TTS_TEXT_NORMALIZE_H */
