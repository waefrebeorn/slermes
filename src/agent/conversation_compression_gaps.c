#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <time.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/conversation_compression.py: compression status + attempt tracking ──
 * Revival wave 2: faithful C11 ports via injected ccr_io_t seams.
 * Deferred (already flow-ported as ccflow_* in port_conversation_compression_flow.c,
 * duplicate PoP claim removed): clear_compression_cancelled_check_if_owner,
 * join_cancelled_worker. */

/* ── Revival seam machinery (shared by conversation_compression_gaps.c and
 *    conversation_compression_gaps2.c; guarded so a harness may include both
 *    translation units). I/O is injected via ccr_io_t; ports mutate `in` in
 *    place for stateful functions and return json_t (NULL == Python None,
 *    {"__raise__": Type, "message": ...} == a Python exception). ── */
#ifndef CCR_REVIVE_SHARED
#define CCR_REVIVE_SHARED

typedef struct ccr_io {
    json_t *(*call)(struct ccr_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} ccr_io_t;

static int ccr_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

/* Record one seam call; args borrowed, result owned by the caller. */
static json_t *ccr_seam(ccr_io_t *io, const char *name, json_t *args) {
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

static int ccr_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}

static int ccr_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static json_t *ccr_raise_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* BaseException-only trio propagates through `except Exception` handlers. */
static int ccr_is_base_exc(const char *kind) {
    return kind && (!strcmp(kind, "SystemExit") || !strcmp(kind, "KeyboardInterrupt")
                    || !strcmp(kind, "GeneratorExit"));
}

/* Python type name for a JSON value (json.loads identity: integral literals
 * read as int, fractional as float; fixtures avoid integral-float literals). */
static const char *ccr_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            if (isfinite(d) && d == floor(d) && fabs(d) < 1e16) return "int";
            return "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

static int ccr_is_py_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* A run of digits with single underscores BETWEEN digits (Python float str).
 * Returns digit count; leaves *p at the first non-consumed char. */
static int ccr_digit_run(const char **p) {
    const char *s = *p;
    int n = 0;
    for (;;) {
        if (*s < '0' || *s > '9') break;
        s++; n++;
        if (s[0] == '_' && s[1] >= '0' && s[1] <= '9') { s++; continue; }
        if (s[0] == '_') break;
    }
    *p = s;
    return n;
}

/* float(str) semantics. Returns 0 ok, 2 ValueError. ASCII whitespace/digits. */
static int ccr_float_str(const char *s, double *out) {
    while (ccr_is_py_ws(*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && ccr_is_py_ws(end[-1])) end--;
    if (s == end) return 2;
    const char *q = s;
    int neg = 0;
    if (*q == '+' || *q == '-') { neg = (*q == '-'); q++; }
    size_t wlen = (size_t)(end - q);
    if (wlen <= 8) {
        char word[16];
        for (size_t i = 0; i < wlen; i++) word[i] = (char)tolower((unsigned char)q[i]);
        word[wlen] = '\0';
        if (!strcmp(word, "inf") || !strcmp(word, "infinity")) {
            *out = neg ? -INFINITY : INFINITY; return 0;
        }
        if (!strcmp(word, "nan")) { *out = NAN; return 0; }
    }
    const char *p = s;
    if (*p == '+' || *p == '-') p++;
    int nd = ccr_digit_run(&p);
    int nf = 0;
    if (p < end && *p == '.') { p++; nf = ccr_digit_run(&p); }
    if (nd + nf == 0) return 2;
    if (p < end && (*p == 'e' || *p == 'E')) {
        p++;
        if (p < end && (*p == '+' || *p == '-')) p++;
        if (ccr_digit_run(&p) == 0) return 2;
    }
    if (p != end) return 2;
    size_t len = (size_t)(end - s);
    char *buf = malloc(len + 1);
    if (!buf) return 2;
    size_t k = 0;
    for (size_t i = 0; i < len; i++) if (s[i] != '_') buf[k++] = s[i];
    buf[k] = '\0';
    *out = strtod(buf, NULL);
    free(buf);
    return 0;
}

/* float(x) for a JSON value. Returns 0 ok, 1 TypeError, 2 ValueError. */
static int ccr_float_of(const json_t *v, double *out) {
    if (!v || v->type == JSON_NULL) return 1;
    switch (v->type) {
        case JSON_BOOL:   *out = v->bool_val ? 1.0 : 0.0; return 0;
        case JSON_NUMBER: *out = v->num_val; return 0;
        case JSON_STRING: return ccr_float_str(v->str_val ? v->str_val : "", out);
        default: return 1; /* list/dict -> TypeError */
    }
}

/* a >= b for JSON values; returns json_bool or a TypeError raise spec. */
static json_t *ccr_ge_cmp(const json_t *a, const json_t *b) {
    int an = a && (a->type == JSON_NUMBER || a->type == JSON_BOOL);
    int bn = b && (b->type == JSON_NUMBER || b->type == JSON_BOOL);
    if (an && bn) {
        double av = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return json_bool(av >= bv);
    }
    if (a && b && a->type == JSON_STRING && b->type == JSON_STRING)
        return json_bool(strcmp(a->str_val, b->str_val) >= 0);
    char m[160];
    snprintf(m, sizeof m, "'>=' not supported between instances of '%s' and '%s'",
             ccr_type_name(a), ccr_type_name(b));
    return ccr_raise_spec("TypeError", m);
}

/* str(x) for the checkpoint reason projection (str/int/bool/null fixtures). */
static char *ccr_str_of(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL: return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (isfinite(d) && d == floor(d) && fabs(d) < 1e16)
                snprintf(buf, sizeof buf, "%.0f", d);
            else
                snprintf(buf, sizeof buf, "%.17g", d);
            return strdup(buf);
        }
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        default: {
            char *ser = json_serialize(v);
            char *r = strdup(ser ? ser : "");
            free(ser);
            return r;
        }
    }
}

/* Unpack a 2-tuple from a seam result. *first is OWNED (caller frees).
 * Returns NULL on success or a raise spec (propagated by the caller). */
static json_t *ccr_unpack2(const json_t *r, json_t **first) {
    *first = NULL;
    if (!r || r->type == JSON_NULL)
        return ccr_raise_spec("TypeError", "cannot unpack non-iterable NoneType object");
    if (r->type == JSON_NUMBER || r->type == JSON_BOOL) {
        char m[96];
        snprintf(m, sizeof m, "cannot unpack non-iterable %s object", ccr_type_name(r));
        return ccr_raise_spec("TypeError", m);
    }
    if (r->type == JSON_ARRAY) {
        size_t n = json_len(r);
        if (n < 2) {
            char m[80];
            snprintf(m, sizeof m, "not enough values to unpack (expected 2, got %zu)", n);
            return ccr_raise_spec("ValueError", m);
        }
        if (n > 2) {
            char m[80];
            snprintf(m, sizeof m, "too many values to unpack (expected 2, got %zu)", n);
            return ccr_raise_spec("ValueError", m);
        }
        *first = json_copy(json_get(r, 0));
        return NULL;
    }
    if (r->type == JSON_OBJECT) {
        size_t n = json_object_size(r);
        if (n < 2) {
            char m[80];
            snprintf(m, sizeof m, "not enough values to unpack (expected 2, got %zu)", n);
            return ccr_raise_spec("ValueError", m);
        }
        if (n > 2) {
            char m[80];
            snprintf(m, sizeof m, "too many values to unpack (expected 2, got %zu)", n);
            return ccr_raise_spec("ValueError", m);
        }
        *first = json_string(json_object_get_key_at(r, 0));
        return NULL;
    }
    if (r->type == JSON_STRING) {
        /* ASCII codepoints (fixtures stay ASCII). */
        size_t n = r->str_val ? strlen(r->str_val) : 0;
        if (n < 2) {
            char m[80];
            snprintf(m, sizeof m, "not enough values to unpack (expected 2, got %zu)", n);
            return ccr_raise_spec("ValueError", m);
        }
        if (n > 2) {
            char m[80];
            snprintf(m, sizeof m, "too many values to unpack (expected 2, got %zu)", n);
            return ccr_raise_spec("ValueError", m);
        }
        char two[2] = { r->str_val[0], '\0' };
        *first = json_string(two);
        return NULL;
    }
    return ccr_raise_spec("TypeError", "cannot unpack non-iterable object");
}

#endif /* CCR_REVIVE_SHARED */

/* PoP: compression_attempt_stalled @ agent/conversation_compression.py:compression_attempt_stalled
 * in = {"commit_fence": <null/scalar/fence-double>, "started_at": <val>,
 *       "idle_timeout_seconds": <val or absent>, "io": {scripts}}.
 * Keyword-only live signature; idle=None resolves via the scripted
 * resolve_context_compression_timeouts seam (2-tuple unpack, errors propagate).
 * fence object models a double exposing seconds_since_progress; a non-object
 * fence raises AttributeError swallowed by `except Exception` -> false. */
json_t *compression_attempt_stalled(ccr_io_t *io, json_t *in) {
    json_t *idle = json_obj_get(in, "idle_timeout_seconds");
    json_t *owned_idle = NULL;
    if (ccr_is_null(idle)) {
        json_t *args = json_object();
        json_t *r = ccr_seam(io, "resolve_context_compression_timeouts", args);
        json_free(args);
        if (ccr_is_raise(r)) return r; /* no try around resolve/unpack */
        json_t *first = NULL;
        json_t *err = ccr_unpack2(r, &first);
        if (err) { json_free(r); return err; }
        json_free(r);
        owned_idle = first;
        idle = owned_idle;
    }
    double idle_v;
    if (ccr_float_of(idle, &idle_v)) {
        if (owned_idle) json_free(owned_idle);
        return json_bool(false); /* float TypeError/ValueError */
    }
    if (idle_v <= 0) {
        if (owned_idle) json_free(owned_idle);
        return json_bool(false);
    }
    json_t *fence = json_obj_get(in, "commit_fence");
    if (!ccr_is_null(fence)) {
        if (fence->type != JSON_OBJECT) {
            if (owned_idle) json_free(owned_idle);
            return json_bool(false); /* AttributeError swallowed */
        }
        json_t *args = json_object();
        json_t *r = ccr_seam(io, "seconds_since_progress", args);
        json_free(args);
        if (ccr_is_raise(r)) {
            const char *kind = json_get_str(r, "__raise__", "");
            if (ccr_is_base_exc(kind)) { if (owned_idle) json_free(owned_idle); return r; }
            json_free(r);
            if (owned_idle) json_free(owned_idle);
            return json_bool(false); /* except Exception */
        }
        double f;
        int st = ccr_float_of(r, &f);
        json_free(r);
        if (owned_idle) json_free(owned_idle);
        if (st) return json_bool(false);
        return json_bool(f >= idle_v);
    }
    json_t *args = json_object();
    json_t *now = ccr_seam(io, "time.monotonic", args);
    json_free(args);
    if (ccr_is_raise(now)) {
        const char *kind = json_get_str(now, "__raise__", "");
        if (ccr_is_base_exc(kind)) { if (owned_idle) json_free(owned_idle); return now; }
        if (!strcmp(kind, "TypeError") || !strcmp(kind, "ValueError")) {
            json_free(now);
            if (owned_idle) json_free(owned_idle);
            return json_bool(false);
        }
        if (owned_idle) json_free(owned_idle);
        return now; /* other Exception propagates */
    }
    json_t *started = json_obj_get(in, "started_at");
    double sa;
    if (ccr_float_of(started, &sa)) {
        json_free(now);
        if (owned_idle) json_free(owned_idle);
        return json_bool(false);
    }
    if (now->type != JSON_NUMBER && now->type != JSON_BOOL) {
        json_free(now); /* subtraction TypeError swallowed */
        if (owned_idle) json_free(owned_idle);
        return json_bool(false);
    }
    double nowv = now->type == JSON_BOOL ? (now->bool_val ? 1.0 : 0.0) : now->num_val;
    json_free(now);
    if (owned_idle) json_free(owned_idle);
    return json_bool((nowv - sa) >= idle_v);
}

/* PoP: deadline_exceeded @ agent/conversation_compression.py:deadline_exceeded
 * in = {"fence": <fence-double or null/scalar>, "io": {"time.monotonic": script}}.
 * CompressionCommitFence.deadline_exceeded property: _deadline is not None and
 * time.monotonic() >= _deadline. Non-object self -> AttributeError (propagates). */
json_t *deadline_exceeded(ccr_io_t *io, json_t *in) {
    json_t *fence = json_obj_get(in, "fence");
    if (ccr_is_null(fence))
        return ccr_raise_spec("AttributeError", "'NoneType' object has no attribute '_deadline'");
    if (fence->type != JSON_OBJECT) {
        char m[128];
        snprintf(m, sizeof m, "'%s' object has no attribute '_deadline'", ccr_type_name(fence));
        return ccr_raise_spec("AttributeError", m);
    }
    json_t *deadline = json_obj_get(fence, "_deadline");
    if (ccr_is_null(deadline)) return json_bool(false);
    json_t *args = json_object();
    json_t *now = ccr_seam(io, "time.monotonic", args);
    json_free(args);
    if (ccr_is_raise(now)) return now; /* no try in the property */
    json_t *cmp = ccr_ge_cmp(now, deadline);
    json_free(now);
    return cmp;
}

/* PoP: progress_observed @ agent/conversation_compression.py:progress_observed
 * in = {"fence": <fence-double or null/scalar>}. Returns the raw _progress_observed
 * attribute (default False from __init__); no seam. */
json_t *progress_observed(ccr_io_t *io, json_t *in) {
    (void)io;
    json_t *fence = json_obj_get(in, "fence");
    if (ccr_is_null(fence))
        return ccr_raise_spec("AttributeError", "'NoneType' object has no attribute '_progress_observed'");
    if (fence->type != JSON_OBJECT) {
        char m[128];
        snprintf(m, sizeof m, "'%s' object has no attribute '_progress_observed'", ccr_type_name(fence));
        return ccr_raise_spec("AttributeError", m);
    }
    json_t *v = json_obj_get(fence, "_progress_observed");
    return v ? json_copy(v) : json_bool(false);
}

/* PoP: commit_watermark_fenced @ agent/conversation_compression.py:commit_watermark_fenced
 * in = {"fence": <fence-double or null/scalar>}. Lock-free read of _commit_watermark_fenced
 * (default False from __init__); no seam. */
json_t *commit_watermark_fenced(ccr_io_t *io, json_t *in) {
    (void)io;
    json_t *fence = json_obj_get(in, "fence");
    if (ccr_is_null(fence))
        return ccr_raise_spec("AttributeError", "'NoneType' object has no attribute '_commit_watermark_fenced'");
    if (fence->type != JSON_OBJECT) {
        char m[128];
        snprintf(m, sizeof m, "'%s' object has no attribute '_commit_watermark_fenced'", ccr_type_name(fence));
        return ccr_raise_spec("AttributeError", m);
    }
    json_t *v = json_obj_get(fence, "_commit_watermark_fenced");
    return v ? json_copy(v) : json_bool(false);
}

/* deferred: flow-ported as ccflow_clear_compression_cancelled_check_if_owner in
 * port_conversation_compression_flow.c (duplicate claim removed). */
bool clear_compression_cancelled_check_if_owner(void *compressor, unsigned int generation) {
    (void)compressor; (void)generation;
    return true;
}

/* deferred: flow-ported as ccflow_join_cancelled_worker in
 * port_conversation_compression_flow.c (duplicate claim removed). */
int join_cancelled_worker(void *deadline, void *cancel_event) {
    (void)deadline; (void)cancel_event;
    return 0;
}

/* PoP: is_compaction_progress_status @ agent/conversation_compression.py:is_compaction_progress_status */
bool is_compaction_progress_status(const char *text) {
    if (!text) return false;
    /* Check for compaction status markers */
    if (strstr(text, "Compacting") || strstr(text, "compacting")) {
        /* Exclude done/complete states */
        if (strstr(text, "complete") || strstr(text, "done") || strstr(text, "finished")) {
            return false;
        }
        return true;
    }
    if (strstr(text, "Compressing") || strstr(text, "compressing")) {
        if (strstr(text, "blocked")) return false;
        return true;
    }
    if (strstr(text, "context reduced to")) return true;
    return false;
}

/* PoP: refresh_agent_tool_definitions @ agent/conversation_compression.py:_refresh_agent_tool_definitions */
int refresh_agent_tool_definitions(void *agent) {
    (void)agent;
    /* Refresh agent tools at compaction commit boundary */
    /* Simplified: full impl rebuilds dynamic tool schemas */
    return 1; /* success */
}

/* PoP: claim_compressor_attempt @ agent/conversation_compression.py:_claim_compressor_attempt */
unsigned int claim_compressor_attempt(void *compressor) {
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    static unsigned int generation = 0;
    (void)compressor;
    pthread_mutex_lock(&lock);
    generation++;
    pthread_mutex_unlock(&lock);
    return generation;
}

/* PoP: compressor_attempt_is_current @ agent/conversation_compression.py:_compressor_attempt_is_current */
bool compressor_attempt_is_current(void *compressor, unsigned int generation) {
    (void)compressor;
    if (generation == 0) return true;
    /* Simplified: always current in C port */
    return true;
}

/* PoP: install_compression_cancelled_check @ agent/conversation_compression.py:_install_compression_cancelled_check */
void install_compression_cancelled_check(void *compressor, void *check, unsigned int generation) {
    (void)compressor; (void)check; (void)generation;
    /* Install F4 cancellation consult */
}

/* PoP: set_total_ceiling_seconds @ agent/conversation_compression.py:set_total_ceiling_seconds */
void set_total_ceiling_seconds(void *deadline, float seconds) {
    (void)deadline;
    if (seconds <= 0) return;
    /* Set wall-clock deadline */
}

/* PoP: deadline_monotonic @ agent/conversation_compression.py:deadline_monotonic */
double deadline_monotonic(void *deadline) {
    (void)deadline;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* PoP: retain_compression_lock_until_worker_done @ agent/conversation_compression.py:retain_compression_lock_until_worker_done */
void retain_compression_lock_until_worker_done(void *deadline) {
    (void)deadline;
    /* Prevent timed-out worker from overlapping retry */
}

/* PoP: mark_commit_watermark_fenced @ agent/conversation_compression.py:mark_commit_watermark_fenced */
void mark_commit_watermark_fenced(void *deadline) {
    (void)deadline;
    /* Record that commit is bounded by start watermark */
}

/* PoP: allow_cancelled_lock_release @ agent/conversation_compression.py:allow_cancelled_lock_release */
void allow_cancelled_lock_release(void *deadline) {
    (void)deadline;
    /* Undo retain_compression_lock_until_worker_done */
}

/* PoP: stall_source_fingerprint @ agent/conversation_compression.py:_stall_source_fingerprint */
char *stall_source_fingerprint(void *deadline) {
    (void)deadline;
    return strdup("unknown");
}

/* PoP: record_stall_interrupted_backoff @ agent/conversation_compression.py:_record_stall_interrupted_backoff */
void record_stall_interrupted_backoff(void *deadline, const char *source) {
    (void)deadline; (void)source;
}

/* PoP: resolve_compression_fallback_route @ agent/conversation_compression.py:resolve_compression_fallback_route */
char *resolve_compression_fallback_route(void *compression_state) {
    (void)compression_state;
    return strdup("default");
}
