/*
 * session_live_listing.c - C home for upstream Hermes gateway/session_live_listing.py.
 * PoP: w16_sessionliv_6d3242_execution_state @ gateway/session_live_listing.py:_execution_state
 * PoP: w16_sessionliv_6d3242_masked_pending @ gateway/session_live_listing.py:_masked_pending
 *
 * Faithful ports of the loop-side pure helpers. The store read / SessionHandle
 * construction (async, IO-bound) stays with the live Python owner.
 * LISTING_BATCH from the live module is 256; batches are the caller's concern.
 */
#include "hermes_json.h"
#include <stdlib.h>
#include <string.h>
#include "noc_w16_sessionliv_6d3242.h"

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static int has_status(const json_t *pending, const char *want) {
    size_t n = json_len(pending);
    for (size_t i = 0; i < n; i++) {
        const char *s = json_get_str(json_get(pending, i), "status", "");
        if (s && !strcmp(s, want)) return 1;
    }
    return 0;
}

struct json_t *w16_sessionliv_6d3242_execution_state(const struct json_t *pending) {
    if (has_status(pending, "unknown")) return json_string("unknown");
    if (has_status(pending, "started")) return json_string("running");
    return json_string("idle");
}

typedef struct { char *key; json_t *row; } merged_t;

static long find_merged(merged_t *m, size_t n, const char *key) {
    for (size_t i = 0; i < n; i++)
        if (m[i].key && key && !strcmp(m[i].key, key)) return (long)i;
    return -1;
}

static void put_merged(merged_t **mp, size_t *np, size_t *cap, const char *key, json_t *row) {
    long idx = find_merged(*mp, *np, key);
    if (idx >= 0) {
        json_free((*mp)[idx].row);
        (*mp)[idx].row = row;
        return;
    }
    if (*np == *cap) {
        size_t nc = *cap ? *cap * 2 : 8;
        merged_t *nm = (merged_t *)realloc(*mp, nc * sizeof(merged_t));
        if (!nm) { json_free(row); return; }
        *mp = nm;
        *cap = nc;
    }
    (*mp)[*np].key = dupstr(key);
    (*mp)[*np].row = row;
    (*np)++;
}

struct json_t *w16_sessionliv_6d3242_masked_pending(const struct json_t *rows,
                                                    const struct json_t *settling) {
    size_t sn = json_object_size(settling);
    /* if not live.settling: return rows (no merge, no sort). */
    if (sn == 0) return json_copy(rows);

    merged_t *m = NULL;
    size_t n = 0, cap = 0;
    size_t rn = json_len(rows);
    for (size_t i = 0; i < rn; i++) {
        json_t *row = json_get(rows, i);
        put_merged(&m, &n, &cap, json_get_str(row, "admission_id", ""), json_copy(row));
    }
    for (size_t j = 0; j < sn; j++) {
        const char *key = json_object_get_key_at(settling, j);
        put_merged(&m, &n, &cap, key, json_copy(json_object_get_at(settling, j)));
    }
    /* Stable insertion sort by row['seq'] ascending (Python sorted is stable). */
    for (size_t i = 1; i < n; i++) {
        merged_t cur = m[i];
        double cs = json_get_num(cur.row, "seq", 0.0);
        size_t j = i;
        while (j > 0 && json_get_num(m[j - 1].row, "seq", 0.0) > cs) {
            m[j] = m[j - 1];
            j--;
        }
        m[j] = cur;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < n; i++) json_append(out, json_copy(m[i].row));
    for (size_t i = 0; i < n; i++) { free(m[i].key); json_free(m[i].row); }
    free(m);
    return out;
}
