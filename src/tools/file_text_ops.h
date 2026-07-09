#ifndef FILE_TEXT_OPS_H
#define FILE_TEXT_OPS_H

#include <stdbool.h>

/*
 * file_text_ops.h — self-contained text-shaping helpers for file operations.
 *
 * Stateless string transformers ported from tools/file_operations.py. No
 * global state, no god headers, no void* passthrough. Every function returns
 * a malloc'd string (caller frees) or a bool.
 *
 * Extracted from port_file_operations.c (v551 refactor-first monolith split)
 * and oracle-verified against the live Python equivalents (C == LIVE Python,
 * 0 mismatches) for the deterministic transformers.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Strip leaked terminal fence wrappers / ANSI+OSC escapes from file-read text.
 * Faithful to tools/file_operations.py:_strip_terminal_fence_leaks. */
char *file_text_ops_strip_terminal_fence_leaks(const char *text);

/* Detect dominant line ending in `sample`. Returns malloc'd "crlf" or "lf".
 * Faithful to tools/file_operations.py:_detect_line_ending — Python returns
 * "\r\n" / "\n" / None; None (no newline present) maps to "lf" here since C
 * callers need a concrete default. Python never yields a bare-CR "cr" token. */
char *file_text_ops_detect_line_ending(const char *sample);

/* Normalize all line endings in `text` to `target` ("\n" or "\r\n"). Idempotent.
 * Faithful to tools/file_operations.py:_normalize_line_endings. */
char *file_text_ops_normalize_line_endings(const char *text, const char *target);

/* Strip a single leading UTF-8 BOM. Returns malloc'd string. Faithful to
 * tools/file_operations.py:_strip_bom (had_bom discarded — caller uses
 * file_text_ops_has_bom when it needs the flag). */
char *file_text_ops_strip_bom(const char *text);

/* True when `text` begins with a UTF-8 BOM. Faithful to
 * tools/file_operations.py:_has_bom. */
bool file_text_ops_has_bom(const char *text);

/* Add compact `n|content` line numbers (no padding), truncating lines longer
 * than `max_line_length` (0 = no truncation). Faithful to
 * tools/file_operations.py:FileOperations._add_line_numbers. */
char *file_text_ops_add_line_numbers(const char *content, int start_line, int max_line_length);

/* Expand a leading "~" to $HOME. Returns malloc'd string. Faithful to
 * tools/file_operations.py:FileOperations._expand_path (tilde-only form). */
char *file_text_ops_expand_path(const char *path);

/* Escape a string for safe single-quoted shell use. Returns malloc'd string.
 * Faithful to tools/file_operations.py:FileOperations._escape_shell_arg. */
char *file_text_ops_escape_shell_arg(const char *arg);

/* Parse one `grep -n` / `rg -n` context line into a JSON object
 * {"line": <content>}. Faithful to tools/file_operations.py:_parse_search_context_line. */
char *file_text_ops_parse_search_context_line(const char *line);

#ifdef __cplusplus
}
#endif

#endif /* FILE_TEXT_OPS_H */
