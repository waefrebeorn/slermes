/**
 * port_file_operations.c — Port of Python: tools/file_operations.py
 *
 * Real C implementations for file operation helpers.
 */

#include "port_file_operations.h"
#include "file_text_ops.h"
#include "file_fs_ops.h"
#include "file_pagination_ops.h"
#include "patch.h"
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
/* PoP: file_ops_patch_replace @ tools/file_operations.py:patch_replace */
/* delegates to the first-occurrence replace primitive in file_fs_ops.c */
char *file_ops_patch_replace(const char *content, const char *old_text, const char *new_text)
{
    return file_fs_ops_patch_replace(content, old_text, new_text);
}

/* PoP: file_ops_patch_v4a @ tools/file_operations.py:patch_v4a */
char *file_ops_patch_v4a(const char *patch_content)
{
    if (!patch_content) return strdup("{\"error\":\"patch content required\"}");
    return patch_apply_v4a(patch_content, false);
}

/* ================================================================
 *  Linting helpers  (extracted → src/tools/file_ops_lint.c)
 * ================================================================ */
/* file_ops_looks_like_linter_unusable moved to file_ops_lint.c */

/* ================================================================
 *  Pagination helpers
 * ================================================================ */

/* PoP: file_ops_normalize_read_pagination @ tools/file_operations.py:normalize_read_pagination */
/* delegates to file_pagination_ops_normalize_read_pagination */
char *file_ops_normalize_read_pagination(int offset, int limit, int default_limit)
{
    return file_pagination_ops_normalize_read_pagination(offset, limit, default_limit);
}

/* PoP: file_ops_normalize_search_pagination @ tools/file_operations.py:normalize_search_pagination */
/* delegates to file_pagination_ops_normalize_search_pagination */
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
/* PoP: file_fs_ops_is_likely_binary @ tools/file_operations.py:_is_likely_binary */
/* delegates to file_fs_ops_is_likely_binary (faithful BINARY_EXTENSIONS + 30% rule) */
bool file_ops_is_likely_binary(const char *path)
{
    return file_fs_ops_is_likely_binary(path);
}

/* Port of Python: _is_image */
/* PoP: file_fs_ops_is_image @ tools/file_operations.py:_is_image */
/* delegates to file_fs_ops_is_image (faithful IMAGE_EXTENSIONS, includes .ico) */
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
/* PoP: file_ops_detect_file_line_ending @ tools/file_operations.py:_detect_file_line_ending */
/* delegates to file_fs_ops_detect_file_line_ending */
char *file_ops_detect_file_line_ending(const char *path)
{
    return file_fs_ops_detect_file_line_ending(path);
}

/* PoP: file_ops_file_has_bom @ tools/file_operations.py:_file_has_bom */
/* delegates to file_fs_ops_file_has_bom */
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


/* PoP: file_ops_is_line_oriented_newline_error @ tools/file_operations.py:_is_line_oriented_newline_error */
/* delegates to file_pagination_ops_is_line_oriented_newline_error */
bool file_ops_is_line_oriented_newline_error(const char *error)
{
    return file_pagination_ops_is_line_oriented_newline_error(error);
}

/* PoP: file_ops_maybe_warn_line_oriented_newline_pattern @ tools/file_operations.py:_maybe_warn_line_oriented_newline_pattern */
/* delegates to file_pagination_ops_maybe_warn_line_oriented_newline_pattern */
json_t *file_ops_maybe_warn_line_oriented_newline_pattern(json_t *result, const char *pattern)
{
    return file_pagination_ops_maybe_warn_line_oriented_newline_pattern(result, pattern);
}

/* PoP: file_ops_pattern_has_regex_newline @ tools/file_operations.py:_pattern_has_regex_newline */
/* delegates to file_pagination_ops_pattern_has_regex_newline */
bool file_ops_pattern_has_regex_newline(const char *pattern)
{
    return file_pagination_ops_pattern_has_regex_newline(pattern);
}

/* ================================================================\
 *  Search recovery helpers
 * =============================================================== */

/* Return true if *pattern* contains any regex metacharacter that the
 * Python _zero_match_probe checks for. */
static bool fops_pattern_has_metachars(const char *pattern) {
    if (!pattern) return false;
    /* . [ ] ( ) { } ? * + ^ $ |  (Python regex: [.\[\](){}?*+^$\|) */
    return strpbrk(pattern, ".[]{}?*+^$|\\") != NULL;
}

/* Trim leading/trailing whitespace (Python .strip()). */
static char *fops_strip(char *s) {
    if (!s) return s;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    char *e = s + strlen(s);
    while (e > s && (*(e-1) == ' ' || *(e-1) == '\t' || *(e-1) == '\n' || *(e-1) == '\r')) {
        *--e = '\0';
    }
    return s;
}

/* Parse rg --count-matches output lines, summing the trailing count per
 * file. Returns total matches and file count via out_* params. */
static void fops_parse_rg_count(const char *out, int *out_total, int *out_files) {
    *out_total = 0;
    *out_files = 0;
    char *lines = strdup(out ? out : "");
    char *save = NULL;
    char *line = strtok_r(lines, "\n", &save);
    while (line) {
        line = fops_strip(line);
        if (line[0]) {
            char *colon = strrchr(line, ':');
            if (colon && colon[1]) {
                char *end = NULL;
                long n = strtol(colon + 1, &end, 10);
                if (end && *end == '\0' && n > 0) {
                    *out_total += (int)n;
                    (*out_files)++;
                }
            }
        }
        line = strtok_r(NULL, "\n", &save);
    }
    free(lines);
}

/* PoP: file_ops_zero_match_probe @ tools/file_operations.py:_zero_match_probe */
char *file_ops_zero_match_probe(const char *pattern, const char *path,
                                  const char *file_glob) {
    /* Python: run ONE cheap case-insensitive count probe; if it hits, say so.
     * If the pattern contains regex metacharacters, also probe as fixed string.
     * Bounded: two rg invocations max, count-only output. */
    if (!pattern || !path) return NULL;
    if (!file_ops_has_command("rg")) return NULL;

    char *escaped_pat = file_ops_escape_shell_arg(pattern);
    char *escaped_path = file_ops_escape_shell_arg(path);

    /* Build glob expression and probe command. */
    char glob_expr[512];
    glob_expr[0] = '\0';
    if (file_glob && file_glob[0]) {
        char *esc_glob = file_ops_escape_shell_arg(file_glob);
        snprintf(glob_expr, sizeof(glob_expr), " --glob %s", esc_glob);
        free(esc_glob);
    }

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "rg -i --count-matches%s %s %s 2>/dev/null | head -50",
        glob_expr, escaped_pat, escaped_path);

    char *probe = file_ops_exec(cmd, NULL);
    int ci_total = 0, ci_files = 0;
    fops_parse_rg_count(probe, &ci_total, &ci_files);
    free(probe);
    free(escaped_pat);
    free(escaped_path);

    if (ci_total > 0) {
        char *hint = NULL;
        asprintf(&hint,
            "0 exact matches, but %d case-insensitive match(es) "
            "in %d file(s) — the pattern's casing may be wrong.",
            ci_total, ci_files);
        return hint;
    }

    /* Hidden/ignored probe: rg skips dotdirs and .gitignore'd files by default. */
    escaped_pat = file_ops_escape_shell_arg(pattern);
    escaped_path = file_ops_escape_shell_arg(path);
    snprintf(cmd, sizeof(cmd),
        "rg --hidden --no-ignore --count-matches%s %s %s 2>/dev/null | head -50",
        glob_expr, escaped_pat, escaped_path);
    probe = file_ops_exec(cmd, NULL);
    int h_total = 0, h_files = 0;
    fops_parse_rg_count(probe, &h_total, &h_files);
    free(probe);

    if (h_total > 0) {
        char *hint = NULL;
        asprintf(&hint,
            "0 matches in visible files, but %d match(es) in "
            "%d hidden or gitignored file(s) — these are excluded "
            "by default. Search the hidden path explicitly to include them.",
            h_total, h_files);
        free(escaped_pat); free(escaped_path);
        return hint;
    }

    /* If pattern has metachars, probe as fixed string. */
    if (fops_pattern_has_metachars(pattern)) {
        free(escaped_pat); free(escaped_path);
        escaped_pat = file_ops_escape_shell_arg(pattern);
        escaped_path = file_ops_escape_shell_arg(path);
        snprintf(cmd, sizeof(cmd),
            "rg -F --count-matches%s %s %s 2>/dev/null | head -50",
            glob_expr, escaped_pat, escaped_path);
        probe = file_ops_exec(cmd, NULL);
        int f_total = 0, f_files = 0;
        fops_parse_rg_count(probe, &f_total, &f_files);
        free(probe);
        free(escaped_pat); free(escaped_path);

        if (f_total > 0) {
            char *hint = NULL;
            asprintf(&hint,
                "0 regex matches, but %d literal match(es) — the "
                "pattern contains regex metacharacters that likely need "
                "escaping (or pass a simpler substring).", f_total);
            return hint;
        }
    }

    free(escaped_pat); free(escaped_path);
    return NULL;  /* Python: return None */
}

/* PoP: list @ tools/file_operations.py:_try_multi_path_search */

/* Port of Python _search_files_rg: find files by name pattern via rg.
 * Returns space-newline-separated list (caller parses), or empty string. */
static char *fops_rg_search_files(const char *pattern, const char *path,
                                      int limit, int offset) {
    char *escaped_pat = file_ops_escape_shell_arg(pattern);
    char *escaped_path = file_ops_escape_shell_arg(path);
    char cmd[2048];
    /* rg -l gives filenames; paginate with tail/head */
    snprintf(cmd, sizeof(cmd),
        "rg -l --null %s %s 2>/dev/null | "
        "tail -n +%d | head -n %d",
        escaped_pat, escaped_path, offset + 1, limit);
    char *out = file_ops_exec(cmd, NULL);
    free(escaped_pat); free(escaped_path);
    return out;
}

/* Port of Python _search_files: find files by name pattern.
 * Tries rg first, falls back to find. Returns json array of paths. */
json_t *file_ops_try_multi_search_files(const char *pattern, const char *path,
                                          int limit, int offset) {
    if (!pattern || !path) return json_array();
    json_t *files = json_array();
    if (!files) return NULL;

    if (file_ops_has_command("rg")) {
        char *out = fops_rg_search_files(pattern, path, limit, offset);
        if (out) {
            char *save = NULL;
            char *tok = strtok_r(out, "\n", &save);
            while (tok && json_len(files) < (size_t)limit) {
                char *clean = fops_strip(tok);
                if (clean[0]) {
                    json_append(files, json_string(clean));
                }
                tok = strtok_r(NULL, "\n", &save);
            }
            free(out);
        }
        return files;
    }

    /* Fallback: find */
    if (!file_ops_has_command("find")) {
        json_free(files);
        return json_array();
    }
    char *escaped_path = file_ops_escape_shell_arg(path);
    char *escaped_pat = file_ops_escape_shell_arg(pattern);
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "find %s -not -path '*/.*' -type f -name %s 2>/dev/null | "
        "sort -rn | tail -n +%d | head -n %d",
        escaped_path, escaped_pat, offset + 1, limit);
    char *out = file_ops_exec(cmd, NULL);
    if (out) {
        char *save = NULL;
        char *tok = strtok_r(out, "\n", &save);
        while (tok && json_len(files) < (size_t)limit) {
            char *clean = fops_strip(tok);
            if (clean[0]) json_append(files, json_string(clean));
            tok = strtok_r(NULL, "\n", &save);
        }
        free(out);
    }
    free(escaped_path); free(escaped_pat);
    return files;
}

/* Port of Python _try_multi_path_search.
 * Recover a not-found path that is really several paths in one string.
 * Splits on whitespace/commas; searches existing paths, merges results,
 * notes skipped missing parts. Returns a JSON result object or NULL. */
json_t *file_ops_try_multi_path_search(const char *pattern, const char *path,
                                         const char *target, const char *file_glob,
                                         int limit, int offset,
                                         const char *output_mode, int context) {
    (void)file_glob; (void)output_mode; (void)context;
    if (!pattern || !path) return NULL;

    /* Split on commas, then whitespace (Python: [p for chunk in path.split(",") for p in chunk.split() if p.strip()]) */
    char *path_copy = strdup(path);
    if (!path_copy) return NULL;

    char *existing[64];
    char *missing[64];
    int n_existing = 0, n_missing = 0;
    int n_parts = 0;

    char *semicolon = strchr(path_copy, ',');
    /* First split on commas */
    char *comma_save = NULL;
    char *chunk = strtok_r(path_copy, ",", &comma_save);
    while (chunk) {
        /* Then split on whitespace */
        char *ws_save = NULL;
        char *part = strtok_r(chunk, " \t", &ws_save);
        while (part) {
            n_parts++;
            char *expanded = file_ops_expand_path(part);
            struct stat st;
            if (expanded && stat(expanded, &st) == 0) {
                if (n_existing < 64) existing[n_existing++] = strdup(expanded);
            } else {
                if (n_missing < 64) missing[n_missing++] = strdup(expanded ? expanded : part);
            }
            if (expanded) free(expanded);
            part = strtok_r(NULL, " \t", &ws_save);
        }
        chunk = strtok_r(NULL, ",", &comma_save);
    }
    free(path_copy);

    /* Python: if len(parts) < 2: return None */
    if (n_parts < 2 || n_existing == 0) {
        for (int i = 0; i < n_existing; i++) free(existing[i]);
        for (int i = 0; i < n_missing; i++) free(missing[i]);
        return NULL;
    }

    /* Search each existing path and merge. */
    json_t *merged = json_object();
    json_t *matches_arr = json_array();   /* only relevant for content search */
    json_t *files_arr = json_array();
    int total_count = 0;

    for (int i = 0; i < n_existing; i++) {
        const char *p = existing[i];
        if (target && strcmp(target, "files") == 0) {
            json_t *sub = file_ops_try_multi_search_files(pattern, p, limit, offset);
            if (sub) {
                /* append file paths to files_arr */
                size_t n = json_len(sub);
                for (size_t j = 0; j < n; j++) {
                    json_t *item = json_get(sub, j);
                    if (item) json_append(files_arr, json_copy(item));
                }
                total_count += (int)n;
                json_free(sub);
            }
        } else {
            /* content search via rg -i */
            char *escaped_pat = file_ops_escape_shell_arg(pattern);
            char *escaped_path = file_ops_escape_shell_arg(p);
            char glob_expr[512];
            glob_expr[0] = '\0';
            if (file_glob && file_glob[0]) {
                char *esc_g = file_ops_escape_shell_arg(file_glob);
                snprintf(glob_expr, sizeof(glob_expr), " --glob %s", esc_g);
                free(esc_g);
            }
            char cmd[2048];
            snprintf(cmd, sizeof(cmd),
                "rg -i%s %s %s 2>/dev/null | head -n %d",
                glob_expr, escaped_pat, escaped_path, limit);
            char *out = file_ops_exec(cmd, NULL);
            if (out) {
                /* each line is a match */
                char *save = NULL;
                char *line = strtok_r(out, "\n", &save);
                while (line && json_len(files_arr) < (size_t)limit) {
                    char *clean = fops_strip(line);
                    if (clean[0]) {
                        json_append(files_arr, json_string(clean));
                        total_count++;
                    }
                    line = strtok_r(NULL, "\n", &save);
                }
                free(out);
            }
            free(escaped_pat); free(escaped_path);
        }
    }

    json_set(merged, "files", files_arr);
    json_set(merged, "total_count", json_number((double)total_count));
    json_set(merged, "files_count", json_number((double)json_len(files_arr)));

    /* Build warning note. */
    char note[1024];
    char *missing_str = NULL;
    if (n_missing > 0) {
        missing_str = malloc(512); missing_str[0] = '\0';
        for (int i = 0; i < n_missing && i < 3; i++) {
            if (i > 0) strcat(missing_str, ", ");
            strcat(missing_str, missing[i]);
        }
    }
    snprintf(note, sizeof(note),
        "path contained %d entries; searched %d that exist%s%s",
        n_parts, n_existing,
        missing_str ? "; skipped missing: " : "",
        missing_str ? missing_str : "");
    if (missing_str && n_missing > 3) {
        char extra[64];
        snprintf(extra, sizeof(extra), " (+%d more)", n_missing - 3);
        strcat(note, extra);
    }
    json_set(merged, "warning", json_string(note));

    for (int i = 0; i < n_existing; i++) free(existing[i]);
    for (int i = 0; i < n_missing; i++) free(missing[i]);
    free(missing_str);
    return merged;
}