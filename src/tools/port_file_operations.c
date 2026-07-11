/**
 * port_file_operations.c — Port of Python: tools/file_operations.py
 *
 * Real C implementations for file operation helpers.
 */

#include "port_file_operations.h"
#include "file_text_ops.h"
#include "file_fs_ops.h"
#include "file_pagination_ops.h"
#include "hermes_logger.h"
#include "hermes_json.h"
#include "hermes_file_safety.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <stddef.h>

/* Opaque struct definition - private to this translation unit */
struct port_file_operations_state {
    size_t max_file_size;
    bool bom_checked;
};

port_file_operations_state_t *port_file_operations_state_init(void)
{
    port_file_operations_state_t *state = calloc(1, sizeof(*state));
    if (!state) return NULL;
    state->max_file_size = 10 * 1024 * 1024;  /* 10MB default */
    state->bom_checked = false;
    return state;
}

void port_file_operations_state_cleanup(port_file_operations_state_t *state)
{
    if (!state) return;
    free(state);
}

/* ================================================================
 *  Stateless text-shaping helpers.
 *  Extracted to src/tools/file_text_ops.{h,c} (v551 refactor-first
 *  monolith split); each public entry point delegates to that
 *  self-contained, oracle-verified module.
 * ================================================================ */

/* PoP: file_ops_strip_terminal_fence_leaks @ tools/file_operations.py:_strip_terminal_fence_leaks */
char *file_ops_strip_terminal_fence_leaks(const char *text)
{
    return file_text_ops_strip_terminal_fence_leaks(text);
}

/* PoP: file_ops_detect_line_ending @ tools/file_operations.py:_detect_line_ending */
char *file_ops_detect_line_ending(const char *text)
{
    return file_text_ops_detect_line_ending(text);
}

/* PoP: file_ops_normalize_line_endings @ tools/file_operations.py:_normalize_line_endings */
char *file_ops_normalize_line_endings(const char *text, const char *target)
{
    return file_text_ops_normalize_line_endings(text, target);
}

/* PoP: file_ops_strip_bom @ tools/file_operations.py:_strip_bom */
char *file_ops_strip_bom(const char *text)
{
    return file_text_ops_strip_bom(text);
}

/* PoP: file_ops_has_bom @ tools/file_operations.py:_has_bom */
bool file_ops_has_bom(const char *text)
{
    return file_text_ops_has_bom(text);
}

/* ================================================================
 *  Search context line parsing  (delegates → src/tools/file_text_ops.c)
 * ================================================================ */

/* PoP: file_ops_parse_search_context_line @ tools/file_operations.py:_parse_search_context_line */
char *file_ops_parse_search_context_line(const char *line)
{
    return file_text_ops_parse_search_context_line(line);
}
/* ================================================================
 *  File read/write operations  (extracted → src/tools/file_fs_ops.c)
 * ================================================================ */

/* PoP: file_ops_read_file_raw @ tools/file_operations.py:read_file_raw */
char *file_ops_read_file_raw(const char *path)
{
    return file_fs_ops_read_file_raw(path);
}

/* PoP: file_ops_delete_path @ tools/file_operations.py:delete_path */
char *file_ops_delete_path(const char *path) { return (char*)file_fs_ops_delete_path(path); }

/* PoP: file_ops_python_delete @ tools/file_operations.py:_python_delete */
bool file_ops_python_delete(const char *path)
{
    return file_fs_ops_python_delete(path);
}

/* ================================================================
 *  Patch operations
 * ================================================================ */

/* Port of Python: patch_replace */
/* PoP: file_ops_patch_replace @ tools/file_operations.py:patch_replace
 * delegates to the first-occurrence replace primitive in file_fs_ops.c */
char *file_ops_patch_replace(const char *content, const char *old_text, const char *new_text)
{
    return file_fs_ops_patch_replace(content, old_text, new_text);
}

/* NOTE: tools/file_operations.py:patch_v4a (V4A patch applier) is NOT yet
 * ported. The earlier stub (return strdup(content)) was removed because it
 * faked a working port. This is an honest REAL_GAP: the C file tool does not
 * yet apply V4A-format patches. Tracked as a gap, not a phantom port. */

/* ================================================================
 *  Linting helpers  (extracted → src/tools/file_ops_lint.c)
 * ================================================================ */
/* file_ops_looks_like_linter_unusable moved to file_ops_lint.c */

/* ================================================================
 *  Pagination helpers
 * ================================================================ */

/* PoP: file_ops_normalize_read_pagination @ tools/file_operations.py:normalize_read_pagination
 * delegates to file_pagination_ops_normalize_read_pagination */
char *file_ops_normalize_read_pagination(int offset, int limit, int default_limit)
{
    return file_pagination_ops_normalize_read_pagination(offset, limit, default_limit);
}

/* PoP: file_ops_normalize_search_pagination @ tools/file_operations.py:normalize_search_pagination
 * delegates to file_pagination_ops_normalize_search_pagination */
char *file_ops_normalize_search_pagination(int offset, int limit, int default_limit)
{
    return file_pagination_ops_normalize_search_pagination(offset, limit, default_limit);
}

/* ================================================================
 *  Command execution helpers
 * ================================================================ */

/* Port of Python: _exec */
/* PoP: file_ops_exec @ tools/file_operations.py:_exec */
char *file_ops_exec(const char *cmd, char **env)
{
    (void)env;
    if (!cmd) return strdup("");

    FILE *fp = popen(cmd, "r");
    if (!fp) return strdup("");

    char *result = NULL;
    size_t size = 0;
    char buf[1024];
    while (fgets(buf, sizeof(buf), fp)) {
        size_t len = strlen(buf);
        char *new_result = realloc(result, size + len + 1);
        if (!new_result) { free(result); pclose(fp); return NULL; }
        result = new_result;
        memcpy(result + size, buf, len);
        size += len;
        result[size] = '\0';
    }
    pclose(fp);
    return result ? result : strdup("");
}

/* Port of Python: _has_command */
/* PoP: file_ops_has_command @ tools/file_operations.py:_has_command */
bool file_ops_has_command(const char *cmd)
{
    if (!cmd) return false;
    char buf[256];
    snprintf(buf, sizeof(buf), "which %s >/dev/null 2>&1", cmd);
    return system(buf) == 0;
}

/* ================================================================
 *  File type detection
 * ================================================================ */

/* Port of Python: _is_likely_binary */
/* PoP: file_ops_is_likely_binary @ tools/file_operations.py:_is_likely_binary
 * delegates to file_fs_ops_is_likely_binary (faithful BINARY_EXTENSIONS + 30% rule) */
bool file_ops_is_likely_binary(const char *path)
{
    return file_fs_ops_is_likely_binary(path);
}

/* Port of Python: _is_image */
/* PoP: file_ops_is_image @ tools/file_operations.py:_is_image
 * delegates to file_fs_ops_is_image (faithful IMAGE_EXTENSIONS, includes .ico) */
bool file_ops_is_image(const char *path)
{
    return file_fs_ops_is_image(path);
}

/* ================================================================
 *  Path & content helpers
 * ================================================================ */

/* Port of Python: _add_line_numbers */
/* PoP: file_ops_add_line_numbers @ tools/file_operations.py:_add_line_numbers */
char *file_ops_add_line_numbers(const char *content)
{
    return file_text_ops_add_line_numbers(content, 1, 0);
}

/* PoP: file_ops_expand_path @ tools/file_operations.py:_expand_path */
char *file_ops_expand_path(const char *path)
{
    return file_text_ops_expand_path(path);
}

/* PoP: file_ops_escape_shell_arg @ tools/file_operations.py:_escape_shell_arg */
char *file_ops_escape_shell_arg(const char *arg)
{
    return file_text_ops_escape_shell_arg(arg);
}

/* ================================================================
 *  File detection helpers
 * ================================================================ */

/* Port of Python: _detect_file_line_ending */
/* PoP: file_ops_detect_file_line_ending @ tools/file_operations.py:_detect_file_line_ending
 * delegates to file_fs_ops_detect_file_line_ending */
char *file_ops_detect_file_line_ending(const char *path)
{
    return file_fs_ops_detect_file_line_ending(path);
}

/* PoP: file_ops_file_has_bom @ tools/file_operations.py:_file_has_bom
 * delegates to file_fs_ops_file_has_bom */
bool file_ops_file_has_bom(const char *path)
{
    return file_fs_ops_file_has_bom(path);
}

/* Port of Python: _unified_diff */

/* ================================================================
 *  File similarity & search
 * ================================================================ */

/* Port of Python: _suggest_similar_files */

/* Port of Python: _search_files */

/* Port of Python: _search_files_rg */

/* Port of Python: _search_content */

/* Port of Python: _search_with_rg */

/* Port of Python: _search_with_grep */

/* ================================================================
 *  LSP helpers
 * ================================================================ */

/* Port of Python: _lsp_local_only */

/* Port of Python: _lsp_handles_extension */

/* Port of Python: _lsp_will_handle */

/* Port of Python: _snapshot_lsp_baseline */


/* Port of Python: _maybe_lsp_diagnostics */


/* Port of Python: _check_lint */


/* Port of Python: _check_lint_delta */


/* ================================================================
 *  Existing functions (with proper PoP annotations)
 * ================================================================ */

/* Port of Python: _densify_matches */


/* PoP: file_ops_is_line_oriented_newline_error @ tools/file_operations.py:_is_line_oriented_newline_error
 * delegates to file_pagination_ops_is_line_oriented_newline_error */
bool file_ops_is_line_oriented_newline_error(const char *error)
{
    return file_pagination_ops_is_line_oriented_newline_error(error);
}

/* PoP: file_ops_maybe_warn_line_oriented_newline_pattern @ tools/file_operations.py:_maybe_warn_line_oriented_newline_pattern
 * delegates to file_pagination_ops_maybe_warn_line_oriented_newline_pattern */
json_t *file_ops_maybe_warn_line_oriented_newline_pattern(json_t *result, const char *pattern)
{
    return file_pagination_ops_maybe_warn_line_oriented_newline_pattern(result, pattern);
}

/* PoP: file_ops_pattern_has_regex_newline @ tools/file_operations.py:_pattern_has_regex_newline
 * delegates to file_pagination_ops_pattern_has_regex_newline */
bool file_ops_pattern_has_regex_newline(const char *pattern)
{
    return file_pagination_ops_pattern_has_regex_newline(pattern);
}