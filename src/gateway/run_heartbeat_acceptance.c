/*
 * run_heartbeat_acceptance.c - C11 port of upstream gateway/run_heartbeat_acceptance.py
 * (gate w25_runheartbe_5f43e5).
 *
 * PoP: w25_runheartbe_5f43e5_identity @ gateway/run_heartbeat_acceptance.py:admit_heartbeat
 * PoP: w25_runheartbe_5f43e5_owner_is_current @ gateway/run_heartbeat_acceptance.py:heartbeat_owner_is_current
 * PoP: w25_runheartbe_5f43e5_resolve_owner @ gateway/run_heartbeat_acceptance.py:resolve_heartbeat_owner
 * PoP: w25_runheartbe_5f43e5_settle @ gateway/run_heartbeat_acceptance.py:settle_heartbeat_attempt
 * PoP: w25_runheartbe_5f43e5_pending_has_heartbeat @ gateway/run_heartbeat_acceptance.py:admit_heartbeat
 * PoP: w25_runheartbe_5f43e5_constants @ gateway/run_heartbeat_acceptance.py:<literal tables>
 *
 * The session store, the persistence handle, the executor, the HeartbeatManager
 * and the adapter are seams.  What is ported here is the part that is a pure
 * function of values: which lineage a tick may run in, whether an unexecuted
 * attempt is refunded, the identity it is deduplicated on, and whether a
 * pending admission already covers the route.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_runheartbe_5f43e5.h"

/* Python truthiness of a JSON value. */
static int rh_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

static int rh_num(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* Python '==': absent and null are the same value, True == 1, 1 == 1.0. */
static int rh_equal(const json_t *a, const json_t *b) {
    double x = 0.0, y = 0.0;
    size_t i, n;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if (rh_num(a, &x) && rh_num(b, &y)) return x == y;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    if (a->type == JSON_ARRAY && b->type == JSON_ARRAY) {
        n = json_len(a);
        if (n != json_len(b)) return 0;
        for (i = 0; i < n; i++)
            if (!rh_equal(json_get(a, i), json_get(b, i))) return 0;
        return 1;
    }
    if (a->type == JSON_OBJECT && b->type == JSON_OBJECT) {
        n = json_object_size(a);
        if (n != json_object_size(b)) return 0;
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(a, i);
            if (!k || !rh_equal(json_object_get_at(a, i), json_obj_get(b, k))) return 0;
        }
        return 1;
    }
    return 0;
}

/* ---- a growable text buffer ------------------------------------------- */

typedef struct {
    char *p;
    size_t n, cap;
} rh_buf;

static void rh_buf_add(rh_buf *b, const char *s, size_t n) {
    if (b->n + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        char *grown;
        while (cap < b->n + n + 1) cap *= 2;
        grown = (char *)realloc(b->p, cap);
        if (!grown) return;
        b->p = grown;
        b->cap = cap;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void rh_buf_str(rh_buf *b, const char *s) { rh_buf_add(b, s, strlen(s)); }

/* json.dumps' string literal: quotes, the two mandatory escapes, the short
 * control escapes, and \u00XX for the rest of C0. */
static void rh_buf_json_string(rh_buf *b, const char *s) {
    char esc[8];
    if (!s) s = "";
    rh_buf_add(b, "\"", 1);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
        case '"':  rh_buf_str(b, "\\\""); break;
        case '\\': rh_buf_str(b, "\\\\"); break;
        case '\n': rh_buf_str(b, "\\n"); break;
        case '\r': rh_buf_str(b, "\\r"); break;
        case '\t': rh_buf_str(b, "\\t"); break;
        case '\b': rh_buf_str(b, "\\b"); break;
        case '\f': rh_buf_str(b, "\\f"); break;
        default:
            if (c < 0x20) {
                snprintf(esc, sizeof esc, "\\u%04x", c);
                rh_buf_str(b, esc);
            } else {
                rh_buf_add(b, (const char *)&c, 1);
            }
        }
    }
    rh_buf_add(b, "\"", 1);
}

/* The shortest text that round-trips -- json.dumps' float repr. */
static void rh_buf_number(rh_buf *b, double d) {
    char tmp[64];
    int prec;
    if (d == (double)(long long)d && fabs(d) < 1e15) {
        snprintf(tmp, sizeof tmp, "%.0f", d);
    } else {
        for (prec = 15; prec <= 17; prec++) {
            snprintf(tmp, sizeof tmp, "%.*g", prec, d);
            if (strtod(tmp, NULL) == d) break;
        }
    }
    rh_buf_str(b, tmp);
}

/* One value the way json.dumps renders it (used for the identity list). */
static void rh_buf_value(rh_buf *b, const json_t *v) {
    size_t i, n;
    if (!v) { rh_buf_str(b, "null"); return; }
    switch (v->type) {
    case JSON_NULL:   rh_buf_str(b, "null"); break;
    case JSON_BOOL:   rh_buf_str(b, v->bool_val ? "true" : "false"); break;
    case JSON_NUMBER: rh_buf_number(b, v->num_val); break;
    case JSON_STRING: rh_buf_json_string(b, v->str_val); break;
    case JSON_ARRAY:
        n = json_len(v);
        rh_buf_str(b, "[");
        for (i = 0; i < n; i++) {
            if (i) rh_buf_str(b, ",");
            rh_buf_value(b, json_get(v, i));
        }
        rh_buf_str(b, "]");
        break;
    case JSON_OBJECT:
        n = json_object_size(v);
        rh_buf_str(b, "{");
        for (i = 0; i < n; i++) {
            const char *k = json_object_get_key_at(v, i);
            if (i) rh_buf_str(b, ",");
            rh_buf_json_string(b, k ? k : "");
            rh_buf_str(b, ":");
            rh_buf_value(b, json_object_get_at(v, i));
        }
        rh_buf_str(b, "}");
        break;
    }
}

json_t *w25_runheartbe_5f43e5_identity(const json_t *req) {
    rh_buf b;
    const json_t *sid, *created, *fires;
    json_t *out;

    if (!req || req->type != JSON_OBJECT) return json_null();
    sid = json_obj_get(req, "session_id");
    created = json_obj_get(req, "created_at");
    fires = json_obj_get(req, "fire_count");

    memset(&b, 0, sizeof b);
    rh_buf_str(&b, "[");
    rh_buf_json_string(&b, W25_RUNHEARTBE_5F43E5_IDENTITY_PREFIX);
    rh_buf_str(&b, ",");
    rh_buf_value(&b, sid);
    rh_buf_str(&b, ",");
    rh_buf_value(&b, created);
    rh_buf_str(&b, ",");
    rh_buf_value(&b, fires);
    rh_buf_str(&b, "]");

    out = json_string(b.p ? b.p : "[]");
    free(b.p);
    return out;
}

/* heartbeat_owner_is_current over the store row the caller looked up. */
static int rh_owner_current(const json_t *expected, const json_t *current) {
    const json_t *suspended, *held;
    if (!rh_truthy(expected)) return 1;
    if (!current || current->type != JSON_OBJECT) return 0;
    suspended = json_obj_get(current, "suspended");
    if (rh_truthy(suspended)) return 0;
    held = json_obj_get(current, "session_id");
    return rh_equal(held, expected);
}

json_t *w25_runheartbe_5f43e5_owner_is_current(const json_t *req) {
    if (!req || req->type != JSON_OBJECT) return json_bool(true);
    return json_bool(rh_owner_current(json_obj_get(req, "expected"),
                                      json_obj_get(req, "current")) ? true : false);
}

json_t *w25_runheartbe_5f43e5_resolve_owner(const json_t *req) {
    const json_t *expected, *resolved, *tip, *current;
    json_t *out;

    if (!req || req->type != JSON_OBJECT) return json_bool(true);
    expected = json_obj_get(req, "expected");
    /* No heartbeat lineage on the event: normal resolution stands. */
    if (!rh_truthy(expected)) return json_bool(true);

    resolved = json_obj_get(req, "resolved");
    if (!rh_equal(resolved, expected)) {
        /* The runner recomputes the lineage tip in an executor; the value it
         * would return is the caller's seam. */
        tip = json_obj_get(req, "tip");
        if (!rh_equal(tip, resolved)) return json_bool(false);
    }
    /* event._heartbeat_resolved_session_id = resolved, then the current check. */
    current = json_obj_get(req, "current");
    out = json_bool(rh_owner_current(resolved, current) ? true : false);
    return out;
}

json_t *w25_runheartbe_5f43e5_settle(const json_t *req) {
    const json_t *started, *raises;
    json_t *out;
    int refunded, warned;

    if (!req || req->type != JSON_OBJECT) return json_null();
    started = json_obj_get(req, "execution_started");
    raises = json_obj_get(req, "abandon_raises");
    refunded = 0;
    warned = 0;
    if (!rh_truthy(started)) {
        /* manager.abandon_fire(), whose exception is logged and swallowed */
        if (rh_truthy(raises)) warned = 1;
        else refunded = 1;
    }
    out = json_object();
    json_set(out, "abandoned", json_bool(refunded ? true : false));
    json_set(out, "warned", json_bool(warned ? true : false));
    return out;
}

/* row.payload.get('native_text_v1', {}).get('automation', {}).get('heartbeat')
 * or row.payload.get('local_automation_v1', {}).get('heartbeat') */
static int rh_pending_row(const json_t *row) {
    const json_t *payload, *native, *automation, *local, *beat;
    if (!row || row->type != JSON_OBJECT) return 0;
    payload = json_obj_get(row, "payload");
    if (!payload || payload->type != JSON_OBJECT) return 0;

    native = json_obj_get(payload, "native_text_v1");
    if (native && native->type == JSON_OBJECT) {
        automation = json_obj_get(native, "automation");
        if (automation && automation->type == JSON_OBJECT) {
            beat = json_obj_get(automation, "heartbeat");
            if (rh_truthy(beat)) return 1;
        }
    }
    local = json_obj_get(payload, "local_automation_v1");
    if (local && local->type == JSON_OBJECT) {
        beat = json_obj_get(local, "heartbeat");
        if (rh_truthy(beat)) return 1;
    }
    return 0;
}

json_t *w25_runheartbe_5f43e5_pending_has_heartbeat(const json_t *req) {
    const json_t *pending;
    size_t i, n;
    if (!req || req->type != JSON_OBJECT) return json_bool(false);
    pending = json_obj_get(req, "pending");
    n = (pending && pending->type == JSON_ARRAY) ? json_len(pending) : 0;
    for (i = 0; i < n; i++)
        if (rh_pending_row(json_get(pending, i))) return json_bool(true);
    return json_bool(false);
}

json_t *w25_runheartbe_5f43e5_constants(void) {
    json_t *o = json_object(), *keys = json_array();
    json_set(o, "identity_prefix", json_string(W25_RUNHEARTBE_5F43E5_IDENTITY_PREFIX));
    json_append(keys, json_string("native_text_v1"));
    json_append(keys, json_string("automation"));
    json_append(keys, json_string("local_automation_v1"));
    json_append(keys, json_string("heartbeat"));
    json_set(o, "payload_path", keys);
    return o;
}
