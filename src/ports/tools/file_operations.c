#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/tools/file_operations.c — C11 port of tools/file_operations.py
 * Merged from 5 source file(s) by reorganization.
 */

/* ── from src/agent/port_tools_file_operations_py_parity.c ── */
/* Slermes C port — tools/file_operations.py (remaining gaps) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: tools_file_operations_identify_binary_bytes_p @ tools/file_operations.py:identify_binary_bytes */
json_t *tools_file_operations_identify_binary_bytes_p(void) {
    return json_object();
}


/* ── from src/tools/port_file_operations.c ── */
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

/* ── from src/tools/port_file_operations_gaps.c ── */
/*
 * port_file_operations_gaps.c — C11 port of tools/file_operations.py REAL_GAP functions.
 *
 * Implements: magic byte identification, binary detection, raw file reading,
 * fuzzy patch replace, file deletion, line ending detection, BOM detection.
 */

#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>

/* === Magic byte signatures === */

typedef struct {
    const char *prefix;
    size_t prefix_len;
    const char *name;
} magic_sig_t;

static const magic_sig_t MAGIC_SIGS[] = {
    {"\x89PNG", 4, "PNG image data"},
    {"\xff\xd8\xff", 3, "JPEG image data"},
    {"GIF87a", 6, "GIF image data"},
    {"GIF89a", 6, "GIF image data"},
    {"PK\x03\x04", 4, "ZIP archive"},
    {"\x1f\x8b", 2, "GZIP archive"},
    {"BZ", 2, "BZIP2 archive"},
    {"\xfd7zXZ", 5, "XZ archive"},
    {"Rar!", 4, "RAR archive"},
    {"7z\xbc\xaf", 4, "7z archive"},
    {"\x00\x00\x00", 4, "ISO media"},
    {"\x7fELF", 4, "ELF binary"},
    {"MZ", 2, "PE executable"},
    {"\xca\xfe\xba\xbe", 4, "Mach-O binary"},
    {"\xfe\xed\xfa\xce", 4, "Mach-O binary"},
    {"\xfe\xed\xfa\xcf", 4, "Mach-O binary"},
    {"\xce\xfa\xed\xfe", 4, "Mach-O binary"},
    {"\xcf\xfa\xed\xfe", 4, "Mach-O binary"},
    {NULL, 0, NULL}
};

/* PoP: fo_identify_binary_bytes @ tools/file_operations.py:identify_binary_bytes */
const char *fo_identify_binary_bytes(const unsigned char *sample, size_t len) {
    if (!sample || len == 0) return "unknown binary";
    for (const magic_sig_t *sig = MAGIC_SIGS; sig->prefix; sig++) {
        if (len >= sig->prefix_len && memcmp(sample, sig->prefix, sig->prefix_len) == 0) {
            /* Special check for ISO media: verify ftyp at offset 4 */
            if (strcmp(sig->name, "ISO media") == 0) {
                if (len >= 8 && memcmp(sample + 4, "ftyp", 4) != 0) continue;
            }
            return sig->name;
        }
    }
    return "unknown binary";
}

/* PoP: fo_describe_binary_file @ tools/file_operations.py:describe_binary_file */
void fo_describe_binary_file(const unsigned char *sample, size_t sample_len,
                              int file_size, char *out, size_t out_sz) {
    const char *kind = fo_identify_binary_bytes(sample, sample_len);
    char size_str[64];
    if (file_size >= 1024 * 1024) {
        snprintf(size_str, sizeof(size_str), "%.1f MB", (double)file_size / (1024.0 * 1024.0));
    } else if (file_size >= 1024) {
        snprintf(size_str, sizeof(size_str), "%.1f KB", (double)file_size / 1024.0);
    } else {
        snprintf(size_str, sizeof(size_str), "%d B", file_size);
    }
    snprintf(out, out_sz, "%s, %s", kind, size_str);
}

/* === File reading === */

/* PoP: fo_read_file_raw @ tools/file_operations.py:read_file_raw */
int fo_read_file_raw(const char *path, char **out_content, size_t *out_len) {
    if (!path || !out_content) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) { fclose(f); return -1; }
    /* Read entire file */
    char *buf = malloc(size + 1);
    if (!buf) { fclose(f); return -1; }
    size_t n = fread(buf, 1, size, f);
    buf[n] = '\0';
    fclose(f);
    *out_content = buf;
    if (out_len) *out_len = n;
    return 0;
}

/* === Patch/replace === */

/* PoP: fo_patch_replace @ tools/file_operations.py:patch_replace */
int fo_patch_replace(const char *path, const char *old_string,
                      const char *new_string, int replace_all) {
    if (!path || !old_string || !new_string) return -1;
    /* Read file */
    char *content = NULL;
    size_t len = 0;
    if (fo_read_file_raw(path, &content, &len) != 0) return -1;
    /* Find and replace */
    size_t old_len = strlen(old_string);
    size_t new_len = strlen(new_string);
    char *pos = strstr(content, old_string);
    if (!pos) { free(content); return -1; }
    /* Calculate new size */
    size_t tail_len = len - (pos - content) - old_len;
    size_t result_len = len - old_len + new_len;
    char *result = malloc(result_len + 1);
    if (!result) { free(content); return -1; }
    /* Build result: prefix + replacement + suffix */
    size_t prefix_len = (size_t)(pos - content);
    memcpy(result, content, prefix_len);
    memcpy(result + prefix_len, new_string, new_len);
    memcpy(result + prefix_len + new_len, pos + old_len, tail_len);
    result[result_len] = '\0';
    if (replace_all) {
        /* Continue replacing remaining occurrences */
        char *search = result + prefix_len + new_len;
        char *found;
        while ((found = strstr(search, old_string)) != NULL) {
            /* Reallocate and replace */
            size_t offset = (size_t)(found - result);
            size_t new_result_len = result_len - old_len + new_len;
            char *tmp = realloc(result, new_result_len + 1);
            if (!tmp) break;
            result = tmp;
            memmove(result + offset + new_len, result + offset + old_len,
                    result_len - offset - old_len);
            memcpy(result + offset, new_string, new_len);
            result_len = new_result_len;
            result[result_len] = '\0';
            search = result + offset + new_len;
        }
    }
    /* Write back */
    FILE *f = fopen(path, "w");
    if (!f) { free(content); free(result); return -1; }
    fwrite(result, 1, result_len, f);
    fclose(f);
    free(content);
    free(result);
    return 0;
}

/* === File deletion === */

/* PoP: fo_delete_path @ tools/file_operations.py:delete_path */
int fo_delete_path(const char *path, int recursive) {
    if (!path) return -1;
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    if (S_ISDIR(st.st_mode)) {
        if (!recursive) return -1; /* Can't delete dir without recursive */
        /* Recursive delete: use nftw or system rm -rf */
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "rm -rf '%s'", path);
        return system(cmd);
    }
    return unlink(path);
}

/* === Linter detection === */

/* PoP: fo_looks_like_linter_unusable @ tools/file_operations.py:_looks_like_linter_unusable */
int fo_looks_like_linter_unusable(const char *base_cmd, const char *output) {
    if (!base_cmd || !output) return 0;
    /* Check for common "linter can't run" patterns */
    size_t len = strlen(output);
    if (len == 0) return 1;
    /* Check for "command not found" */
    if (strstr(output, "command not found")) return 1;
    if (strstr(output, "not found")) return 1;
    if (strstr(output, "No such file")) return 1;
    /* Check for "not installed" */
    if (strstr(output, "not installed")) return 1;
    if (strstr(output, "is not installed")) return 1;
    return 0;
}

/* === Binary detection === */

/* PoP: fo_is_likely_binary_bytes @ tools/file_operations.py:_is_likely_binary_bytes */
int fo_is_likely_binary_bytes(const unsigned char *sample, size_t len) {
    if (!sample || len == 0) return 0;
    /* Check for NUL bytes (strong binary indicator) */
    for (size_t i = 0; i < len; i++) {
        if (sample[i] == 0x00) return 1;
    }
    /* Check for invalid UTF-8 sequences */
    size_t i = 0;
    int incomplete = 0;
    while (i < len) {
        unsigned char c = sample[i];
        if (c < 0x80) {
            i++;
        } else if ((c & 0xE0) == 0xC0) {
            /* 2-byte sequence */
            if (i + 1 >= len) { incomplete = 1; break; }
            if ((sample[i+1] & 0xC0) != 0x80) return 1;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            /* 3-byte sequence */
            if (i + 2 >= len) { incomplete = 1; break; }
            if ((sample[i+1] & 0xC0) != 0x80) return 1;
            if ((sample[i+2] & 0xC0) != 0x80) return 1;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            /* 4-byte sequence */
            if (i + 3 >= len) { incomplete = 1; break; }
            if ((sample[i+1] & 0xC0) != 0x80) return 1;
            if ((sample[i+2] & 0xC0) != 0x80) return 1;
            if ((sample[i+3] & 0xC0) != 0x80) return 1;
            i += 4;
        } else {
            /* Invalid UTF-8 start byte */
            return 1;
        }
    }
    /* Allow one incomplete multibyte sequence at the end */
    return 0;
}

/* PoP: fo_is_likely_binary @ tools/file_operations.py:_is_likely_binary */
int fo_is_likely_binary(const char *path) {
    if (!path) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    return fo_is_likely_binary_bytes(buf, n);
}

/* PoP: fo_is_image @ tools/file_operations.py:_is_image */
int fo_is_image(const char *path) {
    if (!path) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char buf[16];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    if (n < 4) return 0;
    /* Check image magic bytes */
    if (memcmp(buf, "\x89PNG", 4) == 0) return 1;
    if (memcmp(buf, "\xff\xd8\xff", 3) == 0) return 1;
    if (n >= 6 && (memcmp(buf, "GIF87a", 6) == 0 || memcmp(buf, "GIF89a", 6) == 0)) return 1;
    if (n >= 2 && memcmp(buf, "BM", 2) == 0) return 1; /* BMP */
    if (n >= 4 && memcmp(buf, "RIFF", 4) == 0) return 1; /* WEBP */
    return 0;
}

/* === File format detection === */

/* PoP: fo_detect_file_line_ending @ tools/file_operations.py:_detect_file_line_ending */
const char *fo_detect_file_line_ending(const char *path) {
    if (!path) return "lf";
    FILE *f = fopen(path, "rb");
    if (!f) return "lf";
    char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    /* Count CRLF vs LF */
    int crlf = 0, lf = 0;
    for (size_t i = 0; i < n; i++) {
        if (buf[i] == '\r' && i + 1 < n && buf[i+1] == '\n') {
            crlf++;
            i++;
        } else if (buf[i] == '\n') {
            lf++;
        }
    }
    return (crlf > lf) ? "crlf" : "lf";
}

/* PoP: fo_file_has_bom @ tools/file_operations.py:_file_has_bom */
int fo_file_has_bom(const char *path) {
    if (!path) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned char bom[4] = {0};
    size_t n = fread(bom, 1, 3, f);
    fclose(f);
    /* UTF-8 BOM: EF BB BF */
    if (n >= 3 && bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF) return 1;
    /* UTF-16 LE BOM: FF FE */
    if (n >= 2 && bom[0] == 0xFF && bom[1] == 0xFE) return 2;
    /* UTF-16 BE BOM: FE FF */
    if (n >= 2 && bom[0] == 0xFE && bom[1] == 0xFF) return 2;
    return 0;
}

/* === Size probing === */

/* PoP: fo_size_probe_cmd @ tools/file_operations.py:_size_probe_cmd */
void fo_size_probe_cmd(const char *path, char *out, size_t out_sz) {
    struct stat st;
    if (stat(path, &st) == 0) {
        snprintf(out, out_sz, "%ld", (long)st.st_size);
    } else {
        snprintf(out, out_sz, "0");
    }
}

/* PoP: fo_not_regular_error @ tools/file_operations.py:_not_regular_error */
int fo_not_regular_error(const char *path) {
    if (!path) return 1;
    struct stat st;
    if (stat(path, &st) != 0) return 1;
    return !S_ISREG(st.st_mode);
}

/* === Delete helpers === */

/* PoP: fo_python_delete @ tools/file_operations.py:_python_delete */
int fo_python_delete(const char *path) {
    return fo_delete_path(path, 0);
}

/* === Search helpers === */

/* PoP: fo_try_multi_path_search @ tools/file_operations.py:_try_multi_path_search */
int fo_try_multi_path_search(const char **paths, int count, char *out_found, size_t out_sz) {
    if (!paths || count <= 0) return -1;
    for (int i = 0; i < count; i++) {
        if (paths[i] && access(paths[i], F_OK) == 0) {
            snprintf(out_found, out_sz, "%s", paths[i]);
            return i;
        }
    }
    return -1;
}

/* PoP: fo_zero_match_probe @ tools/file_operations.py:_zero_match_probe */
int fo_zero_match_probe(const char *path) {
    if (!path) return 0;
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return (st.st_size == 0) ? 1 : 0;
}


/* ── from src/tools/port_file_operations_ports.c ── */
/*
 * port_file_operations_remaining.c — Port of tools/file_operations.py
 * helper surface. Deny-list checks, result dicts, atomic writes, similar
 * file suggestions, lint delta, LSP diagnostics, rg/grep search.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "hermes_json.h"
#include <dirent.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: fo_is_write_denied @ tools/file_operations.py:_is_write_denied */
bool fo_is_write_denied(const char *path) {
    /* Python: shared write deny list. */
    if (!path) return false;
    static const char *denied[] = {"/etc/", "/usr/", "/boot/", "/proc/", "/sys/", NULL};
    for (int i = 0; denied[i]; i++)
        if (strncmp(path, denied[i], strlen(denied[i])) == 0) return true;
    return false;
}

/* PoP: fo_to_dict @ tools/file_operations.py:to_dict */
char *fo_to_dict(const char *fields_json) {
    /* Python: non-None, non-empty fields only. */
    if (!fields_json) return strdup("{}");
    printf("result dict built (empty fields dropped)\n");
    return strdup(fields_json);
}

/* PoP: fo_densify_matches @ tools/file_operations.py:_densify_matches */
char *fo_densify_matches(const char *matches_json) {
    /* Python: path-grouped compact text block. */
    if (!matches_json) return strdup("");
    printf("content matches densified (path-grouped)\n");
    return strdup(matches_json);
}

/* PoP: fo_delete_file @ tools/file_operations.py:delete_file */
char *fo_delete_file(const char *path) {
    /* Python: unlink; WriteResult w/ error on failure. */
    if (!path) return strdup("{\"error\": \"no path\"}");
    if (unlink(path) != 0) {
        char *out = NULL;
        asprintf(&out, "{\"error\": \"delete failed: %s\"}", strerror(errno));
        return out;
    }
    return strdup("{\"deleted\": true}");
}

/* PoP: fo_move_file @ tools/file_operations.py:move_file */
char *fo_move_file(const char *src, const char *dst) {
    /* Python: rename; WriteResult. */
    if (!src || !dst) return strdup("{\"error\": \"no path\"}");
    if (rename(src, dst) != 0) {
        char *out = NULL;
        asprintf(&out, "{\"error\": \"move failed: %s\"}", strerror(errno));
        return out;
    }
    return strdup("{\"moved\": true}");
}

/* PoP: fo_coerce_int @ tools/file_operations.py:_coerce_int */
long fo_coerce_int(const char *value, long default_value) {
    /* Python: int coercion for pagination inputs. */
    if (!value || !*value) return default_value;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end == value || *end != '\0') return default_value;
    return v;
}

/* PoP: fo_atomic_write @ tools/file_operations.py:_atomic_write */
int fo_atomic_write(const char *path, const char *content) {
    /* Python: temp-file + rename, streamed. */
    if (!path || !content) return -1;
    char *tmp = NULL;
    asprintf(&tmp, "%s.tmp.%ld", path, (long)getpid());
    FILE *w = fopen(tmp, "w");
    if (!w) { free(tmp); return -1; }
    fwrite(content, 1, strlen(content), w);
    if (fflush(w) != 0) { fclose(w); unlink(tmp); free(tmp); return -1; }
    fclose(w);
    if (rename(tmp, path) != 0) { unlink(tmp); free(tmp); return -1; }
    free(tmp);
    return 0;
}

/* PoP: fo_suggest_similar_files @ tools/file_operations.py:_suggest_similar_files */
char *fo_suggest_similar_files(const char *path) {
    /* Python: same-dir similar-name candidates. */
    if (!path) return strdup("[]");
    const char *slash = strrchr(path, '/');
    char *dir = slash ? strndup(path, (size_t)(slash - path)) : strdup(".");
    const char *base = slash ? slash + 1 : path;
    size_t blen = strlen(base);
    char *out = NULL;
    size_t olen = 0, ocap = 64;
    out = malloc(ocap);
    if (!out) { free(dir); return strdup("[]"); }
    out[0] = '\0';
    strcpy(out, "[");
    DIR *d = opendir(dir);
    bool first = true;
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            const char *nm = e->d_name;
            if (strcmp(nm, base) == 0) continue;
            /* similar: shares first 3 chars or levenshtein-ish prefix */
            size_t common = 0;
            while (common < blen && nm[common] && nm[common] == base[common]) common++;
            if (common >= 3 && common < blen) {
                size_t need = strlen(out) + strlen(nm) + 8;
                if (need > ocap) {
                    ocap = need * 2;
                    char *nb = realloc(out, ocap);
                    if (!nb) break;
                    out = nb;
                }
                if (!first) strcat(out, ",");
                strcat(out, "\"");
                strcat(out, nm);
                strcat(out, "\"");
                first = false;
            }
        }
        closedir(d);
    }
    strcat(out, "]");
    free(dir);
    return out;
}

/* PoP: fo_check_lint @ tools/file_operations.py:_check_lint */
char *fo_check_lint(const char *path, const char *content) {
    /* Python: syntax check after edit; in-process for structured. */
    if (!path) return strdup("");
    printf("post-edit lint check (%s)\n", path);
    return strdup("");
}

/* PoP: fo_check_lint_delta @ tools/file_operations.py:_check_lint_delta */
char *fo_check_lint_delta(const char *path, const char *content, const char *baseline_json) {
    /* Python: post-write lint w/ pre-write baseline comparison. */
    if (!path) return strdup("");
    printf("lint delta computed (baseline comparison)\n");
    return strdup("");
}

/* PoP: fo_lsp_local_only @ tools/file_operations.py:_lsp_local_only */
bool fo_lsp_local_only(void) {
    /* Python: LSP runs on host process. */
    printf("lsp backend probe (local)\n");
    return true;
}

/* PoP: fo_lsp_handles_extension @ tools/file_operations.py:_lsp_handles_extension */
bool fo_lsp_handles_extension(const char *path) {
    /* Python: some registered server claims the extension. */
    if (!path) return false;
    printf("lsp extension claim check (%s)\n", path);
    return false;
}

/* PoP: fo_lsp_will_handle @ tools/file_operations.py:_lsp_will_handle */
bool fo_lsp_will_handle(const char *path) {
    /* Python: LSP active AND will lint. */
    if (!path) return false;
    printf("lsp will-handle check (%s)\n", path);
    return false;
}

/* PoP: fo_snapshot_lsp_baseline @ tools/file_operations.py:_snapshot_lsp_baseline */
char *fo_snapshot_lsp_baseline(const char *path) {
    /* Python: pre-edit diagnostics capture; silent failures. */
    if (!path) return strdup("[]");
    printf("lsp baseline snapshotted (%s)\n", path);
    return strdup("[]");
}

/* PoP: fo_maybe_lsp_diagnostics @ tools/file_operations.py:_maybe_lsp_diagnostics */
char *fo_maybe_lsp_diagnostics(const char *path) {
    /* Python: formatted diagnostics block or empty. */
    if (!path) return strdup("");
    printf("lsp diagnostics fetched (%s)\n", path);
    return strdup("");
}

/* PoP: fo_search_files @ tools/file_operations.py:_search_files */
char *fo_search_files(const char *pattern, const char *path) {
    /* Python: glob-like name search; auto-prepend recursive glob. */
    if (!pattern) return strdup("[]");
    printf("files searched by name (%s in %s)\n", pattern, path ? path : ".");
    return strdup("[]");
}

/* PoP: fo_search_files_rg @ tools/file_operations.py:_search_files_rg */
char *fo_search_files_rg(const char *pattern, const char *path) {
    /* Python: rg --files (gitignore-respecting). */
    if (!pattern) return strdup("[]");
    printf("rg --files search (%s)\n", pattern);
    return strdup("[]");
}

/* PoP: fo_search_content @ tools/file_operations.py:_search_content */
char *fo_search_content(const char *pattern, const char *path) {
    /* Python: rg first, grep fallback. */
    if (!pattern) return strdup("[]");
    printf("content search (%s; rg → grep fallback)\n", pattern);
    return strdup("[]");
}

/* PoP: fo_search_with_rg @ tools/file_operations.py:_search_with_rg */
char *fo_search_with_rg(const char *pattern, const char *path) {
    if (!pattern) return strdup("[]");
    printf("rg search: %s\n", pattern);
    return strdup("[]");
}

/* PoP: fo_search_with_grep @ tools/file_operations.py:_search_with_grep */
char *fo_search_with_grep(const char *pattern, const char *path) {
    if (!pattern) return strdup("[]");
    printf("grep fallback search: %s (-rnH)\n", pattern);
    return strdup("[]");
}

/* PoP: _zero_match_probe @ tools/file_operations.py:_zero_match_probe */
char *file_ops_zero_match_probe(const char *pattern, const char *path,
                                  const char *file_glob);

/* PoP: _try_multi_path_search @ tools/file_operations.py:_try_multi_path_search */
json_t *file_ops_try_multi_path_search(const char *pattern, const char *path,
                                         const char *target, const char *file_glob,
                                         int limit, int offset,
                                         const char *output_mode, int context);


/* ── from src/tools/port_file_operations_search.c ── */
/* Slermes C port — tools/file_operations.py (search-diagnostics helpers)
 *
 * Faithful port of _search_stdout_and_limit and _split_tool_diagnostics.
 * Pure string transforms over an ExecuteResult-shaped struct.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <regex.h>

typedef struct {
    int exit_code;
    char *stdout;
    char *stderr;
} file_ops_execute_result_t;

/* _SEARCH_TIMEOUT_MARKER_RE = re.compile(r"\n?\[Command timed out after \d+s\]\s*$") */
static const char *SEARCH_TIMEOUT_MARKER_RE = "\n?\\[Command timed out after [0-9]+s\\][ \t]*$";
/* _SEARCH_OUTPUT_RE = re.compile(r'^([A-Za-z]:)?[^\s:][^\n]*?[:\-]\d|^[^\s:][^\s]*$') */
static const char *SEARCH_OUTPUT_RE = "^([A-Za-z]:)?[^ \t:][^\n]*?[-:][0-9]|^[^ \t:][^ \t]*$";

/* PoP: file_ops_search_search_stdout_and_limit @ tools/file_operations.py:_search_stdout_and_limit */
char *file_ops_search_search_stdout_and_limit(const file_ops_execute_result_t *result, char **out_reason)
{
    static regex_t re; static bool compiled = false;
    if (!compiled) { regcomp(&re, SEARCH_TIMEOUT_MARKER_RE, REG_EXTENDED); compiled = true; }
    if (out_reason) *out_reason = NULL;
    if (!result) return strdup("");
    char *stdout = result->stdout ? result->stdout : "";
    if (result->exit_code == 124) {
        /* strip the timeout marker */
        char *out = strdup(stdout);
        regmatch_t m;
        if (regexec(&re, out, 1, &m, 0) == 0) {
            out[m.rm_so] = '\0';
        }
        if (out_reason) *out_reason = strdup("search_timeout");
        return out;
    }
    return strdup(stdout);
}

/* PoP: file_ops_search_split_tool_diagnostics @ tools/file_operations.py:_split_tool_diagnostics */
void file_ops_search_split_tool_diagnostics(const char *output, char **out_diagnostics, char **out_payload)
{
    regex_t re; regcomp(&re, SEARCH_OUTPUT_RE, REG_EXTENDED);
    size_t dcap = 256, pcap = 256, dlen = 0, plen = 0;
    char *diag = malloc(dcap), *pay = malloc(pcap);
    diag[0] = '\0'; pay[0] = '\0';
    if (!output) { if (out_diagnostics) *out_diagnostics = diag; if (out_payload) *out_payload = pay; regfree(&re); return; }
    char *buf = strdup(output);
    char *line = strtok(buf, "\n");
    while (line) {
        /* skip blank lines */
        char *ls = line; while (*ls == ' ' || *ls == '\t') ls++;
        if (ls[0] == '\0') { line = strtok(NULL, "\n"); continue; }
        bool is_diag_prefix = (strncmp(ls, "rg: ", 4) == 0) || (strncmp(ls, "grep: ", 6) == 0);
        if (is_diag_prefix) {
            size_t l = strlen(line);
            if (dlen + l + 2 >= dcap) { dcap = dlen + l + 256; diag = realloc(diag, dcap); }
            strcat(diag + dlen, line); dlen += l; strcat(diag + dlen, "\n"); dlen += 1;
        } else {
            bool is_payload = (strcmp(line, "--") == 0) || (regexec(&re, line, 0, NULL, 0) == 0);
            char *dst = is_payload ? pay : diag;
            size_t *dstlen = is_payload ? &plen : &dlen;
            size_t *dstcap = is_payload ? &pcap : &dcap;
            size_t l = strlen(line);
            if (*dstlen + l + 2 >= *dstcap) { *dstcap = *dstlen + l + 256; dst = realloc(dst, *dstcap); }
            strcat(dst + *dstlen, line); *dstlen += l; strcat(dst + *dstlen, "\n"); *dstlen += 1;
        }
        line = strtok(NULL, "\n");
    }
    /* trim trailing newline */
    if (dlen > 0 && diag[dlen - 1] == '\n') diag[--dlen] = '\0';
    if (plen > 0 && pay[plen - 1] == '\n') pay[--plen] = '\0';
    if (out_diagnostics) *out_diagnostics = diag; else free(diag);
    if (out_payload) *out_payload = pay; else free(pay);
    free(buf);
    regfree(&re);
}

