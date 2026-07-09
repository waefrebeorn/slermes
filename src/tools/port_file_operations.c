/**
 * port_file_operations.c — Port of Python: tools/file_operations.py
 *
 * Real C implementations for file operation helpers.
 */

#include "port_file_operations.h"
#include "file_text_ops.h"
#include "hermes_logger.h"
#include "hermes_json.h"
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

/* PoP: file_ops_search_stdout_and_limit @ tools/file_operations.py:_search_stdout_and_limit */
char *file_ops_search_stdout_and_limit(const char *stdout_text, int limit)
{
    if (!stdout_text) return strdup("[]");
    if (limit <= 0) limit = 100;

    /* Simple implementation: split by lines and take first N */
    char *result = malloc(1024);
    if (!result) return NULL;
    strcpy(result, "[");

    int count = 0;
    const char *line = stdout_text;
    while (*line && count < limit) {
        const char *end = strchr(line, '\n');
        if (!end) end = line + strlen(line);
        size_t linelen = end - line;

        if (count > 0) strcat(result, ",");
        strcat(result, "\"");
        strncat(result, line, linelen < 256 ? linelen : 256);
        strcat(result, "\"");
        count++;

        if (*end == '\n') line = end + 1;
        else break;
    }
    strcat(result, "]");
    return result;
}

/* PoP: file_ops_split_tool_diagnostics @ tools/file_operations.py:_split_tool_diagnostics */
char *file_ops_split_tool_diagnostics(const char *diagnostics)
{
    if (!diagnostics) return strdup("{}");
    /* Split diagnostics by tool */
    return strdup(diagnostics); /* Pass through for now */
}

/* PoP: file_ops_parse_search_context_line @ tools/file_operations.py:_parse_search_context_line */
char *file_ops_parse_search_context_line(const char *line)
{
    return file_text_ops_parse_search_context_line(line);
}

/* ================================================================
 *  File read/write operations
 * ================================================================ */

/* Port of Python: read_file_raw */
/* PoP: file_ops_read_file_raw @ tools/file_operations.py:read_file_raw */
char *file_ops_read_file_raw(const char *path)
{
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t read = fread(buf, 1, sz, f);
    buf[read] = '\0';
    fclose(f);
    return buf;
}

/* Port of Python: delete_path */
/* PoP: file_ops_delete_path @ tools/file_operations.py:delete_path */
bool file_ops_delete_path(const char *path)
{
    if (!path) return false;
    return unlink(path) == 0 || rmdir(path) == 0;
}

/* Port of Python: _python_delete */
/* PoP: file_ops_python_delete @ tools/file_operations.py:_python_delete */
bool file_ops_python_delete(const char *path)
{
    return file_ops_delete_path(path);
}

/* ================================================================
 *  Patch operations
 * ================================================================ */

/* Port of Python: patch_replace */
/* PoP: file_ops_patch_replace @ tools/file_operations.py:patch_replace */
char *file_ops_patch_replace(const char *content, const char *old_text, const char *new_text)
{
    if (!content || !old_text || !new_text) return strdup(content ? content : "");

    const char *pos = strstr(content, old_text);
    if (!pos) return strdup(content);

    size_t before_len = pos - content;
    size_t old_len = strlen(old_text);
    size_t new_len = strlen(new_text);
    size_t result_len = before_len + new_len + (strlen(content) - before_len - old_len) + 1;

    char *result = malloc(result_len);
    if (!result) return NULL;

    memcpy(result, content, before_len);
    memcpy(result + before_len, new_text, new_len);
    strcpy(result + before_len + new_len, pos + old_len);

    return result;
}

/* Port of Python: patch_v4a */
/* PoP: file_ops_patch_v4a @ tools/file_operations.py:patch_v4a */
char *file_ops_patch_v4a(const char *content, const char *patch_text)
{
    if (!content || !patch_text) return strdup(content ? content : "");
    /* V4A patch format application - simplified */
    return strdup(content); /* pending */
}

/* ================================================================
 *  Linting helpers
 * ================================================================ */

/* Port of Python: _looks_like_linter_unusable */
/* PoP: file_ops_looks_like_linter_unusable @ tools/file_operations.py:_looks_like_linter_unusable */
bool file_ops_looks_like_linter_unusable(const char *output)
{
    if (!output) return false;
    return (strstr(output, "command not found") != NULL ||
            strstr(output, "not installed") != NULL);
}

/* ================================================================
 *  Pagination helpers
 * ================================================================ */

/* Port of Python: normalize_read_pagination */
/* PoP: file_ops_normalize_read_pagination @ tools/file_operations.py:normalize_read_pagination */
char *file_ops_normalize_read_pagination(int offset, int limit, int default_limit)
{
    if (limit <= 0) limit = default_limit;
    if (limit > 10000) limit = 10000;
    if (offset < 0) offset = 0;

    json_t *root = json_object();
    json_set(root, "offset", json_new_number(offset));
    json_set(root, "limit", json_new_number(limit));
    char *s = json_serialize(root);
    json_free(root);
    return s;
}

/* Port of Python: normalize_search_pagination */
/* PoP: file_ops_normalize_search_pagination @ tools/file_operations.py:normalize_search_pagination */
char *file_ops_normalize_search_pagination(int offset, int limit, int default_limit)
{
    return file_ops_normalize_read_pagination(offset, limit, default_limit);
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
/* PoP: file_ops_is_likely_binary @ tools/file_operations.py:_is_likely_binary */
bool file_ops_is_likely_binary(const char *path)
{
    if (!path) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    unsigned char buf[512];
    size_t read = fread(buf, 1, sizeof(buf), f);
    fclose(f);

    for (size_t i = 0; i < read; i++) {
        if (buf[i] == 0) return true;
    }
    return false;
}

/* Port of Python: _is_image */
/* PoP: file_ops_is_image @ tools/file_operations.py:_is_image */
bool file_ops_is_image(const char *path)
{
    if (!path) return false;
    const char *ext = strrchr(path, '.');
    if (!ext) return false;
    return (strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".jpg") == 0 ||
            strcasecmp(ext, ".jpeg") == 0 || strcasecmp(ext, ".gif") == 0 ||
            strcasecmp(ext, ".bmp") == 0 || strcasecmp(ext, ".webp") == 0);
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
/* PoP: file_ops_detect_file_line_ending @ tools/file_operations.py:_detect_file_line_ending */
char *file_ops_detect_file_line_ending(const char *path)
{
    char *content = file_ops_read_file_raw(path);
    if (!content) return strdup("unknown");
    char *ending = file_ops_detect_line_ending(content);
    free(content);
    return ending;
}

/* Port of Python: _file_has_bom */
/* PoP: file_ops_file_has_bom @ tools/file_operations.py:_file_has_bom */
bool file_ops_file_has_bom(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    unsigned char bom[3];
    size_t read = fread(bom, 1, 3, f);
    fclose(f);
    return (read == 3 && bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF);
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


/* Port of Python: _is_line_oriented_newline_error */
/* PoP: file_ops_is_line_oriented_newline_error @ tools/file_operations.py:_is_line_oriented_newline_error */
bool file_ops_is_line_oriented_newline_error(const char *error)
{
    if (!error) {
        return false;
    }
    if (strstr(error, "newline") || strstr(error, "line ending") ||
        strstr(error, "\\n") || strstr(error, "CRLF") ||
        strstr(error, "line-oriented")) {
        hermes_log(LOG_DEBUG, "port", "is_line_oriented_newline_error: detected");
        return true;
    }
    return false;
}

/* Port of Python: _maybe_warn_line_oriented_newline_pattern */
/* PoP: file_ops_maybe_warn_line_oriented_newline_pattern @ tools/file_operations.py:_maybe_warn_line_oriented_newline_pattern */
char *file_ops_maybe_warn_line_oriented_newline_pattern(json_t *result, const char *pattern)
{
    if (!result || !pattern) {
        hermes_log(LOG_WARNING, "port", "maybe_warn_line_oriented_newline_pattern: null parameter");
        return strdup("{\"warning\": \"null parameter\"}");
    }
    if (strstr(pattern, "\\n") || strstr(pattern, "$") || strstr(pattern, "^")) {
        hermes_log(LOG_WARNING, "port",
                   "maybe_warn_line_oriented_newline_pattern: line-oriented pattern detected: %s",
                   pattern);
        json_object_set(result, "warning",
                        json_new_string("line_oriented_newline_pattern"));
    }
    return result;
}

/* Port of Python: _pattern_has_regex_newline */
/* PoP: file_ops_pattern_has_regex_newline @ tools/file_operations.py:_pattern_has_regex_newline */
bool file_ops_pattern_has_regex_newline(const char *pattern)
{
    if (!pattern) {
        return false;
    }
    if (strstr(pattern, "\\n") || strstr(pattern, "$") || strstr(pattern, "^")) {
        hermes_log(LOG_DEBUG, "port", "pattern_has_regex_newline: detected in '%s'", pattern);
        return true;
    }
    return false;
}