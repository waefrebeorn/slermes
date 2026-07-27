/* input_sanitize.h — opaque-free interface for the faithful C11 port of
 * hermes_cli/input_sanitize.py
 *
 * Sanitize user prompt text leaked from terminal / paste control sequences.
 * Pure string logic (no IO). Exposes sanitize_user_prompt_text() and its two
 * helpers. Self-contained; minimal includes. Reuses lib/ base string utils
 * plus a small local str_remove_all helper (CRT strstr).
 */

#ifndef SLERMES_INPUT_SANITIZE_H
#define SLERMES_INPUT_SANITIZE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Strip leaked bracketed-paste wrapper markers from user-visible text.
 * Caller frees the returned string. Returns NULL on alloc failure. */
char *input_sanitize_strip_leaked_bracketed_paste_wrappers(const char *text);

/* Drop a trailing run of the desktop ~[[e corruption signature.
 * Caller frees the returned string. Returns NULL on alloc failure. */
char *input_sanitize_collapse_repeated_input_artifacts(const char *text,
                                                       int min_repeats);

/* Normalize user-authored prompt text before persistence / model input.
 * Caller frees the returned string. Returns NULL on alloc failure or when
 * text is NULL/empty (mirrors Python returning the input unchanged). */
char *sanitize_user_prompt_text(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_INPUT_SANITIZE_H */
