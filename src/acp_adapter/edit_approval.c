/*
 * edit_approval.c - faithful C11 port of selected pure helpers from
 * upstream Hermes acp_adapter/edit_approval.py (gate w11_editapprov_59a256).
 *
 * PoP: w11_editapprov_59a256_extract_v4a_patch_paths @ acp_adapter/edit_approval.py:_extract_v4a_patch_paths
 * PoP: w11_editapprov_59a256_path_is_sensitive @ acp_adapter/edit_approval.py:_path_is_sensitive
 * PoP: w11_editapprov_59a256_denied @ acp_adapter/edit_approval.py:_denied
 *
 * Filesystem/network/permission-bridge helpers are intentionally NOT ported:
 * they are stateful, process-bound or require ACP/asyncio runtime objects.
 */
#include "noc_w11_editapprov_59a256.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* SENSITIVE_AUTO_APPROVE_NAMES (lower-cased). */
static const char *const SENSITIVE_NAMES[] = {
    ".env", ".env.local", ".env.production", "id_rsa", "id_ed25519", NULL
};

static int py_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python str.strip() over ASCII whitespace (s is a NUL-terminated slice). */
static char *py_strip_dup(const char *s, size_t n) {
    size_t a = 0, b = n;
    while (a < b && py_isspace((unsigned char)s[a])) a++;
    while (b > a && py_isspace((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static char *lower_dup(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++)
        out[i] = (char)tolower((unsigned char)s[i]);
    out[n] = '\0';
    return out;
}

/* Word match at offset; returns word length or 0 when absent. */
static size_t match_word(const char *line, size_t len, size_t i, const char *word) {
    size_t wl = strlen(word);
    if (i + wl <= len && memcmp(line + i, word, wl) == 0) return wl;
    return 0;
}

/* ---- _extract_v4a_patch_paths ---------------------------------------- */
json_t *w11_editapprov_59a256_extract_v4a_patch_paths(const char *patch_body) {
    json_t *result = json_array();
    if (!result) return NULL;
    if (!patch_body) return result;

    size_t total = strlen(patch_body);

    /* Pass 1: ^\*\*\*\s*(?:Update|Add|Delete)\s+File:\s*(.+)$ (MULTILINE). */
    size_t start = 0;
    while (start <= total) {
        size_t e = start;
        while (e < total && patch_body[e] != '\n') e++;
        size_t len = e - start;
        const char *line = patch_body + start;

        if (len >= 3 && memcmp(line, "***", 3) == 0) {
            size_t i = 3;
            while (i < len && py_isspace((unsigned char)line[i])) i++;
            size_t wl = 0;
            if ((wl = match_word(line, len, i, "Update")) == 0 &&
                (wl = match_word(line, len, i, "Add")) == 0 &&
                (wl = match_word(line, len, i, "Delete")) == 0) wl = 0;
            if (wl) {
                i += wl;
                if (i < len && py_isspace((unsigned char)line[i])) {
                    while (i < len && py_isspace((unsigned char)line[i])) i++;
                    if (i + 5 <= len && memcmp(line + i, "File:", 5) == 0) {
                        i += 5;
                        while (i < len && py_isspace((unsigned char)line[i])) i++;
                        if (i < len) {
                            char *p = py_strip_dup(line + i, len - i);
                            if (p && *p) json_append(result, json_string(p));
                            free(p);
                        }
                    }
                }
            }
        }
        if (e >= total) break;
        start = e + 1;
    }

    /* Pass 2: ^\*\*\*\s*Move\s+File:\s*(.+?)\s*->\s*(.+)$ (MULTILINE). */
    start = 0;
    while (start <= total) {
        size_t e = start;
        while (e < total && patch_body[e] != '\n') e++;
        size_t len = e - start;
        const char *line = patch_body + start;

        if (len >= 3 && memcmp(line, "***", 3) == 0) {
            size_t i = 3;
            while (i < len && py_isspace((unsigned char)line[i])) i++;
            size_t wl = match_word(line, len, i, "Move");
            if (wl) {
                i += wl;
                if (i < len && py_isspace((unsigned char)line[i])) {
                    while (i < len && py_isspace((unsigned char)line[i])) i++;
                    if (i + 5 <= len && memcmp(line + i, "File:", 5) == 0) {
                        i += 5;
                        while (i < len && py_isspace((unsigned char)line[i])) i++;
                        const char *rest = line + i;
                        size_t rlen = len - i;
                        /* First "->" (non-greedy group 1). */
                        size_t k = 0;
                        int found = 0;
                        for (; k + 1 < rlen; k++) {
                            if (rest[k] == '-' && rest[k + 1] == '>') { found = 1; break; }
                        }
                        if (found) {
                            char *g1 = py_strip_dup(rest, k);
                            char *g2 = py_strip_dup(rest + k + 2, rlen - (k + 2));
                            if (g1 && *g1) json_append(result, json_string(g1));
                            if (g2 && *g2) json_append(result, json_string(g2));
                            free(g1);
                            free(g2);
                        }
                    }
                }
            }
        }
        if (e >= total) break;
        start = e + 1;
    }
    return result;
}

/* ---- _path_is_sensitive(Path(path)) ---------------------------------- */
bool w11_editapprov_59a256_path_is_sensitive(const char *path) {
    if (!path) return false;
    size_t n = strlen(path);
    /* pathlib.Path(...) drops empty segments and '.'; keeps '..'. */
    const char **parts = (const char **)malloc((n + 2) * sizeof(*parts));
    size_t *lens = (size_t *)malloc((n + 2) * sizeof(*lens));
    if (!parts || !lens) { free(parts); free(lens); return false; }
    size_t pc = 0;
    size_t i = 0;
    while (i < n) {
        size_t j = i;
        while (j < n && path[j] != '/' && path[j] != '\\') j++;
        size_t seg_len = j - i;
        if (seg_len && !(seg_len == 1 && path[i] == '.')) {
            parts[pc] = path + i;
            lens[pc] = seg_len;
            pc++;
        }
        i = (j < n) ? j + 1 : j;
    }

    bool sensitive = false;
    char **lowered = (char **)malloc(pc * sizeof(*lowered));
    if (!lowered) { free(parts); free(lens); return false; }
    for (size_t k = 0; k < pc; k++) {
        lowered[k] = lower_dup(parts[k], lens[k]);
        if (!lowered[k]) continue;
        if (strcmp(lowered[k], ".git") == 0 || strcmp(lowered[k], ".ssh") == 0)
            sensitive = true;
    }
    if (pc && lowered[pc - 1]) {
        const char *name = lowered[pc - 1];
        for (int k = 0; SENSITIVE_NAMES[k]; k++) {
            if (strcmp(name, SENSITIVE_NAMES[k]) == 0) sensitive = true;
        }
    }
    for (size_t k = 0; k < pc; k++) free(lowered[k]);
    free(lowered);
    free(parts);
    free(lens);
    return sensitive;
}

/* ---- _denied(message) ------------------------------------------------ */
char *w11_editapprov_59a256_denied(const char *message) {
    const char *m = message ? message : "";
    char *esc = json_escape_string(m);            /* includes surrounding quotes */
    if (!esc) return NULL;
    size_t need = strlen("{\"error\": ") + strlen(esc) + strlen("}") + 1;
    char *out = (char *)malloc(need);
    if (!out) { free(esc); return NULL; }
    snprintf(out, need, "{\"error\": %s}", esc);
    free(esc);
    return out;
}
