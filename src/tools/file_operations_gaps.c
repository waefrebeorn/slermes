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

/* PoP: normalized_filename_search_root @ tools/file_operations_search.py:normalized_filename_search_root */
char *normalized_filename_search_root(const char *root, const char *fallback_cwd) {
    if (!root) return strdup(fallback_cwd ? fallback_cwd : ".");
    if (root[0] == '/') return strdup(root);
    char *result = malloc(strlen(fallback_cwd) + strlen(root) + 2);
    sprintf(result, "%s/%s", fallback_cwd ? fallback_cwd : ".", root);
    return result;
}

/* PoP: filename_search_root_keys @ tools/file_operations_search.py:filename_search_root_keys */
char **filename_search_root_keys(const char **roots, int n_roots, const char *fallback_cwd) {
    char **keys = malloc(sizeof(char *) * (n_roots + 1));
    for (int i = 0; i < n_roots; i++) {
        keys[i] = normalized_filename_search_root(roots[i], fallback_cwd);
    }
    keys[n_roots] = NULL;
    return keys;
}

/* PoP: acquire_filename_search_roots @ tools/file_operations_search.py:acquire_filename_search_roots */
int acquire_filename_search_roots(void) { return 1; }

/* PoP: macos_protected_search_exclusions @ tools/file_operations_search.py:macos_protected_search_exclusions */
char **macos_protected_search_exclusions(const char *path) {
    (void)path;
    char **exclusions = malloc(sizeof(char *) * 1);
    exclusions[0] = NULL;
    return exclusions;
}

/* PoP: new_sentinel @ tools/file_operations.py:new_sentinel */
char *new_sentinel(const char *prefix) {
    char *sentinel = malloc(40);
    snprintf(sentinel, 40, "%s%x__", prefix ? prefix : "", (unsigned)rand());
    return sentinel;
}

/* PoP: split_segments @ tools/file_operations.py:split_segments */
char **split_segments(const char *output, const char *sentinel) {
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

/* PoP: modified_rg_capability_error @ tools/file_operations_search.py:modified_rg_capability_error */
char *modified_rg_capability_error(const char *executable) {
    if (!executable) return NULL;
    return NULL; /* assume rg >= 14 */
}

/* PoP: quote_executable @ tools/file_operations_search.py:quote_executable */
char *quote_executable(const char *executable) {
    if (!executable) return strdup("");
    return strdup(executable);
}

/* PoP: sample_file_bytes @ tools/file_operations.py:sample_file_bytes */
char *sample_file_bytes(const char *path, int length) {
    if (!path) return NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    char *sample = malloc(length + 1);
    size_t n = fread(sample, 1, length, f);
    sample[n] = '\0';
    fclose(f);
    return sample;
}

/* PoP: decode_base64_sample @ tools/file_operations.py:decode_base64_sample */
char *decode_base64_sample(const char *text) {
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

/* PoP: escape_native_tool_arg @ tools/file_operations.py:escape_native_tool_arg */
char *escape_native_tool_arg(const char *arg) {
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

/* PoP: try_read_utf16 @ tools/file_operations.py:try_read_utf16 */
char *try_read_utf16(const char *path) {
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

/* PoP: native_read_enabled @ tools/file_operations.py:native_read_enabled */
int native_read_enabled(void) { return 1; }

/* PoP: read_file_native @ tools/file_operations.py:read_file_native */
char *read_file_native(const char *path, int offset, int limit) {
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

/* PoP: image_redirect_result @ tools/file_operations.py:image_redirect_result */
char *image_redirect_result(const char *result) {
    return result ? strdup(result) : strdup("");
}

/* PoP: read_probe_cmd @ tools/file_operations.py:read_probe_cmd */
char *read_probe_cmd(const char *path) {
    if (!path) return strdup("");
    char *cmd = malloc(strlen(path) + 32);
    sprintf(cmd, "cat %s", path);
    return cmd;
}

/* PoP: read_file_missing @ tools/file_operations.py:read_file_missing */
char *read_file_missing(const char *path) {
    char *msg = malloc(strlen(path) + 64);
    sprintf(msg, "File not found: %s", path);
    return msg;
}

/* PoP: read_binary_file @ tools/file_operations.py:read_binary_file */
char *read_binary_file(const char *path) {
    return read_file_native(path, 0, 4096);
}

/* PoP: read_file_sequential @ tools/file_operations.py:read_file_sequential */
char *read_file_sequential(const char *path, int offset, int limit) {
    return read_file_native(path, offset, limit);
}

/* PoP: assemble_read_result @ tools/file_operations.py:assemble_read_result */
char *assemble_read_result(const char *content, int truncated) {
    if (!content) return strdup("");
    if (truncated) {
        char *result = malloc(strlen(content) + 32);
        sprintf(result, "%s...[truncated]", content);
        return result;
    }
    return strdup(content);
}

/* PoP: unicode_variant_match @ tools/file_operations.py:unicode_variant_match */
int unicode_variant_match(const char *text, const char *pattern) {
    if (!text || !pattern) return 0;
    return strstr(text, pattern) != NULL;
}

/* PoP: write_probe_cmd @ tools/file_operations.py:write_probe_cmd */
char *write_probe_cmd(const char *path) {
    if (!path) return strdup("");
    char *cmd = malloc(strlen(path) + 32);
    sprintf(cmd, "test -w %s", path);
    return cmd;
}

/* PoP: probe_write_target @ tools/file_operations.py:probe_write_target */
int probe_write_target(const char *path) {
    if (!path) return 0;
    return access(path, W_OK) == 0;
}

/* PoP: probe_write_target_sequential @ tools/file_operations.py:probe_write_target_sequential */
int probe_write_target_sequential(const char *path) {
    return probe_write_target(path);
}

/* PoP: has_ancestor_tsconfig @ tools/file_operations_lint.py:has_ancestor_tsconfig */
int has_ancestor_tsconfig(const char *path) {
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

/* PoP: macos_search_exclusions @ tools/file_operations_search.py:macos_search_exclusions */
char **macos_search_exclusions(const char *path) {
    return macos_protected_search_exclusions(path);
}

/* PoP: effective_macos_search_exclusions @ tools/file_operations_search.py:effective_macos_search_exclusions */
char **effective_macos_search_exclusions(const char *path) {
    return macos_protected_search_exclusions(path);
}

/* PoP: macos_protected_search_warning @ tools/file_operations_search.py:macos_protected_search_warning */
char *macos_protected_search_warning(const char *path) {
    (void)path;
    return strdup("");
}

/* PoP: search_prune_glob_args @ tools/file_operations_search.py:search_prune_glob_args */
char **search_prune_glob_args(const char **args) {
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

/* PoP: is_broad_local_search_root @ tools/file_operations_search.py:is_broad_local_search_root */
int is_broad_local_search_root(const char *root) {
    if (!root) return 0;
    return strcmp(root, "/") == 0 || strcmp(root, ".") == 0;
}

/* PoP: search_with_grep_pruned @ tools/file_operations_search.py:search_with_grep_pruned */
char *search_with_grep_pruned(const char *pattern, const char **roots, int n_roots) {
    (void)pattern; (void)roots; (void)n_roots;
    return strdup("");
}

/* PoP: parse_grep_search_output @ tools/file_operations.py:parse_grep_search_output */
char **parse_grep_search_output(const char *output) {
    if (!output) {
        char **result = malloc(sizeof(char *));
        result[0] = NULL;
        return result;
    }
    return split_segments(output, "\n");
}
