/*
 * port_gw_session_store_flow.c — FLOW + leaf ports of
 * hermes_cli/gateway_session_store.py (GatewaySessionStore).
 * See include/port_gw_session_store_flow.h for the seam contract.
 *
 * Live decision chains are transcribed statement-for-statement: kwarg
 * evaluation order (uuid fires BEFORE the revision subscript), truthiness vs
 * `is None` identity, `or {}` detachment, retry-request freezing into the
 * shared prepared dict, generation fences, detach-on-refused-delete recovery,
 * redial/reattach before detach, and the `current is snapshot` identity clear.
 * Real (non-ported) collaborators — store.call, _retry_state, _leave and
 * SessionState.adopt_snapshot — run as internal helpers with NO seam event,
 * exactly as the oracle runs the live methods.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "port_gw_session_store_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

/* ── Python value semantics ── */

static bool gwss_truth(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0; /* NaN is truthy */
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}

static bool gwss_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

/* copy-or-None: json_copy(NULL) is NULL, but a missing key must emit null. */
static json_t *gwss_copy(const json_t *v) { return v ? json_copy(v) : json_null(); }

static const char *gwss_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER:
            return (v->num_val == (double)(long long)v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}

/* Python == over JSON values (bool/number cross-equality included). */
static bool gwss_eq(const json_t *a, const json_t *b) {
    bool an = gwss_is_null(a), bn = gwss_is_null(b);
    if (an || bn) return an && bn;
    bool anum = (a->type == JSON_NUMBER || a->type == JSON_BOOL);
    bool bnum = (b->type == JSON_NUMBER || b->type == JSON_BOOL);
    if (anum && bnum) {
        double av = a->type == JSON_BOOL ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = b->type == JSON_BOOL ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av == bv;
    }
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_STRING: return strcmp(a->str_val, b->str_val) == 0;
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!gwss_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !gwss_eq(a->c.items[i], bv)) return false;
            }
            return true;
        default: return false;
    }
}

/* str() of a scalar for f-strings (None/bool/int/str; float via shortest
 * round-trip %g — fixtures keep floats simple). Containers are excluded from
 * the formatted slots by the fixture contract. */
static void gwss_py_str(const json_t *v, char *buf, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, cap, "None"); return; }
    switch (v->type) {
        case JSON_BOOL: snprintf(buf, cap, "%s", v->bool_val ? "True" : "False"); return;
        case JSON_NUMBER:
            if (v->num_val == (double)(long long)v->num_val) {
                snprintf(buf, cap, "%.0f", v->num_val);
            } else {
                for (int prec = 1; prec <= 17; prec++) {
                    snprintf(buf, cap, "%.*g", prec, v->num_val);
                    if (strtod(buf, NULL) == v->num_val) break;
                }
            }
            return;
        case JSON_STRING: snprintf(buf, cap, "%s", v->str_val ? v->str_val : ""); return;
        default: snprintf(buf, cap, "<%s>", gwss_typename(v)); return;
    }
}

/* repr() of a string (KeyError keys, float() ValueError text). */
static char *gwss_py_repr(const char *s) {
    if (!s) s = "";
    bool has_sq = strchr(s, '\'') != NULL;
    bool has_dq = strchr(s, '"') != NULL;
    char quote = (has_sq && !has_dq) ? '"' : '\'';
    size_t n = strlen(s);
    char *out = (char *)malloc(n * 4 + 3);
    if (!out) return NULL;
    size_t o = 0;
    out[o++] = quote;
    for (size_t i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == (unsigned char)quote || ch == '\\') { out[o++] = '\\'; out[o++] = (char)ch; }
        else if (ch == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (ch == '\r') { out[o++] = '\\'; out[o++] = 'r'; }
        else if (ch == '\t') { out[o++] = '\\'; out[o++] = 't'; }
        else if (ch < 0x20 || ch == 0x7f) { o += (size_t)sprintf(out + o, "\\x%02x", ch); }
        else out[o++] = (char)ch; /* printable UTF-8 kept as-is, like repr */
    }
    out[o++] = quote;
    out[o] = '\0';
    return out;
}

/* ── raise specs ── */

static void gwss_raise(json_t **out, const char *cls, const char *msg) {
    if (!out || *out) return;
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "message", json_string(msg ? msg : ""));
    *out = r;
}

static void gwss_raise_fmt(json_t **out, const char *cls, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    gwss_raise(out, cls, buf);
}

/* KeyError specs carry the RAW key; str(KeyError(k)) == repr(k) at emit. */
static void gwss_raise_key(json_t **out, const char *key) {
    gwss_raise(out, "KeyError", key);
}

static bool gwss_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static const char *gwss_raise_kind(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}

/* except Exception does NOT catch these (asyncio.CancelledError included). */
static bool gwss_is_base_exc(const char *kind) {
    return strcmp(kind, "SystemExit") == 0 || strcmp(kind, "KeyboardInterrupt") == 0
        || strcmp(kind, "GeneratorExit") == 0 || strcmp(kind, "CancelledError") == 0;
}

/* Copy a seam raise spec into a probe/ret slot, reprs-ing KeyError once. */
static json_t *gwss_spec_emit(const json_t *spec) {
    json_t *r = json_copy(spec);
    if (strcmp(gwss_raise_kind(r), "KeyError") == 0) {
        char *rep = gwss_py_repr(json_get_str(r, "message", ""));
        json_set(r, "message", json_string(rep ? rep : ""));
        free(rep);
    }
    return r;
}

/* ── universal seam wrapper ── */

/* Scripted seam: args owned here (freed after the call); result caller-freed.
 * A raise spec result transfers into *raise_out. */
static json_t *gwss_seam(gwss_io_t *io, const char *name, json_t *args, json_t **raise_out) {
    json_t *r = io->call(io->ctx, name, args);
    json_free(args);
    if (gwss_is_raise(r)) { *raise_out = r; return NULL; }
    return r;
}

static json_t *gwss_seam_obj(gwss_io_t *io, const char *name, json_t **raise_out) {
    return gwss_seam(io, name, json_object(), raise_out);
}

/* ── state mirror accessors ── */

static json_t *gwss_mget(gwss_store_t *st, const char *key) {
    return json_obj_get(st->m, key);
}

static json_t *gwss_snapshots(gwss_store_t *st) { return gwss_mget(st, "snapshots"); }

static json_t *gwss_conn(gwss_store_t *st) { return gwss_mget(st, "connection"); }

static json_t *gwss_states(gwss_store_t *st) {
    json_t *c = gwss_conn(st);
    return c ? json_obj_get(c, "states") : NULL;
}

/* SessionState projection fields, in dump order (both sides identical). */
static const char *GWSS_STATE_FIELDS[] = {
    "session_id", "info", "title", "todo_state", "revision", "subscription_id",
    "execution_generation", "messages", "pending", "pending_seq", "gaps", "attached_gaps",
};

static json_t *gwss_state_new(const char *sid) {
    json_t *s = json_object();
    json_set(s, "session_id", json_string(sid));
    json_set(s, "info", json_object());
    json_set(s, "title", json_null());
    json_set(s, "todo_state", json_null());
    json_set(s, "revision", json_null());
    json_set(s, "subscription_id", json_null());
    json_set(s, "execution_generation", json_null());
    json_set(s, "messages", json_array());
    json_set(s, "pending", json_array());
    json_set(s, "pending_seq", json_number(0));
    json_set(s, "gaps", json_number(0));
    json_set(s, "attached_gaps", json_number(0));
    return s;
}

static json_t *gwss_state_proj(const json_t *s) {
    json_t *p = json_object();
    for (size_t i = 0; i < sizeof(GWSS_STATE_FIELDS) / sizeof(GWSS_STATE_FIELDS[0]); i++) {
        json_t *v = json_obj_get(s, GWSS_STATE_FIELDS[i]);
        json_set(p, GWSS_STATE_FIELDS[i], v ? json_copy(v) : json_null());
    }
    return p;
}

/* GatewayConnection.state(sid): get-or-create, records one seam event whose
 * ret is the projection BEFORE any adopt effects. Returns BORROWED state. */
static json_t *gwss_conn_state(gwss_store_t *st, gwss_io_t *io, const char *sid) {
    json_t *states = gwss_states(st);
    json_t *s = states ? json_obj_get(states, sid) : NULL;
    if (!s && states) { s = gwss_state_new(sid); json_set(states, sid, s); }
    json_t *args = json_object();
    json_set(args, "session_id", json_string(sid));
    json_t *r = io->call(io->ctx, "connection.state", args);
    json_free(args);
    /* The stub returns the same mirror node; prefer it, fall back to ours. */
    return r ? r : s;
}

/* SessionState.adopt_snapshot — real method both sides, silent state effect.
 * Fixture contract: info/messages/pending are dict/list/null (dict()/list()
 * of exotic scalars is out of scope). */
static void gwss_adopt_snapshot(json_t *s, const json_t *snap) {
    if (!s || s->type != JSON_OBJECT || !snap || snap->type != JSON_OBJECT) return;
    json_t *info = json_obj_get(snap, "info");
    json_set(s, "info", gwss_truth(info) ? json_copy(info) : json_object());
    json_t *v;
    v = json_obj_get(snap, "revision");
    if (v) json_set(s, "revision", json_copy(v));
    v = json_obj_get(snap, "subscription_id");
    if (v) json_set(s, "subscription_id", json_copy(v));
    v = json_obj_get(snap, "execution_generation");
    if (v) json_set(s, "execution_generation", json_copy(v));
    json_t *msgs = json_obj_get(snap, "messages");
    json_set(s, "messages", gwss_truth(msgs) ? json_copy(msgs) : json_array());
    if (json_has(snap, "todo_state")) {
        json_t *ts = json_obj_get(snap, "todo_state");
        json_set(s, "todo_state",
                 (ts && ts->type == JSON_OBJECT) ? json_copy(ts) : json_null());
    }
    json_t *pend = json_obj_get(snap, "pending");
    if (pend && pend->type == JSON_ARRAY) {
        json_set(s, "pending", json_copy(pend));
        json_t *seq = json_obj_get(s, "pending_seq");
        double n = (seq && seq->type == JSON_NUMBER) ? seq->num_val : 0.0;
        json_set(s, "pending_seq", json_number(n + 1));
    }
    json_t *sinfo = json_obj_get(s, "info");
    json_t *title = sinfo ? json_obj_get(sinfo, "title") : NULL;
    if (gwss_truth(title)) json_set(s, "title", json_copy(title));
}

/* ── real collaborators (no seam events; oracle runs the live methods) ── */

/* store.call(method, **params) -> connection.call(method, **params).
 * Builds the connection.call args; duplicate method/abandon kwargs raise
 * TypeError at the call site BEFORE any seam event, exactly as live binding. */
static json_t *gwss_call_args(const json_t *method, const json_t *abandon,
                              const json_t *params, json_t **raise_out) {
    if (params && params->type != JSON_OBJECT) {
        gwss_raise_fmt(raise_out, "TypeError",
                       "argument of type '%s' is not a mapping", gwss_typename(params));
        return NULL;
    }
    if (params && params->type == JSON_OBJECT && json_obj_get(params, "method")) {
        gwss_raise(raise_out, "TypeError", "call() got multiple values for argument 'method'");
        return NULL;
    }
    bool have_abandon = !gwss_is_null(abandon);
    if (have_abandon && params && params->type == JSON_OBJECT
        && json_obj_get(params, "abandon")) {
        gwss_raise(raise_out, "TypeError",
                   "call() got multiple values for keyword argument 'abandon'");
        return NULL;
    }
    json_t *args = json_object();
    json_set(args, "method", method ? json_copy(method) : json_null());
    if (have_abandon) json_set(args, "abandon", json_copy(abandon));
    if (params && params->type == JSON_OBJECT) {
        for (size_t i = 0; i < params->c.count; i++)
            json_set(args, params->c.keys[i], json_copy(params->c.items[i]));
    }
    return args;
}

static json_t *gwss_store_call(gwss_store_t *st, const json_t *method,
                               const json_t *params, gwss_io_t *io, json_t **raise_out) {
    (void)st;
    json_t *args = gwss_call_args(method, NULL, params, raise_out);
    if (!args) return NULL;
    return gwss_seam(io, "connection.call", args, raise_out);
}

/* store._call(method, abandon, **params): abandon is None -> self.call;
 * else connection.call(method, abandon=abandon, **params). `is None` is
 * identity, not truthiness (abandon=0/"" still takes the direct branch).
 * The caller's **params spread binds against _call's OWN signature first:
 * a non-mapping params, or a params carrying method/abandon, raises TypeError
 * at the call site (prefix "_call()") BEFORE the body routes anything. */
static json_t *gwss__call(gwss_store_t *st, const json_t *method, const json_t *abandon,
                          const json_t *params, gwss_io_t *io, json_t **raise_out) {
    if (params && params->type != JSON_OBJECT) {
        /* explicit None (or any non-mapping) spread raises; an ABSENT params
         * (NULL pointer) models the caller's default {} and spreads nothing. */
        gwss_raise_fmt(raise_out, "TypeError",
                       "argument of type '%s' is not a mapping", gwss_typename(params));
        return NULL;
    }
    if (params && params->type == JSON_OBJECT) {
        if (json_obj_get(params, "method")) {
            gwss_raise(raise_out, "TypeError", "_call() got multiple values for argument 'method'");
            return NULL;
        }
        if (json_obj_get(params, "abandon")) {
            gwss_raise(raise_out, "TypeError", "_call() got multiple values for argument 'abandon'");
            return NULL;
        }
    }
    if (gwss_is_null(abandon))
        return gwss_store_call(st, method, params, io, raise_out);
    json_t *args = gwss_call_args(method, abandon, params, raise_out);
    if (!args) return NULL;
    return gwss_seam(io, "connection.call", args, raise_out);
}

/* store._retry_state(): (None, None) when retry is None else the tuple.
 * Returns BORROWED prepared/abandon from the mirror (mutations of prepared
 * freeze into store.retry, as live). */
static void gwss_retry_state(gwss_store_t *st, json_t **prepared, json_t **abandon) {
    json_t *retry = gwss_mget(st, "retry");
    if (!retry || retry->type != JSON_ARRAY || retry->c.count < 2) {
        *prepared = NULL; *abandon = NULL;
        return;
    }
    *prepared = retry->c.items[0];
    *abandon = retry->c.items[1];
    if (*prepared && (*prepared)->type == JSON_NULL) *prepared = NULL;
    if (*abandon && (*abandon)->type == JSON_NULL) *abandon = NULL;
}

/* store._leave(sid): drop the subscription record, return its id. Silent. */
static json_t *gwss_leave(gwss_store_t *st, const char *sid) {
    json_t *states = gwss_states(st);
    json_t *s = states ? json_obj_get(states, sid) : NULL;
    if (!s || s->type != JSON_OBJECT) return NULL;
    json_t *sub = json_obj_get(s, "subscription_id");
    json_t *ret = sub ? json_copy(sub) : NULL;
    json_set(s, "subscription_id", json_null());
    return ret;
}

/* str(exc) for a raise spec: KeyError reprs its key, others use the message. */
static void gwss_exc_str(const json_t *spec, char *buf, size_t cap) {
    if (spec && strcmp(gwss_raise_kind(spec), "KeyError") == 0) {
        char *rep = gwss_py_repr(json_get_str(spec, "message", ""));
        snprintf(buf, cap, "%s", rep ? rep : "");
        free(rep);
        return;
    }
    gwss_py_str(spec ? json_obj_get(spec, "message") : NULL, buf, cap);
}

/* logger.debug sink: fmt + args verbatim (exception args projected to str). */
static void gwss_log_debug(gwss_io_t *io, const char *fmt, const json_t *exc_spec) {
    json_t *args = json_object();
    json_set(args, "fmt", json_string(fmt));
    json_t *arr = json_array();
    char buf[512];
    gwss_exc_str(exc_spec, buf, sizeof(buf));
    json_append(arr, json_string(buf));
    json_set(args, "args", arr);
    json_t *r = io->call(io->ctx, "logger.debug", args);
    json_free(args);
    json_free(r);
}/* ── float() coercion (str/int/float/bool; underscore neighbor-digit rule) ── */

static bool gwss_is_digit(char c) { return c >= '0' && c <= '9'; }

/* Validate the shape of a float() string argument (sign already allowed).
 * Returns true when strtod-able after underscore removal. */
static bool gwss_float_shape(const char *s) {
    /* every '_' must sit BETWEEN two digits (sign is not a digit) */
    for (const char *p = s; *p; p++) {
        if (*p == '_') {
            if (p == s || !gwss_is_digit(p[-1]) || !gwss_is_digit(p[1])) return false;
        }
    }
    const char *p = s;
    if (*p == '+' || *p == '-') p++;
    /* inf / infinity / nan (case-insensitive) */
    if ((*p == 'i' || *p == 'I') &&
        ((p[1] == 'n' || p[1] == 'N') && (p[2] == 'f' || p[2] == 'F'))) {
        const char *q = p + 3;
        if ((*q == 'i' || *q == 'I') && (q[1] == 'n' || q[1] == 'N') &&
            (q[2] == 'i' || q[2] == 'I') && (q[3] == 't' || q[3] == 'T') &&
            (q[4] == 'y' || q[4] == 'Y')) q += 5;
        return *q == '\0';
    }
    if ((*p == 'n' || *p == 'N') && (p[1] == 'a' || p[1] == 'A') &&
        (p[2] == 'n' || p[2] == 'N'))
        return p[3] == '\0';
    /* mantissa: digits [. digits] | . digits */
    bool any = false;
    while (gwss_is_digit(*p) || *p == '_') { if (gwss_is_digit(*p)) any = true; p++; }
    if (*p == '.') {
        p++;
        while (gwss_is_digit(*p) || *p == '_') { if (gwss_is_digit(*p)) any = true; p++; }
    }
    if (!any) return false;
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        bool ed = false;
        while (gwss_is_digit(*p) || *p == '_') { if (gwss_is_digit(*p)) ed = true; p++; }
        if (!ed) return false;
    }
    return *p == '\0';
}

/* float(v) -> JSON_NUMBER, or a raise spec. Mirrors CPython float():
 * strings are whitespace-stripped, underscores validated, then strtod;
 * the ValueError text reprs the ORIGINAL argument. */
static json_t *gwss_float_coerce(const json_t *v, json_t **raise_out) {
    if (!v || v->type == JSON_NULL) {
        gwss_raise(raise_out, "TypeError",
                   "float() argument must be a string or a real number, not 'NoneType'");
        return NULL;
    }
    if (v->type == JSON_BOOL) return json_number(v->bool_val ? 1.0 : 0.0);
    if (v->type == JSON_NUMBER) return json_number(v->num_val);
    if (v->type != JSON_STRING) {
        gwss_raise_fmt(raise_out, "TypeError",
                       "float() argument must be a string or a real number, not '%s'",
                       gwss_typename(v));
        return NULL;
    }
    const char *s = v->str_val ? v->str_val : "";
    /* strip ASCII whitespace both ends (fixture contract: ASCII) */
    const char *a = s;
    while (*a == ' ' || *a == '\t' || *a == '\n' || *a == '\r' || *a == '\v' || *a == '\f') a++;
    const char *b = s + strlen(s);
    while (b > a && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\n' || b[-1] == '\r'
                     || b[-1] == '\v' || b[-1] == '\f')) b--;
    size_t n = (size_t)(b - a);
    char *tmp = (char *)malloc(n + 1);
    if (!tmp) return NULL;
    memcpy(tmp, a, n);
    tmp[n] = '\0';
    bool ok = gwss_float_shape(tmp);
    double val = 0.0;
    if (ok) {
        /* remove underscores (validated between digits) */
        char *w = tmp;
        for (char *r = tmp; *r; r++) if (*r != '_') *w++ = *r;
        *w = '\0';
        val = strtod(tmp, NULL);
    }
    free(tmp);
    if (!ok) {
        char *rep = gwss_py_repr(s);
        gwss_raise_fmt(raise_out, "ValueError", "could not convert string to float: %s",
                       rep ? rep : "''");
        free(rep);
        return NULL;
    }
    return json_number(val);
}

/* ── module constants (seeded from the live module) ── */

static const char *GWSS_READS[] = {
    "get_session", "get_session_title", "get_session_by_title", "resolve_resume_session_id",
    "get_messages_as_conversation", "get_resume_conversations", "assert_resume_safe",
    "is_kanban_owned_session", "list_sessions_rich", "search_sessions", "get_next_title_in_lineage",
    "get_handoff_state", "get_meta", "get_compression_tip", "resolve_session_by_title", "get_messages",
    "session_count", "sanitize_title", "session_yolo_enabled", "get_session_model_config_value",
};
static const char *GWSS_GATEWAY_OWNED[] = {
    "create_session", "reopen_session", "append_messages_batch", "flush_token_counts",
    "update_session_model", "patch_session_model_config", "maybe_auto_archive",
    "maybe_auto_prune_and_vacuum", "set_meta", "update_token_counts", "close",
};
static const char *GWSS_GENERATION_FENCED[] = {
    "delete", "rewind", "reset", "branch", "model", "compress", "workspace",
};
static const char *GWSS_COUNTED_REPAIRS[] = {
    "prune_empty_ghost_sessions", "finalize_orphaned_compression_sessions",
};
static const char *GWSS_RESUME_REFUSALS[] = { "not_found", "permission_denied" };

static bool gwss_in_list(const char *name, const char **list, size_t n) {
    for (size_t i = 0; i < n; i++) if (strcmp(name, list[i]) == 0) return true;
    return false;
}

/* ── leaves ── */

/* PoP: gwss_session_label @ hermes_cli/gateway_session_store.py:session_label */
json_t *gwss_session_label(const json_t *session_id, json_t **raise_out) {
    /* _CANONICAL_LOCAL_ID = ^local-([0-9a-f]{8})[0-9a-f]{56}$ */
    const json_t *arg = session_id;
    if (!gwss_truth(arg)) {
        /* match(session_id or "") -> "" never matches; return the ORIGINAL */
        return arg ? json_copy(arg) : NULL;
    }
    if (arg->type != JSON_STRING) {
        gwss_raise_fmt(raise_out, "TypeError",
                       "expected string or bytes-like object, got '%s'", gwss_typename(arg));
        return NULL;
    }
    const char *s = arg->str_val;
    size_t n = strlen(s);
    bool match = (n == 70) && strncmp(s, "local-", 6) == 0;
    if (match) {
        for (size_t i = 6; i < 70; i++) {
            char c = s[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) { match = false; break; }
        }
    }
    if (match) {
        char grp[9];
        memcpy(grp, s + 6, 8);
        grp[8] = '\0';
        return json_string(grp);
    }
    return json_copy(arg);
}

/* PoP: gwss_classic_only_policy @ hermes_cli/gateway_session_store.py:_classic_only_policy */
json_t *gwss_classic_only_policy(const json_t *launch, json_t **raise_out) {
    if (!launch || launch->type != JSON_OBJECT) {
        gwss_raise_fmt(raise_out, "AttributeError", "'%s' object has no attribute 'get'",
                       gwss_typename(launch));
        return NULL;
    }
    json_t *policy = json_object();
    json_t *rb = json_obj_get(launch, "run_budget");
    if (rb && rb->type != JSON_NULL) { /* is not None — 0/"" still coerce */
        json_t *f = gwss_float_coerce(rb, raise_out);
        if (!f) { json_free(policy); return NULL; }
        json_set(policy, "run_budget", f);
    }
    if (gwss_truth(json_obj_get(launch, "verbose")))
        json_set(policy, "verbose", json_bool(true));
    return policy;
}

/* PoP: gwss_transcript_view @ hermes_cli/gateway_session_store.py:_transcript_view */
json_t *gwss_transcript_view(const json_t *include_inactive,
                             const json_t *include_compacted, json_t **raise_out) {
    (void)raise_out;
    /* TranscriptView members projected to their .value (wire form). */
    if (gwss_truth(include_inactive)) return json_string("all");
    return json_string(gwss_truth(include_compacted) ? "display" : "active");
}

/* PoP: gwss_window @ hermes_cli/gateway_session_store.py:_window */
json_t *gwss_window(const json_t *limit, const json_t *offset, json_t **raise_out) {
    /* SessionWindow(limit=None if limit is not None and limit < 0 else limit,
     *               offset=offset); __post_init__ fences offset < 0. */
    json_t *lim = NULL;
    bool lim_owned = false;
    if (!gwss_is_null(limit)) {
        if (limit->type != JSON_NUMBER && limit->type != JSON_BOOL) {
            gwss_raise_fmt(raise_out, "TypeError",
                           "'<' not supported between instances of '%s' and 'int'",
                           gwss_typename(limit));
            return NULL;
        }
        double lv = limit->type == JSON_BOOL ? (limit->bool_val ? 1.0 : 0.0) : limit->num_val;
        if (lv < 0) { lim = json_null(); lim_owned = true; }
    }
    if (!lim) { lim = limit ? json_copy(limit) : json_null(); lim_owned = true; }
    /* __post_init__: offset < 0 (limit is already >= 0 or None) */
    if (gwss_is_null(offset) || (offset->type != JSON_NUMBER && offset->type != JSON_BOOL)) {
        gwss_raise_fmt(raise_out, "TypeError",
                       "'<' not supported between instances of '%s' and 'int'",
                       gwss_typename(offset));
        json_free(lim);
        return NULL;
    }
    double ov = offset->type == JSON_BOOL ? (offset->bool_val ? 1.0 : 0.0) : offset->num_val;
    if (ov < 0) {
        char lb[64], ob[64];
        gwss_py_str(lim, lb, sizeof(lb));
        gwss_py_str(offset, ob, sizeof(ob));
        gwss_raise_fmt(raise_out, "ValueError",
                       "SessionWindow needs limit >= 0 and offset >= 0, got %s, %s", lb, ob);
        json_free(lim);
        return NULL;
    }
    json_t *w = json_object();
    json_set(w, "limit", lim);
    json_set(w, "offset", json_copy(offset));
    (void)lim_owned;
    return w;
}

/* ── small store flows ── */

/* PoP: gwss_set_session_yolo @ hermes_cli/gateway_session_store.py:set_session_yolo */
json_t *gwss_set_session_yolo(gwss_store_t *st, const json_t *in, gwss_io_t *io, json_t **raise_out) {
    json_t *params = json_object();
    json_set(params, "session_id", gwss_copy(json_obj_get(in, "session_id")));
    json_set(params, "key", json_string("yolo"));
    json_set(params, "value", json_string(gwss_truth(json_obj_get(in, "enabled")) ? "1" : "0"));
    json_t *method = json_string("config.set");
    json_t *r = gwss_store_call(st, method, params, io, raise_out);
    json_free(method);
    json_free(params);
    json_free(r); /* returns None */
    return NULL;
}

/* PoP: gwss_close_reader @ hermes_cli/gateway_session_store.py:close_reader */
json_t *gwss_close_reader(gwss_store_t *st, const json_t *in, gwss_io_t *io, json_t **raise_out) {
    (void)in;
    json_t *handles = gwss_mget(st, "handles");
    if (!gwss_is_null(handles)) {
        json_t *r = gwss_seam_obj(io, "handles.close", raise_out);
        if (*raise_out) return NULL; /* close() raised: nothing cleared */
        json_free(r);
        json_set(st->m, "handles", json_null());
        json_set(st->m, "reader", json_null());
        json_set(st->m, "persistence", json_null());
    }
    return NULL;
}/* ── __getattr__: the read/write dispatch decision chain ── */

/* Probe-call outcome helpers: the port resolves the callable __getattr__
 * returns, then invokes it with the case's probe args to observe behavior
 * (the oracle does the identical probe). */
static json_t *gwss_probe_result(const char *kind, json_t *probe_ret, json_t *probe_raise) {
    json_t *r = json_object();
    json_set(r, "kind", json_string(kind));
    json_set(r, "probe_ret", probe_ret ? probe_ret : json_null());
    json_set(r, "probe_raise", probe_raise ? probe_raise : json_null());
    return r;
}

/* PoP: gwss_getattr @ hermes_cli/gateway_session_store.py:__getattr__ */
json_t *gwss_getattr(gwss_store_t *st, const json_t *in, gwss_io_t *io, json_t **raise_out) {
    const char *name = json_get_str(in, "name", "");
    const json_t *probe = json_obj_get(in, "probe");
    const json_t *pargs = probe ? json_obj_get(probe, "args") : NULL;
    const json_t *pkwargs = probe ? json_obj_get(probe, "kwargs") : NULL;

    /* if name.startswith("__"): raise AttributeError(name) */
    if (strncmp(name, "__", 2) == 0) {
        json_t *spec = json_object();
        json_set(spec, "__raise__", json_string("AttributeError"));
        json_set(spec, "message", json_string(name));
        return gwss_probe_result("attr_error", NULL, spec);
    }

    const char *kind;
    if (gwss_in_list(name, GWSS_READS, sizeof(GWSS_READS) / sizeof(GWSS_READS[0]))) {
        if (strcmp(name, "sanitize_title") == 0) kind = "sanitize_title";
        else if (strcmp(name, "session_yolo_enabled") == 0) kind = "yolo";
        else {
            json_t *db = gwss_seam_obj(io, "_db", raise_out);
            if (*raise_out) return NULL;
            bool db_none = gwss_is_null(db);
            json_free(db);
            kind = db_none ? "noop" : "read";
        }
    } else if (gwss_in_list(name, GWSS_GATEWAY_OWNED,
                            sizeof(GWSS_GATEWAY_OWNED) / sizeof(GWSS_GATEWAY_OWNED[0]))) {
        kind = "noop";
    } else if (gwss_in_list(name, GWSS_COUNTED_REPAIRS,
                            sizeof(GWSS_COUNTED_REPAIRS) / sizeof(GWSS_COUNTED_REPAIRS[0]))) {
        kind = "zero";
    } else {
        kind = "refused";
    }

    /* probe invocation */
    if (strcmp(kind, "noop") == 0) return gwss_probe_result(kind, NULL, NULL);
    if (strcmp(kind, "zero") == 0) return gwss_probe_result(kind, json_number(0), NULL);
    if (strcmp(kind, "refused") == 0) {
        json_t *spec = json_object();
        json_set(spec, "__raise__", json_string("GatewayClientError"));
        char msg[512];
        snprintf(msg, sizeof(msg), "%s is not on the gateway session yet; nothing was written", name);
        json_set(spec, "message", json_string(msg));
        return gwss_probe_result(kind, NULL, spec);
    }
    if (strcmp(kind, "sanitize_title") == 0 || strcmp(kind, "read") == 0) {
        char seam[256];
        if (strcmp(kind, "read") == 0) snprintf(seam, sizeof(seam), "reader.%s", name);
        else snprintf(seam, sizeof(seam), "sanitize_title");
        json_t *args = json_object();
        json_set(args, "args", pargs ? json_copy(pargs) : json_array());
        json_set(args, "kwargs", pkwargs ? json_copy(pkwargs) : json_object());
        json_t *r = gwss_seam(io, seam, args, raise_out);
        if (*raise_out) {
            json_t *spec = gwss_spec_emit(*raise_out);
            json_free(*raise_out);
            *raise_out = NULL;
            return gwss_probe_result(kind, NULL, spec);
        }
        return gwss_probe_result(kind, r, NULL);
    }
    /* yolo lambda: session_yolo_enabled((session_meta or {}).get("model_config")) */
    size_t nargs = (pargs && pargs->type == JSON_ARRAY) ? pargs->c.count : 0;
    if (nargs < 1) {
        json_t *spec = json_object();
        json_set(spec, "__raise__", json_string("TypeError"));
        json_set(spec, "message",
                 json_string("<lambda>() missing 1 required positional argument: 'session_meta'"));
        return gwss_probe_result(kind, NULL, spec);
    }
    const json_t *meta = pargs->c.items[0];
    const json_t *base = gwss_truth(meta) ? meta : NULL;
    if (base && base->type != JSON_OBJECT) {
        json_t *spec = json_object();
        json_set(spec, "__raise__", json_string("AttributeError"));
        char msg[256];
        snprintf(msg, sizeof(msg), "'%s' object has no attribute 'get'", gwss_typename(base));
        json_set(spec, "message", json_string(msg));
        return gwss_probe_result(kind, NULL, spec);
    }
    json_t *mc = base ? json_obj_get(base, "model_config") : NULL;
    json_t *args = json_object();
    json_t *arr = json_array();
    json_append(arr, gwss_copy(mc));
    json_set(args, "args", arr);
    json_set(args, "kwargs", json_object());
    json_t *r = gwss_seam(io, "session_yolo_enabled", args, raise_out);
    if (*raise_out) {
        json_t *spec = gwss_spec_emit(*raise_out);
        json_free(*raise_out);
        *raise_out = NULL;
        return gwss_probe_result(kind, NULL, spec);
    }
    return gwss_probe_result(kind, r, NULL);
}

/* ── _call leaf (the abandon routing decision) ── */

/* PoP: gwss_call_flow @ hermes_cli/gateway_session_store.py:_call */
json_t *gwss_call_flow(gwss_store_t *st, const json_t *in, gwss_io_t *io, json_t **raise_out) {
    const json_t *method = json_obj_get(in, "method");
    const json_t *abandon = json_obj_get(in, "abandon");
    const json_t *params = json_obj_get(in, "params");
    return gwss__call(st, method, abandon, params, io, raise_out);
}

/* ── _adopt: snapshot replacement ordering ── */

/* PoP: gwss_adopt @ hermes_cli/gateway_session_store.py:_adopt */
json_t *gwss_adopt(gwss_store_t *st, const json_t *snapshot, gwss_io_t *io, json_t **raise_out) {
    if (!snapshot || snapshot->type != JSON_OBJECT) {
        if (gwss_is_null(snapshot))
            gwss_raise(raise_out, "TypeError", "'NoneType' object is not subscriptable");
        else
            gwss_raise_fmt(raise_out, "TypeError", "'%s' object is not subscriptable",
                           gwss_typename(snapshot));
        return NULL;
    }
    json_t *sidn = json_obj_get(snapshot, "stored_session_id");
    if (!sidn) { gwss_raise_key(raise_out, "stored_session_id"); return NULL; }
    char sid[256];
    gwss_py_str(sidn, sid, sizeof(sid));
    /* snapshots[sid] = snapshot; current = snapshot (same object live) */
    json_t *snaps = gwss_snapshots(st);
    if (snaps) json_set(snaps, sid, json_copy(snapshot));
    json_set(st->m, "current", json_copy(snapshot));
    free(st->current_ref);
    st->current_ref = strdup(sid);
    /* connection.state(sid).adopt_snapshot(snapshot) — silent state effect */
    json_t *state = gwss_conn_state(st, io, sid);
    gwss_adopt_snapshot(state, snapshot);
    return json_string(sid);
}

/* ── attach: idempotent resume ── */

/* PoP: gwss_attach @ hermes_cli/gateway_session_store.py:attach */
json_t *gwss_attach(gwss_store_t *st, const json_t *in, gwss_io_t *io, json_t **raise_out) {
    const char *sid = json_get_str(in, "session_id", "");
    bool typed = json_get_bool(in, "typed", false);
    json_t *snaps = gwss_snapshots(st);
    json_t *cached = snaps ? json_obj_get(snaps, sid) : NULL;
    if (cached) return json_copy(cached);
    /* connection.resume(self.call, session_id, typed=typed) */
    json_t *args = json_object();
    json_set(args, "call", json_string("store.call"));
    json_set(args, "session_id", json_string(sid));
    json_set(args, "typed", json_bool(typed));
    json_t *snapshot = gwss_seam(io, "connection.resume", args, raise_out);
    if (*raise_out) return NULL;
    json_t *r = gwss_adopt(st, snapshot, io, raise_out);
    json_free(r); /* adopt's sid return is discarded; attach returns snapshot */
    if (*raise_out) { json_free(snapshot); return NULL; }
    return snapshot;
}/* ── create: retry-frozen session.create ── */

/* PoP: gwss_create @ hermes_cli/gateway_session_store.py:create */
json_t *gwss_create(gwss_store_t *st, const json_t *in, gwss_io_t *io, json_t **raise_out) {
    const json_t *launch = json_obj_get(in, "launch");
    const json_t *title = json_obj_get(in, "title");
    bool record_cwd = json_get_bool(in, "record_cwd", true);
    json_t *prepared, *abandon;
    gwss_retry_state(st, &prepared, &abandon);
    /* params = prepared.get("params") if prepared is not None else None */
    json_t *params = prepared ? json_obj_get(prepared, "params") : NULL; /* borrowed */
    bool params_owned = false;
    if (gwss_is_null(params)) {
        json_t *args = json_object();
        json_set(args, "launch", gwss_copy(launch));
        json_set(args, "title", gwss_copy(title));
        json_set(args, "record_cwd", json_bool(record_cwd));
        params = gwss_seam(io, "_create_params", args, raise_out);
        if (*raise_out) return NULL;
        params_owned = true;
        if (prepared) json_set(prepared, "params", json_copy(params)); /* freeze */
    }
    json_t *method = json_string("session.create");
    json_t *snapshot = gwss__call(st, method, abandon, params, io, raise_out);
    json_free(method);
    if (params_owned) json_free(params);
    if (*raise_out) return NULL;
    /* warning = (snapshot.get("info") or {}).get("config_warning") */
    if (!snapshot || snapshot->type != JSON_OBJECT) {
        gwss_raise_fmt(raise_out, "AttributeError", "'%s' object has no attribute 'get'",
                       gwss_typename(snapshot));
        json_free(snapshot);
        return NULL;
    }
    json_t *info = json_obj_get(snapshot, "info");
    json_t *wbase = gwss_truth(info) ? info : NULL;
    if (wbase && wbase->type != JSON_OBJECT) {
        gwss_raise_fmt(raise_out, "AttributeError", "'%s' object has no attribute 'get'",
                       gwss_typename(wbase));
        json_free(snapshot);
        return NULL;
    }
    json_t *warning = wbase ? json_obj_get(wbase, "config_warning") : NULL;
    if (gwss_truth(warning)) {
        char wstr[512], text[600];
        gwss_py_str(warning, wstr, sizeof(wstr));
        snprintf(text, sizeof(text), "Warning: %s", wstr);
        json_t *pargs = json_object();
        json_set(pargs, "text", json_string(text));
        json_set(pargs, "file", json_string("stderr"));
        json_t *pr = io->call(io->ctx, "print", pargs);
        json_free(pargs);
        json_free(pr);
    }
    json_t *r = gwss_adopt(st, snapshot, io, raise_out);
    json_free(snapshot);
    return r;
}

/* ── branch: retry-frozen generation-fenced fork ── */

/* PoP: gwss_branch @ hermes_cli/gateway_session_store.py:branch */
json_t *gwss_branch(gwss_store_t *st, const json_t *in, gwss_io_t *io, json_t **raise_out) {
    const char *sid = json_get_str(in, "session_id", "");
    const json_t *title = json_obj_get(in, "title");
    json_t *prepared0, *abandon;
    gwss_retry_state(st, &prepared0, &abandon);
    json_t *prepared = prepared0;
    bool prepared_owned = false;
    if (!prepared) { prepared = json_object(); prepared_owned = true; }

    json_t *ret = NULL;
    json_t *snapshot = NULL;   /* resume snapshot (owned until mutate built) */
    json_t *mutate_local = NULL; /* owned until frozen into prepared */
    json_t *receipt = NULL;
    json_t *snap = NULL;

    json_t *child = json_obj_get(prepared, "child"); /* borrowed */
    if (gwss_is_null(child)) {
        json_t *mutate = json_obj_get(prepared, "mutate"); /* borrowed */
        if (gwss_is_null(mutate)) {
            json_t *rparams = json_object();
            json_set(rparams, "session_id", json_string(sid));
            json_t *method = json_string("session.resume");
            snapshot = gwss__call(st, method, abandon, rparams, io, raise_out);
            json_free(method);
            json_free(rparams);
            if (*raise_out) goto done;
            /* dict literal order: session_id, request_id(uuid), revision,
             * generation, operation, payload — uuid fires BEFORE revision. */
            mutate_local = json_object();
            json_set(mutate_local, "session_id", json_string(sid));
            json_t *hex = gwss_seam_obj(io, "uuid4", raise_out);
            if (*raise_out) goto done;
            json_set(mutate_local, "request_id", hex ? hex : json_null());
            if (!snapshot || snapshot->type != JSON_OBJECT) {
                gwss_raise_fmt(raise_out, "TypeError", "'%s' object is not subscriptable",
                               gwss_typename(snapshot));
                goto done;
            }
            json_t *rev = json_obj_get(snapshot, "revision");
            if (!rev) { gwss_raise_key(raise_out, "revision"); goto done; }
            json_set(mutate_local, "expected_revision", json_copy(rev));
            json_t *gen = json_obj_get(snapshot, "execution_generation");
            if (!gen) { gwss_raise_key(raise_out, "execution_generation"); goto done; }
            json_set(mutate_local, "expected_generation", json_copy(gen));
            json_set(mutate_local, "operation", json_string("branch"));
            json_t *payload = json_object();
            if (gwss_truth(title)) json_set(payload, "title", json_copy(title));
            json_set(mutate_local, "payload", payload);
            json_free(snapshot);
            snapshot = NULL;
            /* prepared["mutate"] = mutate; local mutate IS that object */
            json_set(prepared, "mutate", mutate_local);
            mutate_local = NULL;
            mutate = json_obj_get(prepared, "mutate");
        }
        json_t *method2 = json_string("session.mutate");
        receipt = gwss__call(st, method2, abandon, mutate, io, raise_out);
        json_free(method2);
        if (*raise_out) goto done;
        if (!gwss_truth(receipt)) { json_free(receipt); receipt = json_object(); }
        if (receipt->type != JSON_OBJECT) {
            gwss_raise_fmt(raise_out, "AttributeError", "'%s' object has no attribute 'get'",
                           gwss_typename(receipt));
            goto done;
        }
        child = json_obj_get(receipt, "branched_session_id"); /* borrowed */
        if (!gwss_truth(child)) {
            gwss_raise(raise_out, "GatewayClientError", "branch_failed");
            goto done;
        }
        json_set(prepared, "child", json_copy(child));
        json_free(receipt);
        receipt = NULL;
        child = json_obj_get(prepared, "child"); /* borrow the frozen copy */
    }
    /* self._adopt(self.attach(child)) */
    {
        char child_str[256];
        gwss_py_str(child, child_str, sizeof(child_str));
        json_t *ain = json_object();
        json_set(ain, "session_id", json_string(child_str));
        json_set(ain, "typed", json_bool(false));
        snap = gwss_attach(st, ain, io, raise_out);
        json_free(ain);
        if (*raise_out) goto done;
        json_t *r = gwss_adopt(st, snap, io, raise_out);
        json_free(r);
        json_free(snap);
        snap = NULL;
        if (*raise_out) goto done;
        ret = json_copy(child);
    }
done:
    json_free(snapshot);
    json_free(mutate_local);
    json_free(receipt);
    json_free(snap);
    if (prepared_owned) json_free(prepared);
    return ret;
}

/* ── open_session: the resume/create decision chain ── */

/* PoP: gwss_open_session @ hermes_cli/gateway_session_store.py:open_session */
json_t *gwss_open_session(gwss_store_t *st, const json_t *in, gwss_io_t *io, json_t **raise_out) {
    const json_t *cli = json_obj_get(in, "cli");
    const json_t *launch = json_obj_get(in, "launch");
    json_t *conn = gwss_conn(st);
    json_t *ret = NULL;
    json_t *snapshot = NULL;

    /* key = conn.launch_api_key = (launch or {}).get("api_key") or None */
    json_t *base = gwss_truth(launch) ? json_copy(launch) : json_object();
    if (base->type != JSON_OBJECT) {
        gwss_raise_fmt(raise_out, "AttributeError", "'%s' object has no attribute 'get'",
                       gwss_typename(base));
        json_free(base);
        return NULL;
    }
    json_t *ak = json_obj_get(base, "api_key");
    json_t *key = gwss_truth(ak) ? json_copy(ak) : json_null();
    if (conn) json_set(conn, "launch_api_key", json_copy(key));
    /* typed = key is not None and key == getattr(cli, "_typed_api_key", None) */
    bool typed = false;
    if (!gwss_is_null(key)) {
        json_t *tk = cli ? json_obj_get(cli, "_typed_api_key") : NULL;
        typed = gwss_eq(key, tk);
    }
    if (conn) json_set(conn, "launch_api_key_typed", json_bool(typed));

    json_t *resumed = cli ? json_obj_get(cli, "_resumed") : NULL;
    if (gwss_truth(resumed)) {
        json_t *csid = cli ? json_obj_get(cli, "session_id") : NULL;
        if (!csid) {
            gwss_raise(raise_out, "AttributeError",
                       "'SimpleNamespace' object has no attribute 'session_id'");
            goto done;
        }
        json_t *ain = json_object();
        json_set(ain, "session_id", json_copy(csid));
        json_set(ain, "typed", json_bool(typed));
        snapshot = gwss_attach(st, ain, io, raise_out);
        json_free(ain);
        if (*raise_out) {
            /* except GatewayClientError: str(exc) in RESUME_REFUSALS -> None */
            if (strcmp(gwss_raise_kind(*raise_out), "GatewayClientError") == 0) {
                const char *msg = json_get_str(*raise_out, "message", "");
                if (gwss_in_list(msg, GWSS_RESUME_REFUSALS,
                                 sizeof(GWSS_RESUME_REFUSALS) / sizeof(GWSS_RESUME_REFUSALS[0]))) {
                    json_free(*raise_out);
                    *raise_out = NULL;
                    goto done; /* return None */
                }
            }
            goto done; /* re-raise */
        }
        /* _check_resume_launch({**(launch or {}), toolsets, ignore_rules}, snapshot) */
        json_t *merged = json_copy(base);
        json_set(merged, "toolsets", gwss_copy(cli ? json_obj_get(cli, "_typed_toolsets") : NULL));
        json_t *ir = cli ? json_obj_get(cli, "_explicit_ignore_rules") : NULL;
        json_set(merged, "ignore_rules", gwss_truth(ir) ? json_copy(ir) : json_null());
        json_t *cargs = json_object();
        json_set(cargs, "launch", merged);
        json_set(cargs, "snapshot", json_copy(snapshot));
        json_t *cr = gwss_seam(io, "_check_resume_launch", cargs, raise_out);
        json_free(cr);
        if (*raise_out) goto done;
        ret = gwss_adopt(st, snapshot, io, raise_out);
        goto done;
    }
    /* return self.create(launch) */
    {
        json_t *cin = json_object();
        json_set(cin, "launch", gwss_copy(launch));
        ret = gwss_create(st, cin, io, raise_out);
        json_free(cin);
    }
done:
    json_free(base);
    json_free(key);
    json_free(snapshot);
    return ret;
}/* ── _mutate: generation fence + kwarg evaluation order ── */

/* PoP: gwss_mutate @ hermes_cli/gateway_session_store.py:_mutate */
json_t *gwss_mutate(gwss_store_t *st, const json_t *session_id, const json_t *operation,
                    const json_t *payload, const json_t *snapshot, gwss_io_t *io, json_t **raise_out) {
    json_t *snap = NULL;
    bool snap_owned = false;
    json_t *ret = NULL;
    json_t *fence = json_object();
    json_t *params = NULL;

    /* snapshot = snapshot or self.call("session.resume", session_id=session_id) */
    if (gwss_truth(snapshot)) {
        snap = (json_t *)snapshot; /* borrowed */
    } else {
        json_t *rparams = json_object();
        json_set(rparams, "session_id", gwss_copy(session_id));
        json_t *method = json_string("session.resume");
        snap = gwss_store_call(st, method, rparams, io, raise_out);
        json_free(method);
        json_free(rparams);
        if (*raise_out) goto done;
        snap_owned = true;
    }
    /* fence = {}; if operation in _GENERATION_FENCED: fence[expected_generation] */
    {
        const char *op = (operation && operation->type == JSON_STRING) ? operation->str_val : "";
        if (gwss_in_list(op, GWSS_GENERATION_FENCED,
                         sizeof(GWSS_GENERATION_FENCED) / sizeof(GWSS_GENERATION_FENCED[0]))) {
            if (!snap || snap->type != JSON_OBJECT) {
                gwss_raise_fmt(raise_out, "TypeError", "'%s' object is not subscriptable",
                               gwss_typename(snap));
                goto done;
            }
            json_t *gen = json_obj_get(snap, "execution_generation");
            if (!gen) { gwss_raise_key(raise_out, "execution_generation"); goto done; }
            json_set(fence, "expected_generation", json_copy(gen));
        }
    }
    /* self.call("session.mutate", session_id=..., request_id=uuid4().hex,
     *           expected_revision=snapshot["revision"], operation=..., payload=..., **fence)
     * uuid fires BEFORE the revision subscript (kwarg left-to-right). */
    params = json_object();
    json_set(params, "session_id", gwss_copy(session_id));
    {
        json_t *hex = gwss_seam_obj(io, "uuid4", raise_out);
        if (*raise_out) goto done;
        json_set(params, "request_id", hex ? hex : json_null());
    }
    if (!snap || snap->type != JSON_OBJECT) {
        gwss_raise_fmt(raise_out, "TypeError", "'%s' object is not subscriptable",
                       gwss_typename(snap));
        goto done;
    }
    {
        json_t *rev = json_obj_get(snap, "revision");
        if (!rev) { gwss_raise_key(raise_out, "revision"); goto done; }
        json_set(params, "expected_revision", json_copy(rev));
    }
    json_set(params, "operation", gwss_copy(operation));
    json_set(params, "payload", gwss_copy(payload));
    for (size_t i = 0; i < fence->c.count; i++)
        json_set(params, fence->c.keys[i], json_copy(fence->c.items[i]));
    {
        json_t *method = json_string("session.mutate");
        ret = gwss_store_call(st, method, params, io, raise_out);
        json_free(method);
    }
done:
    json_free(fence);
    json_free(params);
    if (snap_owned) json_free(snap);
    return ret;
}

/* ── set_session_title: re-fence + title-holder recovery ── */

/* PoP: gwss_set_session_title @ hermes_cli/gateway_session_store.py:set_session_title */
json_t *gwss_set_session_title(gwss_store_t *st, const json_t *in, gwss_io_t *io, json_t **raise_out) {
    const char *sid = json_get_str(in, "session_id", "");
    const json_t *title = json_obj_get(in, "title");
    json_t *sidn = json_string(sid);
    json_t *snapshot = NULL;
    json_t *ret = NULL;

    json_t *params = json_object();
    json_set(params, "session_id", json_copy(sidn));
    json_t *method = json_string("session.resume");
    snapshot = gwss_store_call(st, method, params, io, raise_out);
    json_free(method);
    json_free(params);
    if (*raise_out) goto done;
    if (!snapshot || snapshot->type != JSON_OBJECT) {
        gwss_raise_fmt(raise_out, "TypeError", "'%s' object is not subscriptable",
                       gwss_typename(snapshot));
        goto done;
    }
    {
        json_t *ssid = json_obj_get(snapshot, "stored_session_id");
        if (!ssid) { gwss_raise_key(raise_out, "stored_session_id"); goto done; }
        if (!gwss_eq(ssid, sidn)) {
            /* re-fence on the named row's own revision */
            json_t *defrev = json_obj_get(snapshot, "revision");
            if (!defrev) { gwss_raise_key(raise_out, "revision"); goto done; }
            json_t *rargs = json_object();
            json_set(rargs, "session_id", json_copy(sidn));
            json_set(rargs, "default", json_copy(defrev));
            json_t *newrev = gwss_seam(io, "_row_revision", rargs, raise_out);
            if (*raise_out) goto done;
            json_t *newsnap = json_copy(snapshot);
            json_set(newsnap, "revision", newrev ? newrev : json_null());
            json_free(snapshot);
            snapshot = newsnap;
        }
    }
    /* try: self._mutate(sid, "rename", {"title": title}, snapshot) */
    {
        json_t *op = json_string("rename");
        json_t *payload = json_object();
        json_set(payload, "title", gwss_copy(title));
        json_t *mr = gwss_mutate(st, sidn, op, payload, snapshot, io, raise_out);
        json_free(op);
        json_free(payload);
        json_free(mr);
    }
    if (*raise_out) {
        json_t *orig = *raise_out;
        *raise_out = NULL;
        if (strcmp(gwss_raise_kind(orig), "GatewayClientError") == 0) {
            json_t *hargs = json_object();
            json_set(hargs, "session_id", json_copy(sidn));
            json_set(hargs, "title", gwss_copy(title));
            json_t *holder = gwss_seam(io, "_title_holder", hargs, raise_out);
            if (*raise_out) { json_free(orig); goto done; } /* holder raise replaces */
            if (gwss_truth(holder)) {
                char tstr[512], hstr[512], msg[1200];
                gwss_py_str(title, tstr, sizeof(tstr));
                gwss_py_str(holder, hstr, sizeof(hstr));
                snprintf(msg, sizeof(msg), "Title '%s' is already in use by session %s", tstr, hstr);
                gwss_raise(raise_out, "ValueError", msg);
                json_free(orig);
                json_free(holder);
                goto done;
            }
            json_free(holder);
            *raise_out = orig; /* re-raise the refusal */
            goto done;
        }
        *raise_out = orig; /* KeyError/TypeError/BaseException propagate */
        goto done;
    }
    /* if sid in self.snapshots: connection.state(sid).title = title */
    {
        json_t *snaps = gwss_snapshots(st);
        if (snaps && json_obj_get(snaps, sid)) {
            json_t *state = gwss_conn_state(st, io, sid);
            if (state) json_set(state, "title", gwss_copy(title));
        }
    }
    ret = json_bool(true);
done:
    json_free(snapshot);
    json_free(sidn);
    return ret;
}

/* ── delete_session: detach-on-refused-delete recovery ── */

/* PoP: gwss_delete_session @ hermes_cli/gateway_session_store.py:delete_session */
json_t *gwss_delete_session(gwss_store_t *st, const json_t *session_id, gwss_io_t *io, json_t **raise_out) {
    char sid[256];
    gwss_py_str(session_id, sid, sizeof(sid));
    json_t *sidn = json_string(sid);
    json_t *snapshot = NULL;
    json_t *ret = NULL;

    /* self.snapshots.pop(session_id, None) — result discarded */
    json_t *snaps = gwss_snapshots(st);
    if (snaps) json_obj_del(snaps, sid);

    json_t *params = json_object();
    json_set(params, "session_id", json_copy(sidn));
    json_t *method = json_string("session.resume");
    snapshot = gwss_store_call(st, method, params, io, raise_out);
    json_free(method);
    json_free(params);
    if (*raise_out) goto done;
    if (!snapshot || snapshot->type != JSON_OBJECT) {
        gwss_raise_fmt(raise_out, "AttributeError", "'%s' object has no attribute 'get'",
                       gwss_typename(snapshot));
        goto done;
    }
    /* resolved = snapshot.get("session_id") or session_id */
    char resolved[256];
    {
        json_t *rsid = json_obj_get(snapshot, "session_id");
        if (gwss_truth(rsid)) gwss_py_str(rsid, resolved, sizeof(resolved));
        else snprintf(resolved, sizeof(resolved), "%s", sid);
    }
    json_t *states = gwss_states(st);
    json_t *state = states ? json_obj_get(states, resolved) : NULL;
    bool held = state && state->type == JSON_OBJECT
        && !gwss_is_null(json_obj_get(state, "subscription_id"));
    /* try: self._mutate(sid, "delete", {}, snapshot) */
    {
        json_t *op = json_string("delete");
        json_t *payload = json_object();
        json_t *mr = gwss_mutate(st, sidn, op, payload, snapshot, io, raise_out);
        json_free(op);
        json_free(payload);
        json_free(mr);
    }
    if (*raise_out) {
        json_t *orig = *raise_out;
        *raise_out = NULL;
        if (strcmp(gwss_raise_kind(orig), "GatewayClientError") == 0) {
            json_t *sub = json_obj_get(snapshot, "subscription_id");
            if (!held && gwss_truth(sub)) {
                /* plain detach of the attachment the read made */
                json_t *dparams = json_object();
                json_set(dparams, "session_id", json_string(resolved));
                json_set(dparams, "subscription_id", json_copy(sub));
                json_t *dm = json_string("session.detach");
                json_t *dr = gwss_store_call(st, dm, dparams, io, raise_out);
                json_free(dm);
                json_free(dparams);
                json_free(dr);
                if (*raise_out) {
                    if (strcmp(gwss_raise_kind(*raise_out), "GatewayClientError") == 0) {
                        gwss_log_debug(io, "session.detach after a refused delete failed: %s", *raise_out);
                        json_free(*raise_out);
                        *raise_out = NULL;
                    } else {
                        json_free(orig); /* detach's other raise replaces */
                        goto done;
                    }
                }
            }
            *raise_out = orig; /* re-raise the delete refusal */
            goto done;
        }
        *raise_out = orig; /* BaseException/other: no detach */
        goto done;
    }
    {
        json_t *lr = gwss_leave(st, sid);
        json_free(lr);
    }
    ret = json_bool(true);
done:
    json_free(snapshot);
    json_free(sidn);
    return ret;
}

/* ── end_session: redial/reattach, detach, identity clear ── */

/* PoP: gwss_end_session @ hermes_cli/gateway_session_store.py:end_session */
json_t *gwss_end_session(gwss_store_t *st, const json_t *in, gwss_io_t *io, json_t **raise_out) {
    const char *sid = json_get_str(in, "session_id", "");
    const json_t *reason = json_obj_get(in, "reason");
    json_t *subscription = NULL;

    /* snapshot = self.snapshots.pop(session_id, None) */
    json_t *snaps = gwss_snapshots(st);
    bool popped_existed = snaps && json_obj_get(snaps, sid) != NULL;
    if (snaps) json_obj_del(snaps, sid);

    json_t *states = gwss_states(st);
    json_t *state = states ? json_obj_get(states, sid) : NULL;
    bool has_state = state && state->type == JSON_OBJECT;
    json_t *subid = has_state ? json_obj_get(state, "subscription_id") : NULL;
    json_t *conn = gwss_conn(st);
    json_t *final = conn ? json_obj_get(conn, "final") : NULL;
    bool reachable = has_state && !gwss_is_null(subid) && !gwss_truth(final);

    if (reachable) {
        /* hung up: redial first, then re-attach on a replay gap */
        json_t *sr = gwss_seam_obj(io, "connection.start", raise_out);
        if (*raise_out) {
            if (strcmp(gwss_raise_kind(*raise_out), "GatewayClientError") == 0) {
                gwss_log_debug(io, "redial for session.detach failed: %s", *raise_out);
                json_free(*raise_out);
                *raise_out = NULL;
                reachable = false;
            } else {
                goto done;
            }
        } else {
            json_free(sr);
            json_t *gaps = json_obj_get(state, "gaps");
            json_t *agaps = json_obj_get(state, "attached_gaps");
            if (!gwss_eq(gaps, agaps)) {
                json_t *rargs = json_object();
                json_set(rargs, "session_id", json_string(sid));
                json_t *rr = gwss_seam(io, "connection.reattach", rargs, raise_out);
                if (*raise_out) {
                    if (strcmp(gwss_raise_kind(*raise_out), "GatewayClientError") == 0) {
                        gwss_log_debug(io, "redial for session.detach failed: %s", *raise_out);
                        json_free(*raise_out);
                        *raise_out = NULL;
                        reachable = false;
                    } else {
                        goto done;
                    }
                } else {
                    json_free(rr);
                }
            }
        }
    }
    /* subscription = self._leave(session_id) */
    subscription = gwss_leave(st, sid);
    if (gwss_truth(subscription) && reachable) {
        json_t *dparams = json_object();
        json_set(dparams, "session_id", json_string(sid));
        json_set(dparams, "subscription_id", json_copy(subscription));
        if (gwss_truth(reason)) json_set(dparams, "end_reason", json_copy(reason));
        json_t *dm = json_string("session.detach");
        json_t *dr = gwss_store_call(st, dm, dparams, io, raise_out);
        json_free(dm);
        json_free(dparams);
        json_free(dr);
        if (*raise_out) {
            if (strcmp(gwss_raise_kind(*raise_out), "GatewayClientError") == 0) {
                gwss_log_debug(io, "session.detach failed: %s", *raise_out);
                json_free(*raise_out);
                *raise_out = NULL;
            } else {
                goto done;
            }
        }
    }
    /* if self.current is snapshot: self.current = None (identity, not value) */
    {
        json_t *current = gwss_mget(st, "current");
        bool identity = popped_existed
            ? (st->current_ref && strcmp(st->current_ref, sid) == 0)
            : gwss_is_null(current);
        if (identity) {
            json_set(st->m, "current", json_null());
            free(st->current_ref);
            st->current_ref = NULL;
        }
    }
done:
    json_free(subscription);
    return NULL;
}

/* ── delete_session_if_empty: fail-closed probe ── */

/* PoP: gwss_delete_session_if_empty @ hermes_cli/gateway_session_store.py:delete_session_if_empty */
json_t *gwss_delete_session_if_empty(gwss_store_t *st, const json_t *in, gwss_io_t *io, json_t **raise_out) {
    const char *sid = json_get_str(in, "session_id", "");
    /* if self._db() is None: return False */
    json_t *db = gwss_seam_obj(io, "_db", raise_out);
    if (*raise_out) return NULL;
    bool db_none = gwss_is_null(db);
    json_free(db);
    if (db_none) return json_bool(false);
    /* try: empty = persistence.sessions.is_empty_candidate(sid) */
    json_t *pargs = json_object();
    json_set(pargs, "session_id", json_string(sid));
    json_t *empty = gwss_seam(io, "persistence.sessions.is_empty_candidate", pargs, raise_out);
    if (*raise_out) {
        if (!gwss_is_base_exc(gwss_raise_kind(*raise_out))) {
            gwss_log_debug(io, "empty-session probe failed: %s", *raise_out);
            json_free(*raise_out);
            *raise_out = NULL;
            return json_bool(false);
        }
        return NULL; /* BaseException propagates */
    }
    bool e = gwss_truth(empty);
    json_free(empty);
    if (!e) return json_bool(false);
    /* try: return self.delete_session(sid) */
    json_t *sidn = json_string(sid);
    json_t *r = gwss_delete_session(st, sidn, io, raise_out);
    json_free(sidn);
    if (*raise_out) {
        if (strcmp(gwss_raise_kind(*raise_out), "GatewayClientError") == 0) {
            gwss_log_debug(io, "empty session prune refused: %s", *raise_out);
            json_free(*raise_out);
            *raise_out = NULL;
            json_free(r);
            return json_bool(false);
        }
        json_free(r);
        return NULL;
    }
    return r;
}