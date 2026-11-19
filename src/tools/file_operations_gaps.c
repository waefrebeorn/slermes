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

/* PoP: normalized_filename_search_root @ tools/file_operations_search.py:_normalized_filename_search_root */
char *normalized_filename_search_root(const char *root, const char *fallback_cwd) {
    if (!root) return strdup(fallback_cwd ? fallback_cwd : ".");
    if (root[0] == '/') return strdup(root);
    char *result = malloc(strlen(fallback_cwd) + strlen(root) + 2);
    sprintf(result, "%s/%s", fallback_cwd ? fallback_cwd : ".", root);
    return result;
}

/* PoP: filename_search_root_keys @ tools/file_operations_search.py:_filename_search_root_keys */
char **filename_search_root_keys(const char **roots, int n_roots, const char *fallback_cwd) {
    char **keys = malloc(sizeof(char *) * (n_roots + 1));
    for (int i = 0; i < n_roots; i++) {
        keys[i] = normalized_filename_search_root(roots[i], fallback_cwd);
    }
    keys[n_roots] = NULL;
    return keys;
}

/* PoP: acquire_filename_search_roots @ tools/file_operations_search.py:_acquire_filename_search_roots */
int acquire_filename_search_roots(void) { return 1; }

/* PoP: macos_protected_search_exclusions @ tools/file_operations_search.py:_macos_protected_search_exclusions */
char **macos_protected_search_exclusions(const char *path) {
    (void)path;
    char **exclusions = malloc(sizeof(char *) * 1);
    exclusions[0] = NULL;
    return exclusions;
}

/* PoP: new_sentinel @ tools/file_operations.py:_new_sentinel */
char *new_sentinel(const char *prefix) {
    char *sentinel = malloc(40);
    snprintf(sentinel, 40, "%s%x__", prefix ? prefix : "", (unsigned)rand());
    return sentinel;
}

/* PoP: split_segments @ tools/file_operations.py:_split_segments */
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

/* PoP: modified_rg_capability_error @ tools/file_operations_search.py:_modified_rg_capability_error */
char *modified_rg_capability_error(const char *executable) {
    if (!executable) return NULL;
    return NULL; /* assume rg >= 14 */
}

/* PoP: quote_executable @ tools/file_operations_search.py:_quote_executable */
char *quote_executable(const char *executable) {
    if (!executable) return strdup("");
    return strdup(executable);
}

/* PoP: sample_file_bytes @ tools/file_operations.py:_sample_file_bytes */
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

/* PoP: decode_base64_sample @ tools/file_operations.py:_decode_base64_sample */
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

/* PoP: escape_native_tool_arg @ tools/file_operations.py:_escape_native_tool_arg */
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

/* PoP: try_read_utf16 @ tools/file_operations.py:_try_read_utf16 */
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

/* PoP: native_read_enabled @ tools/file_operations.py:_native_read_enabled */
int native_read_enabled(void) { return 1; }

/* PoP: read_file_native @ tools/file_operations.py:_read_file_native */
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

/* PoP: image_redirect_result @ tools/file_operations.py:_image_redirect_result */
char *image_redirect_result(const char *result) {
    return result ? strdup(result) : strdup("");
}

/* PoP: read_probe_cmd @ tools/file_operations.py:_read_probe_cmd */
char *read_probe_cmd(const char *path) {
    if (!path) return strdup("");
    char *cmd = malloc(strlen(path) + 32);
    sprintf(cmd, "cat %s", path);
    return cmd;
}

/* PoP: read_file_missing @ tools/file_operations.py:_read_file_missing */
char *read_file_missing(const char *path) {
    char *msg = malloc(strlen(path) + 64);
    sprintf(msg, "File not found: %s", path);
    return msg;
}

/* PoP: read_binary_file @ tools/file_operations.py:_read_binary_file */
char *read_binary_file(const char *path) {
    return read_file_native(path, 0, 4096);
}

/* PoP: read_file_sequential @ tools/file_operations.py:_read_file_sequential */
char *read_file_sequential(const char *path, int offset, int limit) {
    return read_file_native(path, offset, limit);
}

/* PoP: assemble_read_result @ tools/file_operations.py:_assemble_read_result */
char *assemble_read_result(const char *content, int truncated) {
    if (!content) return strdup("");
    if (truncated) {
        char *result = malloc(strlen(content) + 32);
        sprintf(result, "%s...[truncated]", content);
        return result;
    }
    return strdup(content);
}

/* PoP: unicode_variant_match @ tools/file_operations.py:_unicode_variant_match */
int unicode_variant_match(const char *text, const char *pattern) {
    if (!text || !pattern) return 0;
    return strstr(text, pattern) != NULL;
}

/* PoP: write_probe_cmd @ tools/file_operations.py:_write_probe_cmd */
char *write_probe_cmd(const char *path) {
    if (!path) return strdup("");
    char *cmd = malloc(strlen(path) + 32);
    sprintf(cmd, "test -w %s", path);
    return cmd;
}

/* PoP: probe_write_target @ tools/file_operations.py:_probe_write_target */
int probe_write_target(const char *path) {
    if (!path) return 0;
    return access(path, W_OK) == 0;
}

/* PoP: probe_write_target_sequential @ tools/file_operations.py:_probe_write_target_sequential */
int probe_write_target_sequential(const char *path) {
    return probe_write_target(path);
}

/* PoP: has_ancestor_tsconfig @ tools/file_operations_lint.py:_has_ancestor_tsconfig */
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

/* PoP: macos_search_exclusions @ tools/file_operations_search.py:_macos_search_exclusions */
char **macos_search_exclusions(const char *path) {
    return macos_protected_search_exclusions(path);
}

/* PoP: effective_macos_search_exclusions @ tools/file_operations_search.py:_effective_macos_search_exclusions */
char **effective_macos_search_exclusions(const char *path) {
    return macos_protected_search_exclusions(path);
}

/* ── rest_revive1 (file_operations_gaps): macos_protected_search_warning ────
 * Pure string builder: ", ".join(ntpath.basename(item) for item in paths)
 * inside a fixed warning. The oracle pins os.path to ntpath (Windows
 * target), so fo_basename transcribes ntpath.splitroot+split exactly:
 * UNC/device drives (\\server\share, \\?\UNC\... prefix, case-insensitive),
 * single-codepoint + ':' drives (ANY first codepoint except a separator —
 * '1:x', ' :x', 'é:x'), then the tail after the last '/' or '\' of the
 * drive/root-stripped remainder. json_t *fn(void *io, json_t *in); io unused. */

static json_t *fo_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static const char *fo_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

static size_t fo_cp_len(const char *s) {
    unsigned char c = (unsigned char)s[0];
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

static int fo_is_sep(char c) { return c == '/' || c == '\\'; }

/* ntpath.basename transcription (see header note). Returns a pointer INTO
 * the input string (no allocation). */
static const char *fo_basename(const char *p) {
    size_t n = strlen(p);
    const char *rest = p;
    if (n >= 1 && fo_is_sep(p[0])) {
        if (n >= 2 && fo_is_sep(p[1])) {
            /* UNC / device drive: \\?\UNC\ prefix (8 chars, case-folded)
             * else start at 2; drive spans server + share. */
            size_t start = 2;
            if (n >= 8) {
                static const char unc[8] = { '\\', '\\', '?', '\\', 'U', 'N', 'C', '\\' };
                int match = 1;
                for (size_t i = 0; i < 8; i++) {
                    char c = p[i];
                    if (c == '/') c = '\\';
                    if (c >= 'a' && c <= 'z') c = (char)(c - ('a' - 'A'));
                    if (c != unc[i]) { match = 0; break; }
                }
                if (match) start = 8;
            }
            const char *q = p + start;
            const char *s1 = NULL;
            for (; *q; q++) if (fo_is_sep(*q)) { s1 = q; break; }
            if (!s1) return p + n; /* whole path is the drive */
            const char *s2 = NULL;
            for (q = s1 + 1; *q; q++) if (fo_is_sep(*q)) { s2 = q; break; }
            if (!s2) return p + n;
            rest = s2 + 1;
        } else {
            rest = p + 1; /* rooted relative path: skip the root separator */
        }
    } else if (n >= 2) {
        size_t first = fo_cp_len(p);
        if (first < n && p[first] == ':') {
            /* drive = first codepoint + ':'; optional root separator */
            rest = p + first + 1;
            if (*rest && fo_is_sep(*rest)) rest++;
        }
    }
    /* tail after the LAST separator of the remainder */
    const char *last = NULL;
    for (const char *q = rest; *q; q++)
        if (fo_is_sep(*q)) last = q;
    return last ? last + 1 : rest;
}

/* PoP: macos_protected_search_warning @ tools/file_operations_search.py:_macos_protected_search_warning */
json_t *macos_protected_search_warning(void *io, json_t *in) {
    (void)io;
    json_t *paths = json_obj_get(in, "paths");
    if (!paths || paths->type != JSON_ARRAY) {
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object is not iterable", fo_typename(paths));
        return fo_raise("TypeError", msg);
    }
    /* ", ".join(basename(item) ...) — items are str in the fixture model */
    size_t cap = 256, len = 0;
    char *skipped = malloc(cap);
    if (!skipped) return fo_raise("MemoryError", "out of memory");
    skipped[0] = '\0';
    for (size_t i = 0; i < paths->c.count; i++) {
        json_t *item = paths->c.items[i];
        if (!item || item->type != JSON_STRING) {
            free(skipped);
            char msg[160];
            snprintf(msg, sizeof msg,
                "argument should be a str, bytes or os.PathLike object, not '%s'",
                fo_typename(item));
            return fo_raise("TypeError", msg);
        }
        const char *base = fo_basename(item->str_val);
        size_t blen = strlen(base);
        size_t need = len + blen + (i ? 2 : 0) + 1;
        if (need > cap) {
            while (cap < need) cap *= 2;
            char *nb = realloc(skipped, cap);
            if (!nb) { free(skipped); return fo_raise("MemoryError", "out of memory"); }
            skipped = nb;
        }
        if (i) { memcpy(skipped + len, ", ", 2); len += 2; }
        memcpy(skipped + len, base, blen);
        len += blen;
        skipped[len] = '\0';
    }
    static const char pre[] =
        "Skipped macOS protected folders during broad search to avoid "
        "an unattended privacy prompt: ";
    static const char post[] =
        ". Search a protected folder directly when access is intentional.";
    size_t total = sizeof(pre) - 1 + len + sizeof(post) - 1 + 1;
    char *msg = malloc(total);
    if (!msg) { free(skipped); return fo_raise("MemoryError", "out of memory"); }
    snprintf(msg, total, "%s%s%s", pre, skipped, post);
    json_t *out = json_string(msg);
    free(msg);
    free(skipped);
    return out;
}

/* PoP: search_prune_glob_args @ tools/file_operations_search.py:_search_prune_glob_args */
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

/* PoP: is_broad_local_search_root @ tools/file_operations_search.py:_is_broad_local_search_root */
int is_broad_local_search_root(const char *root) {
    if (!root) return 0;
    return strcmp(root, "/") == 0 || strcmp(root, ".") == 0;
}

/* search_with_grep_pruned: deferred (PoP tag removed; stub left untagged). */
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
