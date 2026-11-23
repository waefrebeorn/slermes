/*
 * worktree_archive.c — C port of upstream Hermes hermes_cli/worktree_archive.py.
 *
 * PoP: w15_worktreear_5029d2_ref_safe @ hermes_cli/worktree_archive.py:ref_safe
 * PoP: w15_worktreear_5029d2_parse_meta @ hermes_cli/worktree_archive.py:_parse_meta
 * PoP: w15_worktreear_5029d2_split_oversized @ hermes_cli/worktree_archive.py:_split_oversized
 *
 * Byte counts are carried as decimal strings (strtoll on input, snprintf on
 * output) so int64 sizes survive the double-only JSON number representation.
 */
#include "noc_w15_worktreear_5029d2.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int valid_ref_char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
}

struct json_t *w15_worktreear_5029d2_ref_safe(const char *name) {
    if (!name) name = "";
    size_t n = strlen(name);
    char *s1 = (char *)malloc(n + 1);
    if (!s1) return json_string("tree");
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        char c = name[i];
        if (valid_ref_char(c)) {
            s1[m++] = c;
        } else if (m == 0 || s1[m - 1] != '-') {
            s1[m++] = '-';
        }
    }
    s1[m] = '\0';

    /* Python: .strip("-.") */
    size_t a = 0, b = m;
    while (a < b && (s1[a] == '-' || s1[a] == '.')) a++;
    while (b > a && (s1[b - 1] == '-' || s1[b - 1] == '.')) b--;
    size_t len = b - a;
    if (len == 0) { free(s1); return json_string("tree"); }

    /* Python: .replace("..", "-") (non-overlapping, left to right) */
    char *s2 = (char *)malloc(len + 1);
    if (!s2) { free(s1); return json_string("tree"); }
    size_t k = 0;
    for (size_t i = 0; i < len; i++) {
        if (i + 1 < len && s1[a + i] == '.' && s1[a + i + 1] == '.') {
            s2[k++] = '-';
            i++;
        } else {
            s2[k++] = s1[a + i];
        }
    }
    s2[k] = '\0';
    free(s1);

    /* Python: .removesuffix(".lock") */
    if (k >= 5 && strcmp(s2 + k - 5, ".lock") == 0) k -= 5;
    s2[k] = '\0';
    struct json_t *out = json_string(s2);
    free(s2);
    return out;
}

struct json_t *w15_worktreear_5029d2_parse_meta(const char *body) {
    if (!body) return json_object();
    const char *start = strchr(body, '{');
    if (!start) return json_object();
    json_t *parsed = json_parse(start, NULL);
    if (!parsed || parsed->type != JSON_OBJECT) {
        json_free(parsed);
        return json_object();
    }
    return parsed;
}

static long long get_i64(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_STRING) return strtoll(v->str_val, NULL, 10);
    return 0;
}

struct json_t *w15_worktreear_5029d2_split_oversized(const struct json_t *untracked,
                                                    const struct json_t *sizes, double max_mb) {
    struct json_t *arr = json_array();
    if (max_mb <= 0) return arr;
    double cap = max_mb * 1024.0 * 1024.0;
    if (!json_is_array(untracked)) return arr;
    for (size_t i = 0; i < json_len(untracked); i++) {
        struct json_t *p = json_get(untracked, i);
        if (!p || !json_is_string(p)) continue;
        struct json_t *sv = json_obj_get(sizes, p->str_val);
        if (!sv) continue;                 /* live: os.lstat raises OSError -> skipped */
        long long size = get_i64(sv);
        if ((double)size <= cap) continue;
        struct json_t *entry = json_object();
        json_set(entry, "path", json_string(p->str_val));
        char buf[32];
        snprintf(buf, sizeof(buf), "%lld", size);
        json_set(entry, "bytes", json_string(buf));
        json_append(arr, entry);
    }
    return arr;
}
