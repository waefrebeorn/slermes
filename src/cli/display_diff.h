/* display_diff.h - public API for the diff-rendering subsystem.
 * Opaque to callers: only these symbols are exported. */
#ifndef SLERMES_DISPLAY_DIFF_H
#define SLERMES_DISPLAY_DIFF_H

#include <stdbool.h>

/* Skin-aware ANSI diff color accessors (port of display.py _diff_*). */
const char *display_diff_dim(void);
const char *display_diff_file(void);
const char *display_diff_hunk(void);
const char *display_diff_minus(void);
const char *display_diff_plus(void);

/* Unified-diff pipeline (port of display.py). Caller frees returned strings. */
char  *display_split_diff_sections(const char *diff);
char  *display_summarize_diff(const char *diff, int max_files, int max_lines);
char  *display_extract_edit_diff(const char *tool_name, const char *result_json,
                                 const char *function_args_json,
                                 const char *pre_content, const char *post_content);
bool   display_emit_inline_diff(const char *diff_text, void (*print_fn)(const char *));
bool   display_render_edit_diff(const char *tool_name, const char *result_json,
                                const char *function_args_json,
                                const char *pre_content, const char *post_content,
                                void (*print_fn)(const char *));
char  *display_inline_diff(const char *diff_text);

#endif /* SLERMES_DISPLAY_DIFF_H */
