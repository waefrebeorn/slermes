/*
 * methods_complete_helpers.c - C11 port of upstream
 * tui_gateway/methods_complete_helpers.py (gate w26_methodscom_94b48f).
 *
 * PoP: w26_methodscom_94b48f_rank @ tui_gateway/methods_complete_helpers.py:_fuzzy_basename_rank
 * PoP: w26_methodscom_94b48f_keep_dirnames @ tui_gateway/methods_complete_helpers.py:_walk_repo_files
 * PoP: w26_methodscom_94b48f_constants @ tui_gateway/methods_complete_helpers.py:<module constants>
 *
 * The repo listing (git ls-files, os.walk, the per-root TTL cache) is a
 * subprocess/filesystem seam and stays in the caller.  What is ported is the
 * decision those listings feed: the (tier, length) rank of one candidate path
 * against a typed query, and which directories a fallback walk may descend
 * into.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_methodscom_94b48f.h"

/* _FUZZY_CACHE_TTL_S / _FUZZY_CACHE_MAX_FILES */
#define MC_TTL_S 5.0
#define MC_MAX_FILES 20000

/* _FUZZY_FALLBACK_EXCLUDES, kept in sorted() order (the set has no order). */
static const char *const MC_EXCLUDES[] = {
    ".cache", ".git", ".hg", ".mypy_cache", ".next", ".pytest_cache",
    ".ruff_cache", ".svn", ".venv", "__pycache__", "build", "dist",
    "node_modules", "target", "venv",
};
#define MC_EXCLUDE_COUNT (sizeof(MC_EXCLUDES) / sizeof(MC_EXCLUDES[0]))

static int mc_is_upper_byte(char c) { return c >= 'A' && c <= 'Z'; }

/* len() over Python str semantics: one unit per code point. */
static size_t mc_cp_len(const char *s, size_t n) {
    size_t i, count = 0;
    for (i = 0; i < n; i++) {
        if (((unsigned char)s[i] & 0xC0) != 0x80) count++;
    }
    return count;
}

static char *mc_lower(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    size_t i;
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        char c = s[i];
        out[i] = mc_is_upper_byte(c) ? (char)(c - 'A' + 'a') : c;
    }
    out[n] = '\0';
    return out;
}

/* all(any(c == q for c in it) for q in ql): ql is a subsequence of nl. */
static int mc_subsequence(const char *nl, const char *ql) {
    size_t i = 0, j = 0;
    while (ql[j]) {
        while (nl[i] && nl[i] != ql[j]) i++;
        if (!nl[i]) return 0;
        i++;
        j++;
    }
    return 1;
}

/* p.lower().startswith(ql) over one word-boundary part. */
static int mc_part_hit(const char *p, size_t len, const char *ql) {
    size_t i;
    for (i = 0; ql[i]; i++) {
        char c;
        if (i >= len) return 0;
        c = p[i];
        if (mc_is_upper_byte(c)) c = (char)(c - 'A' + 'a');
        if (c != ql[i]) return 0;
    }
    return 1;
}

/* Word-boundary / camelCase prefix hit over the -_. and uppercase split. */
static int mc_word_prefix(const char *name, size_t n, const char *ql) {
    size_t buf_len = 0, i;
    char *buf = (char *)malloc(n + 1);
    int hit = 0;

    if (!buf) return 0;
    for (i = 0; i <= n; i++) {
        char ch = (i < n) ? name[i] : '\0';
        int split = (i == n) || ch == '-' || ch == '_' || ch == '.' ||
                    (mc_is_upper_byte(ch) && buf_len > 0 &&
                     !mc_is_upper_byte(buf[buf_len - 1]));
        if (!split) { buf[buf_len++] = ch; continue; }
        if (buf_len && mc_part_hit(buf, buf_len, ql)) { hit = 1; break; }
        buf_len = 0;
        if (i < n && ch != '-' && ch != '_' && ch != '.') buf[buf_len++] = ch;
    }
    free(buf);
    return hit;
}

json_t *w26_methodscom_94b48f_rank(const json_t *req) {
    const json_t *name_node = json_obj_get(req, "name");
    const json_t *query_node = json_obj_get(req, "query");
    const char *name = (name_node && name_node->type == JSON_STRING && name_node->str_val)
                           ? name_node->str_val : "";
    const char *query = (query_node && query_node->type == JSON_STRING && query_node->str_val)
                            ? query_node->str_val : "";
    size_t n = strlen(name), nlen = mc_cp_len(name, strlen(name));
    char *nl, *ql;
    json_t *out;

    if (!query[0]) {
        out = json_array();
        json_append(out, json_number(W26_METHODSCOM_94B48F_TIER_SUBSTRING));
        json_append(out, json_number((double)nlen));
        return out;
    }
    nl = mc_lower(name, n);
    ql = mc_lower(query, strlen(query));
    if (!nl || !ql) { free(nl); free(ql); return json_null(); }

    {
        int tier = -1;
        if (strcmp(nl, ql) == 0) tier = W26_METHODSCOM_94B48F_TIER_EXACT;
        else if (strncmp(nl, ql, strlen(ql)) == 0) tier = W26_METHODSCOM_94B48F_TIER_PREFIX;
        else if (mc_word_prefix(name, n, ql)) tier = W26_METHODSCOM_94B48F_TIER_WORD;
        else if (strstr(nl, ql) != NULL) tier = W26_METHODSCOM_94B48F_TIER_SUBSTRING;
        else if (mc_subsequence(nl, ql)) tier = W26_METHODSCOM_94B48F_TIER_SUBSEQUENCE;
        free(nl);
        free(ql);
        if (tier < 0) return json_null();
        out = json_array();
        json_append(out, json_number((double)tier));
        json_append(out, json_number((double)nlen));
        return out;
    }
}

json_t *w26_methodscom_94b48f_keep_dirnames(const json_t *req) {
    const json_t *dirs = json_obj_get(req, "dirnames");
    json_t *out = json_array();
    size_t i, j;

    if (!dirs || dirs->type != JSON_ARRAY) return out;
    for (i = 0; i < json_len(dirs); i++) {
        const json_t *d = json_get(dirs, i);
        const char *s;
        int keep = 1;
        if (!d || d->type != JSON_STRING || !d->str_val) continue;
        s = d->str_val;
        if (s[0] == '.') keep = 0;
        for (j = 0; keep && j < MC_EXCLUDE_COUNT; j++) {
            if (strcmp(s, MC_EXCLUDES[j]) == 0) keep = 0;
        }
        if (keep) json_append(out, json_string(s));
    }
    return out;
}

json_t *w26_methodscom_94b48f_constants(void) {
    json_t *o = json_object();
    json_t *ex = json_array();
    size_t i;
    for (i = 0; i < MC_EXCLUDE_COUNT; i++) json_append(ex, json_string(MC_EXCLUDES[i]));
    json_set(o, "cache_ttl_s", json_number(MC_TTL_S));
    json_set(o, "cache_max_files", json_number(MC_MAX_FILES));
    json_set(o, "fallback_excludes", ex);
    return o;
}
