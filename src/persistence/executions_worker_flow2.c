/*
 * executions_worker_flow2.c — FLOW2 port of the UNCLAIMED remainder verbs of
 * persistence/sqlite/workers/executions.py (hermes-agent, READ-ONLY).
 *
 * A CRLF-aware PoP cross-check confirmed these defs are genuinely UNCLAIMED by
 * executions_worker_flow (which holds the other 24 defs). This is a NEW flow:
 * each verb below builds its wire payload from typed parameters, runs ONE
 * receipt/gateway transaction through the universal seam, and converts the
 * stored result to its contract record. The SQLite/gateway call is NEVER real
 * I/O — it is always a scripted seam.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All helpers are static and ewf2_-prefixed.
 *
 * Universal seam (ewf2_call): name is one of "_persist" / "_value" / "_ledger".
 *   "_persist"  args = {epoch, execution_id, session_id, generation, sequence,
 *                       operation, payload}          (mutate_worker_execution)
 *   "_value"    args = same shape; the ['value'] projection wrapper
 *   "_ledger"   args = {epoch, execution_id, session_id, generation, sequence,
 *                       operation, payload, worker_pid, worker_birth,
 *                       owner_started_at, probe_refused}  (persist_delegation)
 * A script is in.io[name]: a plain value (every call), a JSON ARRAY (a per-call
 * sequence, fail-closed on exhaustion), or {"__raise__": cls, "message": m}
 * (a raised exception; the optional {"__script__":true,"raise":...} /
 * {"__script__":true,"ret":...} form is also honoured). A missing script is a
 * fail-closed RuntimeError. Every call appends ONE ordered trace event
 * {"name","args","ret"} (ret is the value or the raise descriptor).
 *
 * Representation contract (shared with tests/oracle_executions_worker_flow2.py):
 * - NULL json_t* == absent key; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; cls is
 *   canonicalized through the FULL builtin hierarchy (IOError->OSError etc.).
 * - Python numbers are JSON numbers; bools are JSON bools; None is null.
 * - NewType/enum constructors (SessionId etc.) are identity; record
 *   constructors (WorkerLineageView etc.) are field dicts in declaration
 *   order. Constructor VALUE validation is outside the fixture domain.
 * - dataclass collaborator payloads (dispatch/claim/...) are plain JSON values;
 *   delegation_payload(x) == {"delegation_id": x} in this domain.
 * - BaseException members propagate (no except clause in these verbs).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── context ────────────────────────────────────────────────────────────── */
typedef struct ewf2_ctx {
    const json_t *io;      /* borrowed: seam scripts (in.io) */
    json_t *trace;         /* owned: ordered seam events */
    json_t *cursors;       /* owned: per-name sequence cursors */
} ewf2_ctx_t;

static int ewf2_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static json_t *ewf2_get(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static const char *ewf2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

/* ── FULL builtin exception hierarchy (py3.14) ─────────────────────────── */
static const char *ewf2_exc_canon(const char *c) {
    if (ewf2_eq(c, "IOError") || ewf2_eq(c, "EnvironmentError") ||
        ewf2_eq(c, "WindowsError")) return "OSError";
    return c;
}
static const char *ewf2_exc_parent(const char *c) {
    static const struct { const char *c, *p; } T[] = {
        {"BaseException", NULL},
        {"SystemExit", "BaseException"}, {"KeyboardInterrupt", "BaseException"},
        {"GeneratorExit", "BaseException"}, {"BaseExceptionGroup", "BaseException"},
        {"ExceptionGroup", "Exception"}, {"Exception", "BaseException"},
        {"ArithmeticError", "Exception"}, {"FloatingPointError", "ArithmeticError"},
        {"OverflowError", "ArithmeticError"}, {"ZeroDivisionError", "ArithmeticError"},
        {"AssertionError", "Exception"}, {"AttributeError", "Exception"},
        {"BufferError", "Exception"}, {"EOFError", "Exception"},
        {"ImportError", "Exception"}, {"ModuleNotFoundError", "ImportError"},
        {"LookupError", "Exception"}, {"IndexError", "LookupError"},
        {"KeyError", "LookupError"}, {"MemoryError", "Exception"},
        {"NameError", "Exception"}, {"UnboundLocalError", "NameError"},
        {"OSError", "Exception"}, {"BlockingIOError", "OSError"},
        {"ChildProcessError", "OSError"}, {"ConnectionError", "OSError"},
        {"BrokenPipeError", "ConnectionError"}, {"ConnectionAbortedError", "ConnectionError"},
        {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
        {"FileExistsError", "OSError"}, {"FileNotFoundError", "OSError"},
        {"InterruptedError", "OSError"}, {"IsADirectoryError", "OSError"},
        {"NotADirectoryError", "OSError"}, {"PermissionError", "OSError"},
        {"ProcessLookupError", "OSError"}, {"TimeoutError", "OSError"},
        {"ReferenceError", "Exception"}, {"RuntimeError", "Exception"},
        {"NotImplementedError", "RuntimeError"}, {"RecursionError", "RuntimeError"},
        {"PythonFinalizationError", "RuntimeError"}, {"StopAsyncIteration", "Exception"},
        {"StopIteration", "Exception"}, {"SyntaxError", "Exception"},
        {"IndentationError", "SyntaxError"}, {"TabError", "IndentationError"},
        {"_IncompleteInputError", "SyntaxError"}, {"SystemError", "Exception"},
        {"TypeError", "Exception"}, {"ValueError", "Exception"},
        {"UnicodeError", "ValueError"}, {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeEncodeError", "UnicodeError"}, {"UnicodeTranslateError", "UnicodeError"},
        {"Warning", "Exception"}, {"UserWarning", "Warning"},
        {"DeprecationWarning", "Warning"}, {"PendingDeprecationWarning", "Warning"},
        {"SyntaxWarning", "Warning"}, {"RuntimeWarning", "Warning"},
        {"FutureWarning", "Warning"}, {"ImportWarning", "Warning"},
        {"UnicodeWarning", "Warning"}, {"BytesWarning", "Warning"},
        {"ResourceWarning", "Warning"}, {"EncodingWarning", "Warning"},
        {NULL, NULL}
    };
    const char *cc = ewf2_exc_canon(c);
    for (size_t i = 0; T[i].c; i++) if (ewf2_eq(T[i].c, cc)) return T[i].p;
    return "Exception";
}
static bool ewf2_exc_is(const char *kind, const char *base) {
    const char *k = ewf2_exc_canon(kind);
    for (int d = 0; k && d < 32; d++) {
        if (ewf2_eq(k, base)) return true;
        k = ewf2_exc_parent(k);
    }
    return false;
}

/* ── raise descriptors ─────────────────────────────────────────────────── */
static json_t *ewf2_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", ewf2_exc_canon(type ? type : "Exception"));
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static bool ewf2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}

/* ── Python truthiness / str (fixture domain) ──────────────────────────── */
static bool ewf2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static json_t *ewf2_bool(const json_t *v) { return json_bool(ewf2_truthy(v)); }
static json_t *ewf2_pystr(const json_t *v) {
    if (v && v->type == JSON_STRING) return json_copy(v);
    if (v && v->type == JSON_BOOL) return json_string(v->bool_val ? "True" : "False");
    if (v && v->type == JSON_NUMBER) {
        char b[64];
        if (v->num_val == (double)(long long)v->num_val) snprintf(b, sizeof b, "%.0f", v->num_val);
        else snprintf(b, sizeof b, "%.17g", v->num_val);
        return json_string(b);
    }
    if (!v || v->type == JSON_NULL) return json_string("None");
    return json_string("");
}

/* ── the one universal seam ─────────────────────────────────────────────── */
static json_t *ewf2_call(ewf2_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());   /* adopts args */
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    json_t *ret = NULL;
    const json_t *used = entry;
    if (!entry) {
        char m[256]; snprintf(m, sizeof m, "no script for %s", name);
        ret = ewf2_err("RuntimeError", m);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char m[256]; snprintf(m, sizeof m, "script exhausted for %s", name);
            ret = ewf2_err("RuntimeError", m);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT) {
            const json_t *rs = json_obj_get(used, "__raise__");
            if (rs) {
                ret = ewf2_err(json_get_str(used, "__raise__", "Exception"),
                               json_get_str(used, "message", ""));
            } else {
                const json_t *sc = json_obj_get(used, "__script__");
                if (sc && sc->type == JSON_BOOL && sc->bool_val) {
                    const json_t *rse = json_obj_get(used, "raise");
                    if (rse && rse->type == JSON_OBJECT)
                        ret = ewf2_err(json_get_str(rse, "__raise__", "Exception"),
                                       json_get_str(rse, "message", ""));
                    else {
                        const json_t *rr = json_obj_get(used, "ret");
                        ret = rr ? json_copy(rr) : json_null();
                    }
                } else ret = json_copy(used);
            }
        } else ret = used ? json_copy(used) : json_null();
    }
    json_set(ev, "ret", json_copy(ret));
    json_append(ctx->trace, ev);
    return ret;
}

/* Every verb passes these five identity fields to the seam. */
static json_t *ewf2_base_args(const json_t *a) {
    json_t *o = json_object();
    const char *keys[] = {"epoch", "execution_id", "session_id", "generation", "sequence"};
    for (size_t i = 0; i < 5; i++) {
        const json_t *v = ewf2_get(a, keys[i]);
        json_set(o, keys[i], v ? json_copy(v) : json_null());
    }
    return o;
}
/* _persist / _value seam args. */
static json_t *ewf2_seam_args(const json_t *a, const char *operation, json_t *payload) {
    json_t *o = ewf2_base_args(a);
    json_set_hdr(o, "operation", operation);
    json_set(o, "payload", payload ? payload : json_object());
    return o;
}
/* _ledger seam args (persist_worker_delegation). */
static json_t *ewf2_ledger_args(const json_t *a, const char *operation, json_t *payload) {
    json_t *o = ewf2_seam_args(a, operation, payload);
    const char *extra[] = {"worker_pid", "worker_birth", "owner_started_at", "probe_refused"};
    for (size_t i = 0; i < 4; i++) {
        const json_t *v = ewf2_get(a, extra[i]);
        json_set(o, extra[i], v ? json_copy(v) : json_null());
    }
    return o;
}
/* delegation_payload(x) in the fixture domain: {"delegation_id": x}. */
static json_t *ewf2_delegation_payload(const json_t *x) {
    json_t *o = json_object();
    json_set(o, "delegation_id", x ? json_copy(x) : json_null());
    return o;
}
/* bool(self._ledger(...)) helper behind the five *_delegation_delivery verbs. */
static json_t *ewf2_delivery(ewf2_ctx_t *ctx, const char *action, const json_t *a) {
    char op[64];
    snprintf(op, sizeof op, "delegation.%s", action);
    json_t *payload = ewf2_delegation_payload(ewf2_get(a, "claim"));
    json_t *r = ewf2_call(ctx, "_ledger", ewf2_ledger_args(a, op, payload));
    if (ewf2_is_raise(r)) return r;
    json_t *o = ewf2_bool(r); json_free(r); return o;
}

/* ── ported verbs ───────────────────────────────────────────────────────── */

/* PoP: ewf2_renew_turn_lease @ persistence/sqlite/workers/executions.py:renew_turn_lease */
static json_t *ewf2_renew_turn_lease(ewf2_ctx_t *ctx, const json_t *a) {
    const json_t *lease = ewf2_get(a, "lease");
    json_t *payload = json_object();
    const json_t *h = ewf2_get(lease, "holder"), *t = ewf2_get(lease, "ttl_seconds");
    json_set(payload, "holder", h ? json_copy(h) : json_null());
    json_set(payload, "ttl_seconds", t ? json_copy(t) : json_null());
    json_t *r = ewf2_call(ctx, "_value", ewf2_seam_args(a, "turn.renew", payload));
    if (ewf2_is_raise(r)) return r;
    json_t *o = ewf2_bool(r); json_free(r); return o;
}
/* PoP: ewf2_touch_session_activity @ persistence/sqlite/workers/executions.py:touch_session_activity */
static json_t *ewf2_touch_session_activity(ewf2_ctx_t *ctx, const json_t *a) {
    const json_t *act = ewf2_get(a, "activity");
    json_t *payload = json_object();
    const char *keys[] = {"ts", "description", "provenance"};
    for (size_t i = 0; i < 3; i++) {
        const json_t *v = ewf2_get(act, keys[i]);
        json_set(payload, keys[i], v ? json_copy(v) : json_null());
    }
    json_t *r = ewf2_call(ctx, "_value", ewf2_seam_args(a, "session.activity", payload));
    if (ewf2_is_raise(r)) return r;
    json_free(r); return json_null();
}
/* PoP: ewf2_clear_session_activity @ persistence/sqlite/workers/executions.py:clear_session_activity */
static json_t *ewf2_clear_session_activity(ewf2_ctx_t *ctx, const json_t *a) {
    json_t *r = ewf2_call(ctx, "_value", ewf2_seam_args(a, "session.activity_clear", json_object()));
    if (ewf2_is_raise(r)) return r;
    json_free(r); return json_null();
}
/* PoP: ewf2_end_session @ persistence/sqlite/workers/executions.py:end_session */
static json_t *ewf2_end_session(ewf2_ctx_t *ctx, const json_t *a) {
    json_t *payload = json_object();
    const json_t *e = ewf2_get(a, "end_reason");
    json_set(payload, "end_reason", e ? json_copy(e) : json_null());
    json_t *r = ewf2_call(ctx, "_value", ewf2_seam_args(a, "session.end", payload));
    if (ewf2_is_raise(r)) return r;
    json_free(r); return json_null();
}
/* PoP: ewf2_set_session_system_prompt @ persistence/sqlite/workers/executions.py:set_session_system_prompt */
static json_t *ewf2_set_session_system_prompt(ewf2_ctx_t *ctx, const json_t *a) {
    json_t *payload = json_object();
    const json_t *s = ewf2_get(a, "system_prompt");
    json_set(payload, "system_prompt", s ? json_copy(s) : json_null());
    json_t *r = ewf2_call(ctx, "_value", ewf2_seam_args(a, "session.prompt", payload));
    if (ewf2_is_raise(r)) return r;
    json_free(r); return json_null();
}
/* PoP: ewf2_latest_conversation_role @ persistence/sqlite/workers/executions.py:latest_conversation_role */
static json_t *ewf2_latest_conversation_role(ewf2_ctx_t *ctx, const json_t *a) {
    json_t *r = ewf2_call(ctx, "_value", ewf2_seam_args(a, "session.tail_role", json_object()));
    if (ewf2_is_raise(r)) return r;
    return r; /* str | None identity */
}
/* PoP: ewf2_message_role @ persistence/sqlite/workers/executions.py:message_role */
static json_t *ewf2_message_role(ewf2_ctx_t *ctx, const json_t *a) {
    json_t *payload = json_object();
    const json_t *rid = ewf2_get(a, "row_id");
    json_set(payload, "row_id", rid ? json_copy(rid) : json_null());
    json_t *r = ewf2_call(ctx, "_value", ewf2_seam_args(a, "session.row_role", payload));
    if (ewf2_is_raise(r)) return r;
    return r; /* str | None identity */
}
/* PoP: ewf2_acquire_compression_lock @ persistence/sqlite/workers/executions.py:acquire_compression_lock */
static json_t *ewf2_acquire_compression_lock(ewf2_ctx_t *ctx, const json_t *a) {
    const json_t *lease = ewf2_get(a, "lease");
    json_t *payload = json_object();
    const json_t *h = ewf2_get(lease, "holder"), *t = ewf2_get(lease, "ttl_seconds");
    json_set(payload, "holder", h ? json_copy(h):json_null());
    json_set(payload, "ttl_seconds", t ? json_copy(t):json_null());
    json_t *r = ewf2_call(ctx, "_value", ewf2_seam_args(a, "compression.lock.acquire", payload));
    if (ewf2_is_raise(r)) return r;
    json_t *o = ewf2_bool(r); json_free(r); return o;
}
/* PoP: ewf2_renew_compression_lock @ persistence/sqlite/workers/executions.py:renew_compression_lock */
static json_t *ewf2_renew_compression_lock(ewf2_ctx_t *ctx, const json_t *a) {
    const json_t *lease = ewf2_get(a, "lease");
    json_t *payload = json_object();
    const json_t *h = ewf2_get(lease, "holder"), *t = ewf2_get(lease, "ttl_seconds");
    json_set(payload, "holder", h ? json_copy(h):json_null());
    json_set(payload, "ttl_seconds", t ? json_copy(t):json_null());
    json_t *r = ewf2_call(ctx, "_value", ewf2_seam_args(a, "compression.lock.renew", payload));
    if (ewf2_is_raise(r)) return r;
    json_t *o = ewf2_bool(r); json_free(r); return o;
}
/* PoP: ewf2_compression_lock_holder @ persistence/sqlite/workers/executions.py:compression_lock_holder */
static json_t *ewf2_compression_lock_holder(ewf2_ctx_t *ctx, const json_t *a) {
    json_t *r = ewf2_call(ctx, "_value", ewf2_seam_args(a, "compression.lock.holder", json_object()));
    if (ewf2_is_raise(r)) return r;
    return r; /* str | None identity */
}
/* PoP: ewf2_cleanup_compression_lock @ persistence/sqlite/workers/executions.py:cleanup_compression_lock */
static json_t *ewf2_cleanup_compression_lock(ewf2_ctx_t *ctx, const json_t *a) {
    json_t *payload = json_object();
    const json_t *t = ewf2_get(a, "target_session_id"), *h = ewf2_get(a, "holder");
    json_set(payload, "target", t ? json_copy(t):json_null());
    json_set(payload, "holder", h ? json_copy(h):json_null());
    json_t *r = ewf2_call(ctx, "_value", ewf2_seam_args(a, "compression.cleanup", payload));
    if (ewf2_is_raise(r)) return r;
    json_free(r); return json_null();
}
/* PoP: ewf2_record_compression_cooldown @ persistence/sqlite/workers/executions.py:record_compression_cooldown */
static json_t *ewf2_record_compression_cooldown(ewf2_ctx_t *ctx, const json_t *a) {
    const json_t *cd = ewf2_get(a, "cooldown");
    json_t *payload = json_object();
    const json_t *u = ewf2_get(cd, "cooldown_until"), *e = ewf2_get(cd, "error");
    json_set(payload, "cooldown_until", u ? json_copy(u):json_null());
    json_set(payload, "error", e ? json_copy(e):json_null());
    json_t *r = ewf2_call(ctx, "_value", ewf2_seam_args(a, "compression.cooldown.record", payload));
    if (ewf2_is_raise(r)) return r;
    json_free(r); return json_null();
}
/* PoP: ewf2_restore_compression_cooldown @ persistence/sqlite/workers/executions.py:restore_compression_cooldown */
static json_t *ewf2_restore_compression_cooldown(ewf2_ctx_t *ctx, const json_t *a) {
    const json_t *snap = ewf2_get(a, "snapshot");
    json_t *inner = json_object();
    const char *keys[] = {"session_exists", "cooldown_until", "error"};
    for (size_t i = 0; i < 3; i++) {
        const json_t *v = ewf2_get(snap, keys[i]);
        json_set(inner, keys[i], v ? json_copy(v) : json_null());
    }
    json_t *payload = json_object();
    json_set(payload, "snapshot", inner);
    json_t *r = ewf2_call(ctx, "_value", ewf2_seam_args(a, "compression.cooldown.restore", payload));
    if (ewf2_is_raise(r)) return r;
    json_free(r); return json_null();
}
/* PoP: ewf2_clear_compression_cooldown @ persistence/sqlite/workers/executions.py:clear_compression_cooldown */
static json_t *ewf2_clear_compression_cooldown(ewf2_ctx_t *ctx, const json_t *a) {
    json_t *r = ewf2_call(ctx, "_value", ewf2_seam_args(a, "compression.cooldown.clear", json_object()));
    if (ewf2_is_raise(r)) return r;
    json_free(r); return json_null();
}
/* PoP: ewf2_set_compression_fallback_streak @ persistence/sqlite/workers/executions.py:set_compression_fallback_streak */
static json_t *ewf2_set_compression_fallback_streak(ewf2_ctx_t *ctx, const json_t *a) {
    json_t *payload = json_object();
    const json_t *s = ewf2_get(a, "streak");
    json_set(payload, "value", s ? json_copy(s):json_null());
    json_t *r = ewf2_call(ctx, "_value", ewf2_seam_args(a, "compression.fallback", payload));
    if (ewf2_is_raise(r)) return r;
    json_free(r); return json_null();
}
/* PoP: ewf2_set_compression_ineffective_count @ persistence/sqlite/workers/executions.py:set_compression_ineffective_count */
static json_t *ewf2_set_compression_ineffective_count(ewf2_ctx_t *ctx, const json_t *a) {
    json_t *payload = json_object();
    const json_t *c = ewf2_get(a, "count");
    json_set(payload, "value", c ? json_copy(c):json_null());
    json_t *r = ewf2_call(ctx, "_value", ewf2_seam_args(a, "compression.ineffective", payload));
    if (ewf2_is_raise(r)) return r;
    json_free(r); return json_null();
}
/* PoP: ewf2_set_compression_recovery_deadline @ persistence/sqlite/workers/executions.py:set_compression_recovery_deadline */
static json_t *ewf2_set_compression_recovery_deadline(ewf2_ctx_t *ctx, const json_t *a) {
    json_t *payload = json_object();
    const json_t *d = ewf2_get(a, "deadline");
    json_set(payload, "value", d ? json_copy(d):json_null());
    json_t *r = ewf2_call(ctx, "_value", ewf2_seam_args(a, "compression.recovery", payload));
    if (ewf2_is_raise(r)) return r;
    json_free(r); return json_null();
}
/* PoP: ewf2_reopen_orphaned_compression @ persistence/sqlite/workers/executions.py:reopen_orphaned_compression */
static json_t *ewf2_reopen_orphaned_compression(ewf2_ctx_t *ctx, const json_t *a) {
    json_t *r = ewf2_call(ctx, "_value", ewf2_seam_args(a, "compression.reopen", json_object()));
    if (ewf2_is_raise(r)) return r;
    json_t *o = ewf2_bool(r); json_free(r); return o;
}
/* PoP: ewf2_compression_session_context @ persistence/sqlite/workers/executions.py:compression_session_context */
static json_t *ewf2_compression_session_context(ewf2_ctx_t *ctx, const json_t *a) {
    json_t *payload = json_object();
    const json_t *t = ewf2_get(a, "target_session_id");
    json_set(payload, "target", t ? json_copy(t):json_null());
    json_t *r = ewf2_call(ctx, "_persist", ewf2_seam_args(a, "compression.context", payload));
    if (ewf2_is_raise(r)) return r;
    json_t *s = ewf2_get(r, "session");
    json_t *o = s ? json_copy(s) : json_null();
    json_free(r); return o;
}
/* PoP: ewf2_compression_lineage_view @ persistence/sqlite/workers/executions.py:compression_lineage_view */
static json_t *ewf2_compression_lineage_view(ewf2_ctx_t *ctx, const json_t *a) {
    json_t *payload = json_object();
    const json_t *t = ewf2_get(a, "target_session_id");
    json_set(payload, "target", t ? json_copy(t):json_null());
    json_t *r = ewf2_call(ctx, "_persist", ewf2_seam_args(a, "compression.lineage", payload));
    if (ewf2_is_raise(r)) return r;
    json_t *out = json_object();
    /* readable_ids / lineage: tuple(SessionId(s) ...) -> array (identity). */
    const json_t *rids = ewf2_get(r, "readable_ids");
    json_t *ra = json_array();
    if (rids && rids->type == JSON_ARRAY)
        for (size_t i = 0; i < rids->c.count; i++) json_append(ra, json_copy(json_get(rids, i)));
    json_set(out, "readable_ids", ra);
    const json_t *lin = ewf2_get(r, "lineage");
    json_t *la = json_array();
    if (lin && lin->type == JSON_ARRAY)
        for (size_t i = 0; i < lin->c.count; i++) json_append(la, json_copy(json_get(lin, i)));
    json_set(out, "lineage", la);
    const char *one[] = {"root", "tip", "resume", "tool_root"};
    for (size_t i = 0; i < 4; i++) {
        const json_t *v = ewf2_get(r, one[i]);
        json_set(out, one[i], v ? json_copy(v) : json_null());
    }
    /* identity = (bool(x0), str(x1)) -> [bool, str]. */
    const json_t *idv = ewf2_get(r, "identity");
    json_t *pair = json_array();
    json_append(pair, ewf2_bool((idv && idv->type == JSON_ARRAY && idv->c.count > 0) ? json_get(idv, 0) : NULL));
    json_append(pair, ewf2_pystr((idv && idv->type == JSON_ARRAY && idv->c.count > 1) ? json_get(idv, 1) : NULL));
    json_set(out, "identity", pair);
    json_free(r); return out;
}
/* PoP: ewf2_compression_history @ persistence/sqlite/workers/executions.py:compression_history */
static json_t *ewf2_compression_history(ewf2_ctx_t *ctx, const json_t *a) {
    json_t *payload = json_object();
    const char *keys[] = {"target_session_id", "include_ancestors", "include_inactive",
                          "repair_alternation", "include_row_ids", "include_compacted"};
    const char *outk[] = {"target", "include_ancestors", "include_inactive",
                          "repair_alternation", "include_row_ids", "include_compacted"};
    for (size_t i = 0; i < 6; i++) {
        const json_t *v = ewf2_get(a, keys[i]);
        json_set(payload, outk[i], v ? json_copy(v) : json_null());
    }
    json_t *r = ewf2_call(ctx, "_persist", ewf2_seam_args(a, "compression.history", payload));
    if (ewf2_is_raise(r)) return r;
    const json_t *msgs = ewf2_get(r, "messages");
    json_t *o = msgs ? json_copy(msgs) : json_array();
    json_free(r); return o; /* tuple(...) -> array */
}
/* PoP: ewf2_dispatch_delegation @ persistence/sqlite/workers/executions.py:dispatch_delegation */
static json_t *ewf2_dispatch_delegation(ewf2_ctx_t *ctx, const json_t *a) {
    json_t *r = ewf2_call(ctx, "_ledger",
        ewf2_ledger_args(a, "delegation.dispatch",
                         ewf2_delegation_payload(ewf2_get(a, "dispatch"))));
    if (ewf2_is_raise(r)) return r;
    json_free(r); return json_null();
}
/* PoP: ewf2_complete_delegation @ persistence/sqlite/workers/executions.py:complete_delegation */
static json_t *ewf2_complete_delegation(ewf2_ctx_t *ctx, const json_t *a) {
    json_t *r = ewf2_call(ctx, "_ledger",
        ewf2_ledger_args(a, "delegation.complete",
                         ewf2_delegation_payload(ewf2_get(a, "completion"))));
    if (ewf2_is_raise(r)) return r;
    json_free(r); return json_null();
}
/* PoP: ewf2_record_delegation_child @ persistence/sqlite/workers/executions.py:record_delegation_child */
static json_t *ewf2_record_delegation_child(ewf2_ctx_t *ctx, const json_t *a) {
    json_t *r = ewf2_call(ctx, "_ledger",
        ewf2_ledger_args(a, "delegation.child",
                         ewf2_delegation_payload(ewf2_get(a, "child"))));
    if (ewf2_is_raise(r)) return r;
    json_free(r); return json_null();
}
/* PoP: ewf2_read_delegation @ persistence/sqlite/workers/executions.py:read_delegation */
static json_t *ewf2_read_delegation(ewf2_ctx_t *ctx, const json_t *a) {
    return ewf2_call(ctx, "_ledger",
        ewf2_ledger_args(a, "delegation.read",
                         ewf2_delegation_payload(ewf2_get(a, "delegation_id"))));
    /* WorkerDelegationView(**ret) -> field dict (identity in fixture domain). */
}
/* PoP: ewf2_withdraw_delegation @ persistence/sqlite/workers/executions.py:withdraw_delegation */
static json_t *ewf2_withdraw_delegation(ewf2_ctx_t *ctx, const json_t *a) {
    json_t *r = ewf2_call(ctx, "_ledger",
        ewf2_ledger_args(a, "delegation.unscheduled",
                         ewf2_delegation_payload(ewf2_get(a, "delegation_id"))));
    if (ewf2_is_raise(r)) return r;
    json_free(r); return json_null();
}
/* PoP: ewf2_claim_delegation_delivery @ persistence/sqlite/workers/executions.py:claim_delegation_delivery */
static json_t *ewf2_claim_delegation_delivery(ewf2_ctx_t *ctx, const json_t *a) {
    return ewf2_delivery(ctx, "claim", a);
}
/* PoP: ewf2_ack_delegation_delivery @ persistence/sqlite/workers/executions.py:ack_delegation_delivery */
static json_t *ewf2_ack_delegation_delivery(ewf2_ctx_t *ctx, const json_t *a) {
    return ewf2_delivery(ctx, "ack", a);
}
/* PoP: ewf2_drop_delegation_delivery @ persistence/sqlite/workers/executions.py:drop_delegation_delivery */
static json_t *ewf2_drop_delegation_delivery(ewf2_ctx_t *ctx, const json_t *a) {
    return ewf2_delivery(ctx, "drop", a);
}
/* PoP: ewf2_defer_delegation_delivery @ persistence/sqlite/workers/executions.py:defer_delegation_delivery */
static json_t *ewf2_defer_delegation_delivery(ewf2_ctx_t *ctx, const json_t *a) {
    return ewf2_delivery(ctx, "defer", a);
}
/* PoP: ewf2_release_delegation_delivery @ persistence/sqlite/workers/executions.py:release_delegation_delivery */
static json_t *ewf2_release_delegation_delivery(ewf2_ctx_t *ctx, const json_t *a) {
    return ewf2_delivery(ctx, "release", a);
}
