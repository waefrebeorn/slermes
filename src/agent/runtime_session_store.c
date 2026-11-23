/*
 * runtime_session_store.c — faithful C11 port of agent/runtime_session_store.py.
 * Gate w14_runtimeses_86879c.
 *
 * PoP: w14_runtimeses_86879c_apply_row_annotations @ agent/runtime_session_store.py:apply_row_annotations
 * PoP: w14_runtimeses_86879c_session_number @ agent/runtime_session_store.py:RuntimeSessionStore._session_number
 * PoP: w14_runtimeses_86879c_compression_cooldown @ agent/runtime_session_store.py:RuntimeSessionStore.get_compression_failure_cooldown
 *
 * Filesystem outbox, RPC and thread/lock state are intentionally out of scope;
 * the three deterministic row transformations are ported with explicit seams.
 */
#include "noc_w14_runtimeses_86879c.h"
#include "hermes_json.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int py_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

static int parse_number(const json_t *v, int is_float, double *out) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    if (v->type == JSON_NUMBER) {
        if (!isfinite(v->num_val)) return 0;
        *out = is_float ? v->num_val : (double)(int64_t)v->num_val;
        return 1;
    }
    if (v->type == JSON_STRING) {
        const char *p = v->str_val ? v->str_val : "";
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) return 0;
        errno = 0;
        char *end = NULL;
        double d = strtod(p, &end);
        if (end == p || errno == ERANGE) return 0;
        while (*end && isspace((unsigned char)*end)) end++;
        if (*end != '\0') return 0;  /* Python int()/float() reject trailing junk */
        if (!isfinite(d)) return 0;
        *out = is_float ? d : (double)(int64_t)d;
        return 1;
    }
    return 0;  /* list/dict -> TypeError in Python */
}

struct json_t *w14_runtimeses_86879c_apply_row_annotations(const struct json_t *messages,
        const struct json_t *annotations) {
    json_t *out = json_copy((json_t *)messages);
    if (!out || out->type != JSON_ARRAY || !annotations || annotations->type != JSON_ARRAY) return out;
    size_t n = json_len(messages) < json_len(annotations) ? json_len(messages) : json_len(annotations);
    for (size_t i = 0; i < n; i++) {
        json_t *msg = json_get(out, i);
        const json_t *ann = json_get(annotations, i);
        if (!msg || msg->type != JSON_OBJECT || !ann || ann->type != JSON_OBJECT) continue;
        for (size_t k = 0; k < json_object_size(ann); k++) {
            const char *key = json_object_get_key_at(ann, k);
            json_t *val = json_object_get_at(ann, k);
            if (!key) continue;
            if (!val || val->type == JSON_NULL) {
                json_obj_del(msg, key);  /* None = the owner popped it */
            } else {
                json_set(msg, key, json_copy(val));
            }
        }
    }
    return out;
}

struct json_t *w14_runtimeses_86879c_session_number(const struct json_t *row,
        const char *column, const char *cast_kind, double zero) {
    const json_t *v = (row && row->type == JSON_OBJECT && column)
        ? json_obj_get(row, column) : NULL;
    int is_float = cast_kind && !strcmp(cast_kind, "float");
    double result = zero;
    if (py_truthy(v)) {
        double casted = 0.0;
        if (!parse_number(v, is_float, &casted)) return json_number(zero);  /* except path */
        result = casted > zero ? casted : zero;  /* max(zero, cast(...)) */
    }
    return json_number(result);
}

struct json_t *w14_runtimeses_86879c_compression_cooldown(const struct json_t *row, double now) {
    const json_t *cv = (row && row->type == JSON_OBJECT)
        ? json_obj_get(row, "compression_failure_cooldown_until") : NULL;
    const json_t *ev = (row && row->type == JSON_OBJECT)
        ? json_obj_get(row, "compression_failure_error") : NULL;
    double deadline = 0.0;
    if (!cv || cv->type == JSON_NULL || !parse_number(cv, 1, &deadline)) return json_null();
    if (deadline <= now) return json_null();
    json_t *out = json_object();
    json_set(out, "cooldown_until", json_number(deadline));
    json_set(out, "remaining_seconds", json_number(deadline - now));
    json_set(out, "error", ev ? json_copy((json_t *)ev) : json_null());
    return out;
}
