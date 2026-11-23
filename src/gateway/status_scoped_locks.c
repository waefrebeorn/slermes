/*
 * status_scoped_locks.c — real C11 port of upstream Hermes gateway/status_scoped_locks.py.
 * PoP: w14_statusscop_7d459d_same_owner @ gateway/status_scoped_locks.py:_same_owner
 * Ported: _same_owner and the pure helpers it reads (_pid_from_record,
 * _start_times_conflict) so the ownership decision can be differentially
 * gated against the live module. Process-probing / filesystem helpers remain
 * external seams and are excluded.
 */
#include "hermes_json.h"
#include "noc_w14_statusscop_7d459d.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

typedef struct { int has; long long v; } pidres;

static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL:   return 0;
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
        case JSON_ARRAY:  return v->c.count ? 1 : 0;
        case JSON_OBJECT: return v->c.count ? 1 : 0;
        default: return 0;
    }
}

/* Python == for the JSON value shapes the fixtures exercise. A NULL node is
 * a missing key, which Python reads as None. */
static int jval_eq(const json_t *a, const json_t *b) {
    size_t i;
    if (!a) return !b || b->type == JSON_NULL;
    if (!b) return a->type == JSON_NULL;
    if (a->type == JSON_NULL && b->type == JSON_NULL) return 1;
    if (a->type == JSON_BOOL && b->type == JSON_BOOL) return a->bool_val == b->bool_val;
    if ((a->type == JSON_BOOL && b->type == JSON_NUMBER) ||
        (a->type == JSON_NUMBER && b->type == JSON_BOOL)) {
        double x = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double y = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return x == y;
    }
    if (a->type == JSON_NUMBER && b->type == JSON_NUMBER) return a->num_val == b->num_val;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) {
        if (a->c.count != b->c.count) return 0;
        for (i = 0; i < a->c.count; i++) if (!jval_eq(a->c.items[i], b->c.items[i])) return 0;
        return 1;
    }
    if (a->type == JSON_OBJECT && b->type == JSON_OBJECT) {
        if (a->c.count != b->c.count) return 0;
        for (i = 0; i < a->c.count; i++) {
            const json_t *vb = json_obj_get(b, a->c.keys[i]);
            if (!vb || !jval_eq(a->c.items[i], vb)) return 0;
        }
        return 1;
    }
    return 0;
}

/* gateway.status._pid_from_record(record, key='pid') -> Optional[int]. */
static pidres pid_from_record(const json_t *rec, const char *key) {
    pidres r;
    const json_t *v;
    r.has = 0; r.v = 0;
    if (!rec || rec->type != JSON_OBJECT) return r;
    v = json_obj_get(rec, key);
    if (!v || v->type == JSON_NULL) return r;
    if (v->type == JSON_NUMBER) { r.has = 1; r.v = (long long)v->num_val; return r; }
    if (v->type == JSON_BOOL) { r.has = 1; r.v = v->bool_val ? 1 : 0; return r; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        char *end;
        long long x;
        errno = 0;
        x = strtoll(s, &end, 10);
        if (end == s || errno == ERANGE) return r;
        while (*end && isspace((unsigned char)*end)) end++;
        if (*end) return r;
        r.has = 1; r.v = x; return r;
    }
    return r;
}

/* gateway.status._start_times_conflict(recorded, current). */
static int start_times_conflict(const json_t *recorded, const json_t *current) {
    if (!recorded || recorded->type == JSON_NULL) return 0;
    if (!current || current->type == JSON_NULL) return 0;
    return !jval_eq(current, recorded);
}

struct json_t *w14_statusscop_7d459d_same_owner(const struct json_t *existing,
                                                const struct json_t *holder) {
    struct json_t *out = json_object();
    pidres hp, ep;
    const json_t *hn, *en, *es, *hs;
    int same = 1;
    if (!holder || holder->type != JSON_OBJECT || !py_truthy(json_obj_get(holder, "pid")))
        return json_bool(1);
    hp = pid_from_record(holder, "pid");
    ep = pid_from_record(existing, "pid");
    if (hp.has != ep.has || (hp.has && hp.v != ep.v)) same = 0;
    hn = json_obj_get(holder, "pidns");
    en = json_obj_get(existing, "pidns");
    if (!jval_eq(hn, en)) same = 0;
    if (same) {
        es = json_obj_get(existing, "start_time");
        hs = json_obj_get(holder, "start_time");
        same = !start_times_conflict(es, hs);
    }
    json_free(out);
    return json_bool(same ? 1 : 0);
}
