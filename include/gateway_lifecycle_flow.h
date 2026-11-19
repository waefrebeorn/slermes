/*
 * gateway_lifecycle_flow.h — shared trace recorder + JSON helpers for the
 * gateway lifecycle FLOW cores (session_policy / run_startup / run_shutdown).
 *
 * These ports model DECISION SEQUENCES: call order, branch conditions, early
 * returns, exception->fallback paths and mutation order of the live Python.
 * All live I/O (logging, runtime-status writes, allowlist probes, filesystem
 * readiness, forward-policy classification, exit requests) enters through
 * per-flow seam callback structs; the cores themselves perform no I/O.
 *
 * A gflow_trace_t collects one JSON event per seam call in call order. The
 * oracle compares that ordered event list (plus return value / mutated state)
 * against recording monkeypatch stubs driving the LIVE Python with the same
 * scripted results.
 *
 * Everything here is static inline so each harness translation unit (which
 * #includes exactly one flow .c) carries its own copy — no cross-object
 * symbols, no build registration required.
 */
#ifndef GATEWAY_LIFECYCLE_FLOW_H
#define GATEWAY_LIFECYCLE_FLOW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Trace recorder (harness infrastructure, no Python counterpart) ─────────
 * events is a JSON_ARRAY of event objects. gflow_trace_add takes ownership of
 * ev. The trace never leaves the caller. */
typedef struct gflow_trace {
    json_t *events;   /* owned JSON_ARRAY; NULL until first add */
} gflow_trace_t;

static inline void gflow_trace_add(gflow_trace_t *t, json_t *ev) {
    if (!t) { json_free(ev); return; }
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}

static inline void gflow_trace_free(gflow_trace_t *t) {
    if (t && t->events) { json_free(t->events); t->events = NULL; }
}

/* ── Python truthiness for a JSON value (NULL pointer == Python None) ── */
static inline bool gflow_truth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

/* String value of a node, or NULL when absent / not a string. */
static inline const char *gflow_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

/* ── Event builders ── */
static inline json_t *gflow_ev(const char *name) {
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    return e;
}
static inline void gflow_evs(json_t *e, const char *k, const char *v) {
    json_set(e, k, v ? json_string(v) : json_null());
}
static inline void gflow_evb(json_t *e, const char *k, bool v) {
    json_set(e, k, json_bool(v));
}
static inline void gflow_evn(json_t *e, const char *k, double v) {
    json_set(e, k, json_number(v));
}
/* Borrowed value copied into the event (NULL -> json null). */
static inline void gflow_evj(json_t *e, const char *k, const json_t *v) {
    json_set(e, k, v ? json_copy(v) : json_null());
}

/* ── Small JSON access helpers ── */
static inline const json_t *gflow_obj_get(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static inline bool gflow_is_obj(const json_t *v) {
    return v && v->type == JSON_OBJECT;
}
static inline bool gflow_is_arr(const json_t *v) {
    return v && v->type == JSON_ARRAY;
}
static inline bool gflow_is_str(const json_t *v) {
    return v && v->type == JSON_STRING;
}
static inline bool gflow_is_bool(const json_t *v) {
    return v && v->type == JSON_BOOL;
}
static inline bool gflow_is_null(const json_t *v) {
    return !v || v->type == JSON_NULL;
}

/* Python str() for scalars used in log args / refusals; containers are kept
 * as JSON by the callers that need them. malloc'd; caller frees. */
static inline char *gflow_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && d < 9.2e18 && d > -9.2e18)
                snprintf(buf, sizeof(buf), "%lld", (long long)d);
            else
                snprintf(buf, sizeof(buf), "%.17g", d);
            return strdup(buf);
        }
        default: {
            char *s = json_serialize(v);
            return s ? s : strdup("");
        }
    }
}

#ifdef __cplusplus
}
#endif

#endif /* GATEWAY_LIFECYCLE_FLOW_H */
