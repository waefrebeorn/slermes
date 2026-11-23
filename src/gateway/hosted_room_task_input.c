/*
 * hosted_room_task_input.c - Bounded immutable input references (C11 port).
 *
 * PoP: hrti_validate_task_input @ gateway/hosted_room_task_input.py:validate_task_input
 * PoP: hrti_max_task_input_events @ gateway/hosted_room_task_input.py:MAX_TASK_INPUT_EVENTS
 *
 * int64 values cross the JSON boundary as decimal strings (libjson stores
 * numbers as double) and are recovered with strtoll, matching Python's
 * unbounded ints up to the 2**63-1 ceiling the live validator enforces.
 * Error strings are returned verbatim; NULL means success (out is populated).
 */
#include "noc_hrti.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int hrti_max_task_input_events(void) { return HRTI_MAX_TASK_INPUT_EVENTS; }

/* 1 = clean, 2 = out-of-range (strtoll ERANGE), 0 = not an integer */
static int hrti_parse_i64(const json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_STRING) {
        if (!v->str_val || !*v->str_val) return 0;
        errno = 0;
        char *end = NULL;
        long long x = strtoll(v->str_val, &end, 10);
        if (errno == ERANGE) { *out = x; return 2; }
        if (!end || end == v->str_val || *end) return 0;
        *out = x;
        return 1;
    }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return 1; }
    return 0;
}

const char *hrti_validate_task_input(const json_t *value, json_t **out) {
    *out = NULL;
    if (!value || value->type != JSON_OBJECT || json_object_size(value) != 2 ||
        !json_has(value, "watermark") || !json_has(value, "event_seqs"))
        return "invalid task input context";

    json_t *wv = json_obj_get(value, "watermark");
    json_t *sv = json_obj_get(value, "event_seqs");

    long long wm = 0;
    int wr = hrti_parse_i64(wv, &wm);
    if (!wr || wv->type == JSON_BOOL || wm < 0)
        return "invalid task input watermark";
    if (wr == 2) wm = LLONG_MAX; /* Python int > 2**63-1: no seq can exceed it */

    if (!sv || sv->type != JSON_ARRAY) return "task input context exceeded its bound";
    size_t n = json_len(sv);
    if (n < 1 || n > HRTI_MAX_TASK_INPUT_EVENTS)
        return "task input context exceeded its bound";

    long long seqs[HRTI_MAX_TASK_INPUT_EVENTS];
    long long prev = wm;
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_get(sv, i);
        long long seq = 0;
        int sr = hrti_parse_i64(e, &seq);
        if (sr != 1 || e->type == JSON_BOOL || !(prev < seq && seq <= LLONG_MAX))
            return "task input sequences must be increasing after the watermark";
        seqs[i] = seq;
        prev = seq;
    }

    json_t *o = json_object();
    char buf[64];
    snprintf(buf, sizeof(buf), "%lld", wm);
    json_set(o, "watermark", json_string(buf));
    json_t *arr = json_array();
    for (size_t i = 0; i < n; i++) {
        snprintf(buf, sizeof(buf), "%lld", seqs[i]);
        json_append(arr, json_string(buf));
    }
    json_set(o, "event_seqs", arr);
    *out = o;
    return NULL;
}
