#ifndef PORT_INPUT_SANITIZE_H
#define PORT_INPUT_SANITIZE_H

#include <stddef.h>

/* Port of Python: hermes_cli/input_sanitize.py
 *
 * Prompt-text scrubbing for terminal / desktop bracketed-paste leaks.
 * All functions return a heap string (caller frees) and never modify the
 * input. A NULL input returns NULL; an empty input returns an empty string,
 * matching the Python `if not text: return text` guard.
 */

/* Strip canonical + degraded bracketed-paste wrapper markers. */
char *input_sanitize_strip_leaked_bracketed_paste_wrappers(const char *text);

/* Drop a trailing run of the desktop "~[[e" corruption signature when it
 * repeats at least min_repeats times (Python default: 4). */
char *input_sanitize_collapse_repeated_input_artifacts(const char *text,
                                                       int min_repeats);

/* Normalize user-authored prompt text: strip wrappers, then collapse the
 * repeated-artifact tail. */
char *input_sanitize_sanitize_user_prompt_text(const char *text);

#endif /* PORT_INPUT_SANITIZE_H */
