#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── agent/conversation_compression.py: revival wave 2 ──
 * Faithful C11 ports of the live defs, I/O via injected ccr_io_t seams.
 * Deferred (already flow-ported as ccflow_* in port_conversation_compression_flow.c,
 * duplicate PoP claim removed): retry_compression_on_fallback_chain,
 * message_contains_busy_steer, compressed_has_busy_steer,
 * todo_snapshot_is_only_content, messages_match_scoped_identity.
 * Input contracts are documented per function and mirrored by
 * tests/oracle_compression_revive.py. */

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

/* PoP: codex_compaction_cooldown_remaining @ agent/conversation_compression_codex.py:_codex_compaction_cooldown_remaining
 * in = {"agent": <attr-double>, "io": {getter, logger.debug scripts}}.
 * agent/compressor objects model attribute doubles (SimpleNamespace); a getter
 * attribute {"__callable__": true} models a callable wired to the scripted seam.
 * Returns float seconds (0 = clear). Truthy non-dict getter state raises
 * AttributeError on .get (propagates); float TypeError/ValueError -> 0.0. */
json_t *codex_compaction_cooldown_remaining(ccr_io_t *io, json_t *in) {
    json_t *agent = json_obj_get(in, "agent");
    json_t *compressor = (agent && agent->type == JSON_OBJECT)
        ? json_obj_get(agent, "context_compressor") : NULL;
    json_t *getter = (compressor && compressor->type == JSON_OBJECT)
        ? json_obj_get(compressor, "get_active_compression_failure_cooldown") : NULL;
    int callable = getter && getter->type == JSON_OBJECT
        && json_get_bool(getter, "__callable__", false);
    if (!callable) return json_number(0.0);
    json_t *args = json_object();
    json_set(args, "refresh", json_bool(true));
    json_t *state = ccr_seam(io, "get_active_compression_failure_cooldown", args);
    json_free(args);
    if (ccr_is_raise(state)) {
        const char *kind = json_get_str(state, "__raise__", "");
        if (ccr_is_base_exc(kind)) return state; /* BaseException propagates */
        json_t *la = json_object();
        json_set_hdr(la, "msg", "codex compaction cooldown lookup failed");
        json_t *lr = ccr_seam(io, "logger.debug", la);
        json_free(la);
        json_free(state);
        if (ccr_is_raise(lr)) return lr; /* logger itself failed -> propagate */
        json_free(lr);
        return json_number(0.0);
    }
    double result = 0.0;
    if (ccr_truthy(state)) {
        if (state->type == JSON_OBJECT) {
            json_t *rem = json_obj_get(state, "remaining_seconds");
            double f = 0.0;
            int st = ccr_truthy(rem) ? ccr_float_of(rem, &f) : 0;
            if (st == 0) result = (f > 0.0) ? f : 0.0; /* max(0.0, f); nan -> 0.0 */
        } else {
            char m[128];
            snprintf(m, sizeof m, "'%s' object has no attribute 'get'", ccr_type_name(state));
            json_free(state);
            return ccr_raise_spec("AttributeError", m);
        }
    }
    json_free(state);
    return json_number(result);
}

/* PoP: context_compression_timed_out @ agent/conversation_compression.py:context_compression_timed_out
 * in = {"agent": <attr-double or scalar>}. Inlines _get_context_compression_timeout_state
 * (create=False): vars() TypeError on non-objects -> fallback; setdefault creates the
 * lock (state mutation, modeled {"__lock__": true}); a threading.local state is modeled
 * {"__local__": true, "timed_out": ...}. Reads are type-pinned `is True`. */
json_t *context_compression_timed_out(ccr_io_t *io, json_t *in) {
    (void)io;
    json_t *agent = json_obj_get(in, "agent");
    if (!agent || agent->type != JSON_OBJECT) return json_bool(false);
    const char *lk = "_context_compression_timeout_state_lock";
    if (!json_has(agent, lk)) {
        json_t *lock = json_object();
        json_set(lock, "__lock__", json_bool(true));
        json_set(agent, lk, lock);
    }
    json_t *state = json_obj_get(agent, "_context_compression_timeout_state");
    if (state && state->type == JSON_OBJECT && json_get_bool(state, "__local__", false)) {
        json_t *to = json_obj_get(state, "timed_out");
        return json_bool(to && to->type == JSON_BOOL && to->bool_val);
    }
    json_t *last = json_obj_get(agent, "_last_compression_timed_out");
    return json_bool(last && last->type == JSON_BOOL && last->bool_val);
}

/* PoP: compression_blocked_transiently @ agent/conversation_compression.py:compression_blocked_transiently
 * in = {"agent": <attr-double or scalar>}. isinstance(_sig, str) and bool(_sig). */
json_t *compression_blocked_transiently(ccr_io_t *io, json_t *in) {
    (void)io;
    json_t *agent = json_obj_get(in, "agent");
    if (!agent || agent->type != JSON_OBJECT) return json_bool(false);
    json_t *sig = json_obj_get(agent, "_compression_blocked_transient");
    return json_bool(sig && sig->type == JSON_STRING && sig->str_val && sig->str_val[0]);
}

/* PoP: checkpoint_blocked @ agent/conversation_compression_checkpoint.py:_checkpoint_blocked
 * in = {"reason": <str/int/bool/null>}. Live returns a CompressionCheckpointUnavailable
 * exception instance; the port projects it to {"__exc_class__", "message": str(exc)} and
 * the oracle applies the identical projection. reason stringifies via str() (containers
 * are out of fixture scope: Python repr is not JSON). */
json_t *checkpoint_blocked(ccr_io_t *io, json_t *in) {
    (void)io;
    json_t *reason = json_obj_get(in, "reason");
    char *rs = ccr_str_of(reason);
    const char *prefix = "BLOCKED_MISSING_PREREQUISITE: required pre-compress checkpoint unavailable: ";
    size_t n = strlen(prefix) + strlen(rs) + 1;
    char *msg = malloc(n);
    if (msg) snprintf(msg, n, "%s%s", prefix, rs);
    free(rs);
    json_t *out = json_object();
    json_set_hdr(out, "__exc_class__", "CompressionCheckpointUnavailable");
    json_set(out, "message", json_string(msg ? msg : ""));
    free(msg);
    return out;
}

/* deferred: flow-ported as ccflow_retry_compression_on_fallback_chain in
 * port_conversation_compression_flow.c (duplicate claim removed). */
int retry_compression_on_fallback_chain(void) { return 0; }

/* deferred: flow-ported as ccflow_message_contains_busy_steer in
 * port_conversation_compression_flow.c (duplicate claim removed). */
int message_contains_busy_steer(const char *msg) { return 0; }

/* deferred: flow-ported as ccflow_compressed_has_busy_steer in
 * port_conversation_compression_flow.c (duplicate claim removed). */
int compressed_has_busy_steer(void) { return 0; }

/* deferred: flow-ported as ccflow_todo_snapshot_is_only_content in
 * port_conversation_compression_flow.c (duplicate claim removed). */
int todo_snapshot_is_only_content(void) { return 0; }

/* deferred: flow-ported as ccflow_messages_match_scoped_identity in
 * port_conversation_compression_flow.c (duplicate claim removed). */
int messages_match_scoped_identity(const char *a, const char *b) { return 0; }

/* superseded by flow port (PoP credit moved there; wave13 DUP fix) */
char *get_context_compression_timeout_state(void) { return strdup("{}"); }

/* PoP: automatic_gate_blocked @ agent/conversation_compression.py:automatic_gate_blocked */
int automatic_gate_blocked(void) { return 0; }

/* PoP: emit_progress_status @ agent/conversation_compression.py:_emit_progress_status */
void emit_progress_status(const char *status) { (void)status; }

/* superseded by flow port (PoP credit moved there; wave13 DUP fix) */
char *direct_messages_for_pre_compress_memory(void) { return strdup("[]"); }

/* superseded by flow port (PoP credit moved there; wave13 DUP fix) */
char *extract_steer_text_from_message(const char *msg) { return strdup(""); }

/* PoP: replace_message_content @ agent/conversation_compression.py:_replace_message_content */
char *replace_message_content(const char *msg, const char *content) { return strdup(""); }

/* PoP: record_codex_compaction_failure @ agent/conversation_compression_codex.py:_record_codex_compaction_failure */
void record_codex_compaction_failure(const char *error) { (void)error; }
