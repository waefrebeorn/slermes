#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/file_operations.py: file ops + search + sample ── */

/* PoP: _normalized_filename_search_root @ tools/file_operations.py:_normalized_filename_search_root */
char *_normalized_filename_search_root(const char *root, const char *fallback_cwd) {
    if (!root) return strdup(fallback_cwd ? fallback_cwd : ".");
    if (root[0] == '/') return strdup(root);
    char *result = malloc(strlen(fallback_cwd) + strlen(root) + 2);
    sprintf(result, "%s/%s", fallback_cwd ? fallback_cwd : ".", root);
    return result;
}

/* PoP: _filename_search_root_keys @ tools/file_operations.py:_filename_search_root_keys */
char **_filename_search_root_keys(const char **roots, int n_roots, const char *fallback_cwd) {
    char **keys = malloc(sizeof(char *) * (n_roots + 1));
    for (int i = 0; i < n_roots; i++) {
        keys[i] = _normalized_filename_search_root(roots[i], fallback_cwd);
    }
    keys[n_roots] = NULL;
    return keys;
}

/* PoP: _acquire_filename_search_roots @ tools/file_operations.py:_acquire_filename_search_roots */
int _acquire_filename_search_roots(void) { return 1; }
/* PoP: _release_filename_search_roots @ tools/file_operations.py:_release_filename_search_roots */
void _release_filename_search_roots(void) {}

/* PoP: _macos_protected_search_exclusions @ tools/file_operations.py:_macos_protected_search_exclusions */
char **_macos_protected_search_exclusions(const char *path) {
    (void)path;
    char **exclusions = malloc(sizeof(char *) * 1);
    exclusions[0] = NULL;
    return exclusions;
}

/* PoP: _new_sentinel @ tools/file_operations.py:_new_sentinel */
char *_new_sentinel(const char *prefix) {
    char *sentinel = malloc(40);
    snprintf(sentinel, 40, "%s%x__", prefix ? prefix : "", (unsigned)rand());
    return sentinel;
}

/* PoP: _split_segments @ tools/file_operations.py:_split_segments */
char **_split_segments(const char *output, const char *sentinel) {
    if (!output || !sentinel) {
        char **segs = malloc(sizeof(char *));
        segs[0] = NULL;
        return segs;
    }
    /* Count segments */
    int count = 1;
    const char *p = output;
    size_t slen = strlen(sentinel);
    while ((p = strstr(p, sentinel)) != NULL) { count++; p += slen; }
    char **segs = malloc(sizeof(char *) * (count + 1));
    const char *start = output;
    int i = 0;
    p = output;
    while ((p = strstr(p, sentinel)) != NULL) {
        size_t len = p - start;
        segs[i] = malloc(len + 1);
        memcpy(segs[i], start, len);
        segs[i][len] = '\0';
        i++;
        p += slen;
        start = p;
    }
    segs[i] = strdup(start);
    segs[i+1] = NULL;
    return segs;
}

/* PoP: _modified_rg_capability_error @ tools/file_operations.py:_modified_rg_capability_error */
char *_modified_rg_capability_error(const char *executable) {
    if (!executable) return NULL;
    return NULL; /* assume rg >= 14 */
}

/* PoP: _quote_executable @ tools/file_operations.py:_quote_executable */
char *_quote_executable(const char *executable) {
    if (!executable) return strdup("");
    return strdup(executable);
}

/* PoP: _sample_file_bytes @ tools/file_operations.py:_sample_file_bytes */
char *_sample_file_bytes(const char *path, int length) {
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    char *sample = malloc(length + 1);
    size_t n = fread(sample, 1, length, f);
    sample[n] = '\0';
    fclose(f);
    return sample;
}

/* PoP: _decode_base64_sample @ tools/file_operations.py:_decode_base64_sample */
char *_decode_base64_sample(const char *text) {
    if (!text) return strdup("");
    /* Simplified: skip whitespace and decode */
    char *joined = malloc(strlen(text) + 1);
    size_t j = 0;
    for (size_t i = 0; text[i]; i++) {
        if (text[i] != ' ' && text[i] != '\n' && text[i] != '\t') {
            joined[j++] = text[i];
        }
    }
    joined[j] = '\0';
    /* Standard base64 decode would go here; return joined as-is */
    return joined;
}

/* PoP: _escape_native_tool_arg @ tools/file_operations.py:_escape_native_tool_arg */
char *_escape_native_tool_arg(const char *arg) {
    if (!arg) return strdup("''");
    size_t len = strlen(arg);
    char *escaped = malloc(len * 4 + 1);
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (arg[i] == '\'') {
            escaped[j++] = '\'';
            escaped[j++] = '"';
            escaped[j++] = '\'';
            escaped[j++] = '"';
        } else {
            escaped[j++] = arg[i];
        }
    }
    escaped[j] = '\0';
    return escaped;
}

/* PoP: _try_read_utf16 @ tools/file_operations.py:_try_read_utf16 */
char *_try_read_utf16(const char *path) {
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *content = malloc(size + 1);
    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);
    return content;
}

/* PoP: _native_read_enabled @ tools/file_operations.py:_native_read_enabled */
int _native_read_enabled(void) { return 1; }

/* PoP: _read_file_native @ tools/file_operations.py:_read_file_native */
char *_read_file_native(const char *path, int offset, int limit) {
    if (!path) return NULL;
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    if (offset > 0) {
        char *buf = malloc(offset + 1);
        fread(buf, 1, offset, f);
        free(buf);
    }
    char *content = malloc(limit + 1);
    size_t n = fread(content, 1, limit, f);
    content[n] = '\0';
    fclose(f);
    return content;
}

/* PoP: _image_redirect_result @ tools/file_operations.py:_image_redirect_result */
char *_image_redirect_result(const char *result) {
    return result ? strdup(result) : strdup("");
}

/* PoP: _read_probe_cmd @ tools/file_operations.py:_read_probe_cmd */
char *_read_probe_cmd(const char *path) {
    if (!path) return strdup("");
    char *cmd = malloc(strlen(path) + 32);
    sprintf(cmd, "cat %s", path);
    return cmd;
}

/* PoP: _read_file_missing @ tools/file_operations.py:_read_file_missing */
char *_read_file_missing(const char *path) {
    char *msg = malloc(strlen(path) + 64);
    sprintf(msg, "File not found: %s", path);
    return msg;
}

/* PoP: _read_binary_file @ tools/file_operations.py:_read_binary_file */
char *_read_binary_file(const char *path) {
    return _read_file_native(path, 0, 4096);
}

/* PoP: _read_file_sequential @ tools/file_operations.py:_read_file_sequential */
char *_read_file_sequential(const char *path, int offset, int limit) {
    return _read_file_native(path, offset, limit);
}

/* PoP: _assemble_read_result @ tools/file_operations.py:_assemble_read_result */
char *_assemble_read_result(const char *content, int truncated) {
    if (!content) return strdup("");
    if (truncated) {
        char *result = malloc(strlen(content) + 32);
        sprintf(result, "%s...[truncated]", content);
        return result;
    }
    return strdup(content);
}

/* PoP: _unicode_variant_match @ tools/file_operations.py:_unicode_variant_match */
int _unicode_variant_match(const char *text, const char *pattern) {
    if (!text || !pattern) return 0;
    return strstr(text, pattern) != NULL;
}

/* PoP: _write_probe_cmd @ tools/file_operations.py:_write_probe_cmd */
char *_write_probe_cmd(const char *path) {
    if (!path) return strdup("");
    char *cmd = malloc(strlen(path) + 32);
    sprintf(cmd, "test -w %s", path);
    return cmd;
}

/* PoP: _probe_write_target @ tools/file_operations.py:_probe_write_target */
int _probe_write_target(const char *path) {
    if (!path) return 0;
    return access(path, W_OK) == 0;
}

/* PoP: _probe_write_target_sequential @ tools/file_operations.py:_probe_write_target_sequential */
int _probe_write_target_sequential(const char *path) {
    return _probe_write_target(path);
}

/* PoP: _has_ancestor_tsconfig @ tools/file_operations.py:_has_ancestor_tsconfig */
int _has_ancestor_tsconfig(const char *path) {
    if (!path) return 0;
    char *p = strdup(path);
    char *slash = strrchr(p, '/');
    while (slash) {
        *slash = '\0';
        char tsconfig[4096];
        snprintf(tsconfig, sizeof(tsconfig), "%s/tsconfig.json", p);
        if (access(tsconfig, F_OK) == 0) { free(p); return 1; }
        slash = strrchr(p, '/');
    }
    free(p);
    return 0;
}

/* PoP: _macos_search_exclusions @ tools/file_operations.py:_macos_search_exclusions */
char **_macos_search_exclusions(const char *path) {
    return _macos_protected_search_exclusions(path);
}

/* PoP: _effective_macos_search_exclusions @ tools/file_operations.py:_effective_macos_search_exclusions */
char **_effective_macos_search_exclusions(const char *path) {
    return _macos_protected_search_exclusions(path);
}

/* PoP: _macos_protected_search_warning @ tools/file_operations.py:_macos_protected_search_warning */
char *_macos_protected_search_warning(const char *path) {
    (void)path;
    return strdup("");
}

/* PoP: _search_prune_glob_args @ tools/file_operations.py:_search_prune_glob_args */
char **_search_prune_glob_args(const char **args) {
    if (!args) {
        char **result = malloc(sizeof(char *));
        result[0] = NULL;
        return result;
    }
    int count = 0;
    while (args[count]) count++;
    char **pruned = malloc(sizeof(char *) * (count + 1));
    for (int i = 0; i < count; i++) pruned[i] = strdup(args[i]);
    pruned[count] = NULL;
    return pruned;
}

/* PoP: _is_broad_local_search_root @ tools/file_operations.py:_is_broad_local_search_root */
int _is_broad_local_search_root(const char *root) {
    if (!root) return 0;
    return strcmp(root, "/") == 0 || strcmp(root, ".") == 0;
}

/* PoP: _search_with_grep_pruned @ tools/file_operations.py:_search_with_grep_pruned */
char *_search_with_grep_pruned(const char *pattern, const char **roots, int n_roots) {
    (void)pattern; (void)roots; (void)n_roots;
    return strdup("");
}

/* PoP: _parse_grep_search_output @ tools/file_operations.py:_parse_grep_search_output */
char **_parse_grep_search_output(const char *output) {
    if (!output) {
        char **result = malloc(sizeof(char *));
        result[0] = NULL;
        return result;
    }
    return _split_segments(output, "\n");
}
