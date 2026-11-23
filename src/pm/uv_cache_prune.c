/*
 * uv_cache_prune.c — C home for upstream Hermes pm/uv_cache_prune.py.
 *
 * PoP: w6_uvcachepru_3aea47_normalized @ pm/uv_cache_prune.py:_normalized
 * PoP: w6_uvcachepru_3aea47_dist_name @ pm/uv_cache_prune.py:prune_uv_cache_to_lock.dist_name
 *
 * The filesystem side of pruning (tomllib lock parsing, os.walk of wheels /
 * sdists, shutil.rmtree of stale buckets) is platform plumbing and is not
 * reimplemented. The tested behaviour is the pure name-classification logic:
 * PEP 503 normalization and the *.dist-info bucket-name extraction regex,
 * both driven by injected marker filenames so the gate is deterministic.
 */
#include "noc_w6_uvcachepru_3aea47.h"
#include "hermes_json.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* re.sub(r"[-_.]+", "-", name).lower() for ASCII names. */
static char *normalized_alloc(const char *name)
{
    size_t n, i, j = 0;
    int in_sep = 0;
    char *out;

    if (!name)
        return NULL;
    n = strlen(name);
    out = (char *)malloc(n + 1);
    if (!out)
        return NULL;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)name[i];
        if (c == '-' || c == '_' || c == '.') {
            if (!in_sep) {
                out[j++] = '-';
                in_sep = 1;
            }
        } else {
            out[j++] = (char)tolower(c);
            in_sep = 0;
        }
    }
    out[j] = '\0';
    return out;
}

struct json_t *w6_uvcachepru_3aea47_normalized(const char *name)
{
    char *norm = normalized_alloc(name);
    json_t *res;
    if (!norm)
        return json_null();
    res = json_string(norm);
    free(norm);
    return res;
}

static int is_dist_char(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '.';
}

/* Emulate re.match(r"([A-Za-z0-9_.]+?)-\d[^-]*\.dist-info", s).
 * Returns a malloc'd copy of group(1) on match, else NULL. re.match anchors
 * at the start but NOT at the end; group(1) is non-greedy (shortest). */
static char *dist_group1(const char *s)
{
    static const char literal[] = ".dist-info";
    const size_t litlen = sizeof(literal) - 1;
    size_t n = strlen(s), L;

    for (L = 1; L <= n; L++) {
        size_t i, start, q;
        int ok = 1, matched = 0;

        /* group(1): chars [0, L) must all be [A-Za-z0-9_.]. */
        for (i = 0; i < L; i++) {
            if (!is_dist_char((unsigned char)s[i])) {
                ok = 0;
                break;
            }
        }
        /* A charset violation can only grow with L: stop entirely. */
        if (!ok)
            break;
        if (L >= n || s[L] != '-' || L + 1 >= n)
            continue;
        if (s[L + 1] < '0' || s[L + 1] > '9')
            continue;

        /* After -(digit), [^-]* then literal ".dist-info" somewhere. */
        start = L + 2;
        for (q = start; q + litlen <= n; q++) {
            size_t k;
            int seg_ok = 1;
            for (k = start; k < q; k++) {
                if (s[k] == '-') {
                    seg_ok = 0;
                    break;
                }
            }
            if (!seg_ok)
                break;
            if (memcmp(s + q, literal, litlen) == 0) {
                matched = 1;
                break;
            }
        }
        if (matched) {
            char *g = (char *)malloc(L + 1);
            if (!g)
                return NULL;
            memcpy(g, s, L);
            g[L] = '\0';
            return g;
        }
    }
    return NULL;
}

struct json_t *w6_uvcachepru_3aea47_dist_name(const struct json_t *markers)
{
    size_t n, i;

    if (!markers || markers->type != JSON_ARRAY)
        return json_null();
    n = json_len(markers);
    for (i = 0; i < n; i++) {
        json_t *m = json_get(markers, i);
        char *g, *norm;
        json_t *res;
        if (!m || m->type != JSON_STRING || !m->str_val)
            continue;
        g = dist_group1(m->str_val);
        if (!g)
            continue;
        norm = normalized_alloc(g);
        free(g);
        if (!norm)
            return json_null();
        res = json_string(norm);
        free(norm);
        return res;
    }
    return json_null();
}
