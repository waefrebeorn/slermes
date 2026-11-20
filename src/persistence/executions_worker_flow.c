/*
 * executions_worker_flow.c — FLOW port of the worker execution repository
 * verbs in persistence/sqlite/workers/executions.py (hermes-agent, READ-ONLY).
 *
 * A CRLF-aware PoP cross-check confirmed these defs are genuinely UNCLAIMED.
 * This is a decision-sequence port: each verb builds its wire payload from
 * typed parameters, runs ONE receipt transaction through the universal seam,
 * and converts the stored result to its contract record. The SQLite/gateway
 * call itself is NEVER real I/O — it is always a scripted seam.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All helpers are static and ewf_-prefixed.
 *
 * Universal seam (ewf_call): name is one of "_persist" / "_value"; args =
 * {operation, payload, epoch, execution_id, session_id, generation, sequence}.
 * "_value" is the ['value'] projection of "_persist"; the seam script returns
 * the raw result of that wrapper. A script is in.io[name]: a plain value
 * (every call), a JSON ARRAY (a per-call sequence, fail-closed on exhaustion),
 * or {"__raise__": cls, "message": m} (a raised exception). A missing script
 * is fail-closed RuntimeError. Every call appends ONE ordered trace event
 * {"name","args","ret"} (ret is the value or the raise descriptor).
 *
 * Representation contract (shared with tests/oracle_executions_worker_flow.py):
 * - NULL json_t* == absent key; a JSON_NULL node is a real null VALUE.
 * - An exception is {"__raise__": cls, "message": str(exc)}; cls is
 *   canonicalized through the FULL builtin hierarchy (IOError->OSError etc.).
 * - Python numbers are JSON numbers; bools are JSON bools; None is null.
 * - Type constructors (WorkerExecutionId, SessionId, WorkerKind,
 *   AuthorityEpoch, ExecutionGeneration, WorkerStatus, TitleSource,
 *   SessionLifecycleStatus, WorkerExecutionRecord, WorkerTitleResult,
 *   WorkerAppendResult, RowAnnotation) are projected to their FIELD SHAPE
 *   (enums/NewTypes are identity, records are field dicts in declaration
 *   order). Constructor VALUE validation is outside the fixture domain.
 * - Fixture domain: int()/str() inputs are numbers, bools, digit strings or
 *   null; getattr-enum sources are strings or {"__enum__": value} doubles;
 *   Mapping payloads are JSON objects; annotation rows are objects.
 * - last_sequence in _record is a non-negative number (no validation fired).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── context ────────────────────────────────────────────────────────────── */
typedef struct ewf_ctx {
    const json_t *io;      /* borrowed: seam scripts (in.io) */
    json_t *trace;         /* owned: ordered seam events */
    json_t *cursors;       /* owned: per-name sequence cursors */
} ewf_ctx_t;

static int ewf_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static json_t *ewf_get(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}
static const char *ewf_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool ewf_str_eq(const json_t *v, const char *s) {
    const char *t = ewf_str(v);
    return t && s && strcmp(t, s) == 0;
}

/* ── FULL builtin exception hierarchy (py3.14) ─────────────────────────── */
static const char *ewf_exc_canon(const char *c) {
    if (ewf_eq(c, "IOError") || ewf_eq(c, "EnvironmentError") ||
        ewf_eq(c, "WindowsError")) return "OSError";
    return c;
}
static const char *ewf_exc_parent(const char *c) {
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
    const char *cc = ewf_exc_canon(c);
    for (size_t i = 0; T[i].c; i++) if (ewf_eq(T[i].c, cc)) return T[i].p;
    return "Exception";
}
/* Is `kind` the same as or a descendant of `base`? */
static bool ewf_exc_is(const char *kind, const char *base) {
    const char *k = ewf_exc_canon(kind);
    for (int d = 0; k && d < 32; d++) {
        if (ewf_eq(k, base)) return true;
        k = ewf_exc_parent(k);
    }
    return false;
}

/* ── raise descriptors ─────────────────────────────────────────────────── */
static json_t *ewf_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", ewf_exc_canon(type ? type : "Exception"));
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static bool ewf_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *ewf_raise_cls(const json_t *v) {
    return json_get_str(v, "__raise__", "");
}
static const char *ewf_raise_msg(const json_t *v) {
    return json_get_str(v, "message", "");
}

/* ── Python truthiness / value helpers (fixture domain) ────────────────── */
static bool ewf_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static json_t *ewf_bool(const json_t *v) { return json_bool(ewf_truthy(v)); }

/* repr() of a Python str for int() error messages. */
static char *ewf_repr_str(const char *s) {
    if (!s) s = "";
    char q = '\'';
    if (strchr(s, '\'') && !strchr(s, '"')) q = '"';
    size_t cap = strlen(s) * 4 + 3, len = 0;
    char *o = malloc(cap);
    if (!o) return NULL;
    o[len++] = q;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        if (c == (unsigned char)q || c == '\\') { o[len++] = '\\'; o[len++] = (char)c; }
        else if (c == '\n') { o[len++] = '\\'; o[len++] = 'n'; }
        else if (c == '\r') { o[len++] = '\\'; o[len++] = 'r'; }
        else if (c == '\t') { o[len++] = '\\'; o[len++] = 't'; }
        else o[len++] = (char)c;
    }
    o[len++] = q; o[len] = '\0';
    return o;
}

/* int(v) over the documented domain. Returns a number or a raise descriptor. */
static bool ewf_ascii_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
static json_t *ewf_int(const json_t *v) {
    if (v && v->type == JSON_NUMBER) return json_number(trunc(v->num_val));
    if (v && v->type == JSON_BOOL) return json_number(v->bool_val ? 1.0 : 0.0);
    if (v && v->type == JSON_STRING) {
        const char *s = v->str_val;
        while (*s && ewf_ascii_ws((unsigned char)*s)) s++;
        const char *end = s + strlen(s);
        while (end > s && ewf_ascii_ws((unsigned char)end[-1])) end--;
        double sign = 1.0;
        if (s < end && (*s == '+' || *s == '-')) { if (*s == '-') sign = -1.0; s++; }
        double acc = 0.0; bool any = false;
        for (const char *p = s; p < end; p++) {
            if (*p < '0' || *p > '9') { any = false; break; }
            acc = acc * 10.0 + (*p - '0'); any = true;
        }
        if (any) return json_number(sign * acc);
        char *r = ewf_repr_str(v->str_val);
        char msg[512];
        snprintf(msg, sizeof msg, "invalid literal for int() with base 10: %s", r ? r : "''");
        free(r);
        return ewf_err("ValueError", msg);
    }
    const char *tn = (!v || v->type == JSON_NULL) ? "NoneType" :
        (v->type == JSON_ARRAY ? "list" : v->type == JSON_OBJECT ? "dict" : "object");
    char msg[256];
    snprintf(msg, sizeof msg,
             "int() argument must be a string, a bytes-like object or a real number, not '%s'", tn);
    return ewf_err("TypeError", msg);
}

/* str(v) over the documented domain. */
static json_t *ewf_pystr(const json_t *v) {
    if (v && v->type == JSON_STRING) return json_copy(v);
    if (v && v->type == JSON_BOOL) return json_string(v->bool_val ? "True" : "False");
    if (v && v->type == JSON_NUMBER) {
        char b[64];
        if (v->num_val == trunc(v->num_val)) snprintf(b, sizeof b, "%.0f", v->num_val);
        else snprintf(b, sizeof b, "%.17g", v->num_val);
        return json_string(b);
    }
    if (!v || v->type == JSON_NULL) return json_string("None");
    return json_string("");
}

/* getattr(src, 'value', src): an {"__enum__": v} double yields v; otherwise
 * the source itself (a str / null / scalar). */
static json_t *ewf_getattr_value(const json_t *v) {
    if (v && v->type == JSON_OBJECT) {
        const json_t *ev = json_obj_get(v, "__enum__");
        if (ev) return json_copy(ev);
    }
    return v ? json_copy(v) : json_null();
}

/* ── the one universal seam ─────────────────────────────────────────────── */
static json_t *ewf_call(ewf_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());   /* adopts args */
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    json_t *ret = NULL;
    const json_t *used = entry;
    if (!entry) {
        char m[256]; snprintf(m, sizeof m, "no script for %s", name);
        ret = ewf_err("RuntimeError", m);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char m[256]; snprintf(m, sizeof m, "script exhausted for %s", name);
            ret = ewf_err("RuntimeError", m);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT) {
            const json_t *rs = json_obj_get(used, "__raise__");
            if (rs) {
                ret = ewf_err(json_get_str(used, "__raise__", "Exception"),
                              json_get_str(used, "message", ""));
            } else {
                const json_t *sc = json_obj_get(used, "__script__");
                if (sc && sc->type == JSON_BOOL && sc->bool_val) {
                    const json_t *rse = json_obj_get(used, "raise");
                    if (rse && rse->type == JSON_OBJECT)
                        ret = ewf_err(json_get_str(rse, "__raise__", "Exception"),
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

/* Every receipt verb passes these five identity fields to the seam. */
static json_t *ewf_base_args(const json_t *a) {
    json_t *o = json_object();
    const char *keys[] = {"epoch", "execution_id", "session_id", "generation", "sequence"};
    for (size_t i = 0; i < 5; i++) {
        const json_t *v = ewf_get(a, keys[i]);
        json_set(o, keys[i], v ? json_copy(v) : json_null());
    }
    return o;
}
/* Build the full seam args {identity..., operation, payload}. */
static json_t *ewf_seam_args(const json_t *a, const char *operation, json_t *payload) {
    json_t *o = ewf_base_args(a);
    json_set_hdr(o, "operation", operation);
    json_set(o, "payload", payload ? payload : json_object());
    return o;
}
static json_t *ewf_payload_copy(const json_t *a) {
    const json_t *p = ewf_get(a, "payload");
    return p ? json_copy(p) : json_object();
}
static char *ewf_op(const json_t *a) {
    const char *s = json_get_str(a, "operation", "");
    char *o = malloc(strlen(s) + 1);
    if (o) strcpy(o, s);
    return o;
}

/* Set-of-keys validation: set(keys) - allowed, then required - set(keys). */
static bool ewf_keys_ok(const json_t *keys, const char *const *allowed, size_t na,
                        const char *const *required, size_t nr) {
    if (!keys || keys->type != JSON_OBJECT) return keys && keys->type == JSON_NULL;
    for (size_t i = 0; i < keys->c.count; i++) {
        bool found = false;
        for (size_t j = 0; j < na; j++) if (ewf_eq(keys->c.keys[i], allowed[j])) { found = true; break; }
        if (!found) return false;
    }
    for (size_t j = 0; j < nr; j++) {
        bool found = false;
        for (size_t i = 0; i < keys->c.count; i++) if (ewf_eq(keys->c.keys[i], required[j])) { found = true; break; }
        if (!found) return false;
    }
    return true;
}

static const char *const EWF_MAIN_KEYS[] = {
    "input_tokens", "output_tokens", "cache_read_tokens", "cache_write_tokens",
    "reasoning_tokens", "api_call_count", "estimated_cost_usd", "actual_cost_usd",
    "cost_status", "cost_source", "pricing_version", "model", "billing_provider",
    "billing_base_url", "billing_mode", "absolute", "task", "source", "prompt_cache_clock"
};
static const char *const EWF_AUX_KEYS[] = {
    "input_tokens", "output_tokens", "cache_read_tokens", "cache_write_tokens",
    "reasoning_tokens", "api_call_count", "estimated_cost_usd", "model",
    "billing_provider", "billing_base_url"
};
static const char *const EWF_CREATE_KEYS[] = {
    "source", "model", "model_config", "system_prompt", "user_id", "session_key",
    "chat_id", "chat_type", "thread_id", "parent_session_id", "cwd", "profile_name",
    "git_repo_root", "origin_json", "display_name"
};
static const char *const EWF_CREATE_REQ[] = {"source"};
static const char *const EWF_APPEND_KEYS[] = {"turn_lease_holder"};
static const char *const EWF_ANNOTATION_KEYS[] = {
    "_row_id", "timestamp", "_db_row_snapshot", "_canonical_row", "message_uid",
    "_tool_call_uids", "_tool_call_uid"
};

/* _record: field dict in declaration order (constructors are identity). */
static json_t *ewf_record_of(const json_t *pub) {
    json_t *r = json_object();
    const char *keys[] = {"execution_id", "session_id", "kind", "owner_epoch",
                          "generation", "status", "last_sequence"};
    for (size_t i = 0; i < 7; i++) {
        const json_t *v = ewf_get(pub, keys[i]);
        json_set(r, keys[i], v ? json_copy(v) : json_null());
    }
    return r;
}
/* _annotations: tuple(RowAnnotation.from_wire(a)) -> array of 7-field dicts. */
static json_t *ewf_annotations_of(const json_t *result) {
    json_t *out = json_array();
    const json_t *anns = ewf_get(result, "annotations");
    if (anns && anns->type == JSON_ARRAY) {
        for (size_t i = 0; i < anns->c.count; i++) {
            const json_t *a = json_get(anns, i);
            json_t *row = json_object();
            for (size_t k = 0; k < 7; k++) {
                const json_t *v = ewf_get(a, EWF_ANNOTATION_KEYS[k]);
                json_set(row, EWF_ANNOTATION_KEYS[k], v ? json_copy(v) : json_null());
            }
            json_append(out, row);
        }
    }
    return out;
}
/* _messages: [dict(m) if isinstance(m, Mapping) else m for m in messages]. */
static json_t *ewf_messages_of(const json_t *msgs) {
    json_t *out = json_array();
    if (msgs && msgs->type == JSON_ARRAY) {
        for (size_t i = 0; i < msgs->c.count; i++) {
            const json_t *m = json_get(msgs, i);
            json_append(out, (m && m->type == JSON_OBJECT) ? json_copy(m) : (m ? json_copy(m) : json_null()));
        }
    }
    return out;
}

/* ── ported verbs ───────────────────────────────────────────────────────── */

/* PoP: ewf_key_set @ persistence/sqlite/workers/executions.py:_key_set */
static json_t *ewf_key_set(ewf_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *keys = ewf_get(args, "keys");
    json_t *allowed = ewf_get(args, "allowed") ? json_copy(ewf_get(args, "allowed")) : json_array();
    json_t *required = ewf_get(args, "required") ? json_copy(ewf_get(args, "required")) : json_array();
    size_t na = allowed->type == JSON_ARRAY ? json_len(allowed) : 0;
    size_t nr = required->type == JSON_ARRAY ? json_len(required) : 0;
    const char **aa = malloc((na + 1) * sizeof(char *));
    const char **rr = malloc((nr + 1) * sizeof(char *));
    size_t ca = 0, cr = 0;
    if (aa) for (size_t i = 0; i < na; i++) { const char *s = ewf_str(json_get(allowed, i)); if (s) aa[ca++] = s; }
    if (rr) for (size_t i = 0; i < nr; i++) { const char *s = ewf_str(json_get(required, i)); if (s) rr[cr++] = s; }
    bool ok = ewf_keys_ok(keys, aa, ca, rr, cr);
    free((void *)aa); free((void *)rr); json_free(allowed); json_free(required);
    if (!ok) return ewf_err("AuthorityRefusal", "invalid_params");
    return json_null();
}

/* PoP: ewf_record @ persistence/sqlite/workers/executions.py:_record */
static json_t *ewf_record(ewf_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return ewf_record_of(ewf_get(args, "public"));
}

/* PoP: ewf_messages @ persistence/sqlite/workers/executions.py:_messages */
static json_t *ewf_messages(ewf_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return ewf_messages_of(ewf_get(args, "messages"));
}

/* PoP: ewf_annotations @ persistence/sqlite/workers/executions.py:_annotations */
static json_t *ewf_annotations(ewf_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return ewf_annotations_of(ewf_get(args, "result"));
}

/* PoP: ewf_finish @ persistence/sqlite/workers/executions.py:finish */
static json_t *ewf_finish(ewf_ctx_t *ctx, const json_t *args) {
    json_t *r = ewf_call(ctx, "_persist", ewf_seam_args(args, "execution.finish", json_object()));
    if (ewf_is_raise(r)) return r;
    json_free(r); return json_null();
}

/* PoP: ewf_record_usage @ persistence/sqlite/workers/executions.py:record_usage */
static json_t *ewf_record_usage(ewf_ctx_t *ctx, const json_t *args) {
    const json_t *usage = ewf_get(args, "usage");
    if (!ewf_keys_ok(usage, EWF_MAIN_KEYS, sizeof(EWF_MAIN_KEYS) / sizeof(char *), NULL, 0))
        return ewf_err("AuthorityRefusal", "invalid_params");
    json_t *payload = usage ? json_copy(usage) : json_object();
    json_t *r = ewf_call(ctx, "_value", ewf_seam_args(args, "usage.main", payload));
    if (ewf_is_raise(r)) return r;
    json_free(r); return json_null();
}

/* PoP: ewf_record_auxiliary_usage @ persistence/sqlite/workers/executions.py:record_auxiliary_usage */
static json_t *ewf_record_auxiliary_usage(ewf_ctx_t *ctx, const json_t *args) {
    const json_t *usage = ewf_get(args, "usage");
    if (!ewf_keys_ok(usage, EWF_AUX_KEYS, sizeof(EWF_AUX_KEYS) / sizeof(char *), NULL, 0))
        return ewf_err("AuthorityRefusal", "invalid_params");
    json_t *payload = usage ? json_copy(usage) : json_object();
    const json_t *task = ewf_get(args, "task");
    json_set(payload, "task", task ? json_copy(task) : json_null());
    json_t *r = ewf_call(ctx, "_value", ewf_seam_args(args, "usage.auxiliary", payload));
    if (ewf_is_raise(r)) return r;
    json_free(r); return json_null();
}

/* PoP: ewf_acquire_turn_lease @ persistence/sqlite/workers/executions.py:acquire_turn_lease */
static json_t *ewf_acquire_turn_lease(ewf_ctx_t *ctx, const json_t *args) {
    const json_t *lease = ewf_get(args, "lease");
    json_t *payload = json_object();
    const json_t *h = ewf_get(lease, "holder"), *t = ewf_get(lease, "ttl_seconds");
    json_set(payload, "holder", h ? json_copy(h) : json_null());
    json_set(payload, "ttl_seconds", t ? json_copy(t) : json_null());
    json_t *r = ewf_call(ctx, "_value", ewf_seam_args(args, "turn.acquire", payload));
    if (ewf_is_raise(r)) return r;
    json_t *o = ewf_bool(r); json_free(r); return o;
}

/* PoP: ewf_cleanup_turn_lease @ persistence/sqlite/workers/executions.py:cleanup_turn_lease */
static json_t *ewf_cleanup_turn_lease(ewf_ctx_t *ctx, const json_t *args) {
    json_t *payload = json_object();
    const json_t *t = ewf_get(args, "target_session_id"), *h = ewf_get(args, "holder");
    json_set(payload, "target", t ? json_copy(t) : json_null());
    json_set(payload, "holder", h ? json_copy(h) : json_null());
    json_t *r = ewf_call(ctx, "_persist", ewf_seam_args(args, "turn.cleanup", payload));
    if (ewf_is_raise(r)) return r;
    json_free(r); return json_null();
}

/* PoP: ewf_set_session_title @ persistence/sqlite/workers/executions.py:set_session_title */
static json_t *ewf_set_session_title(ewf_ctx_t *ctx, const json_t *args) {
    json_t *payload = json_object();
    const json_t *t = ewf_get(args, "title");
    json_set(payload, "title", t ? json_copy(t) : json_null());
    json_t *sv = ewf_getattr_value(ewf_get(args, "source"));
    json_set(payload, "source", sv);
    json_t *r = ewf_call(ctx, "_persist", ewf_seam_args(args, "session.title", payload));
    if (ewf_is_raise(r)) return r;
    json_t *out = json_object();
    const json_t *v = ewf_get(r, "value");
    json_set(out, "value", ewf_bool(v));
    const json_t *er = ewf_get(r, "error");
    json_set(out, "error", er ? json_copy(er) : json_null());
    const json_t *tc = ewf_get(r, "title_candidate");
    json_set(out, "title_candidate", tc ? json_copy(tc) : json_null());
    json_free(r); return out;
}

/* PoP: ewf_set_session_title_source @ persistence/sqlite/workers/executions.py:set_session_title_source */
static json_t *ewf_set_session_title_source(ewf_ctx_t *ctx, const json_t *args) {
    json_t *payload = json_object();
    json_set(payload, "source", ewf_getattr_value(ewf_get(args, "source")));
    json_t *r = ewf_call(ctx, "_value", ewf_seam_args(args, "session.title_source", payload));
    if (ewf_is_raise(r)) return r;
    json_t *o = ewf_bool(r); json_free(r); return o;
}

/* PoP: ewf_next_session_title @ persistence/sqlite/workers/executions.py:next_session_title */
static json_t *ewf_next_session_title(ewf_ctx_t *ctx, const json_t *args) {
    json_t *payload = json_object();
    const json_t *b = ewf_get(args, "base_title");
    json_set(payload, "base_title", b ? json_copy(b) : json_null());
    json_t *r = ewf_call(ctx, "_value", ewf_seam_args(args, "session.next_title", payload));
    if (ewf_is_raise(r)) return r;
    json_t *o = ewf_pystr(r); json_free(r); return o;
}

/* PoP: ewf_set_session_billing_route @ persistence/sqlite/workers/executions.py:set_session_billing_route */
static json_t *ewf_set_session_billing_route(ewf_ctx_t *ctx, const json_t *args) {
    const json_t *route = ewf_get(args, "route");
    json_t *payload = json_object();
    const char *rk[] = {"provider", "base_url", "billing_mode"};
    for (size_t i = 0; i < 3; i++) {
        const json_t *v = ewf_get(route, rk[i]);
        json_set(payload, rk[i], v ? json_copy(v) : json_null());
    }
    json_t *r = ewf_call(ctx, "_value", ewf_seam_args(args, "session.billing_route", payload));
    if (ewf_is_raise(r)) return r;
    json_free(r); return json_null();
}

/* PoP: ewf_set_latest_user_api_content @ persistence/sqlite/workers/executions.py:set_latest_user_api_content */
static json_t *ewf_set_latest_user_api_content(ewf_ctx_t *ctx, const json_t *args) {
    json_t *payload = json_object();
    const json_t *c = ewf_get(args, "content"), *a = ewf_get(args, "api_content");
    json_set(payload, "content", c ? json_copy(c) : json_null());
    json_set(payload, "api_content", a ? json_copy(a) : json_null());
    json_t *r = ewf_call(ctx, "_value", ewf_seam_args(args, "session.api_content", payload));
    if (ewf_is_raise(r)) return r;
    json_t *o = ewf_int(r); json_free(r); return o;
}

/* PoP: ewf_set_message_api_content @ persistence/sqlite/workers/executions.py:set_message_api_content */
static json_t *ewf_set_message_api_content(ewf_ctx_t *ctx, const json_t *args) {
    json_t *payload = json_object();
    const char *pk[] = {"row_id", "content", "api_content"};
    for (size_t i = 0; i < 3; i++) {
        const json_t *v = ewf_get(args, pk[i]);
        json_set(payload, pk[i], v ? json_copy(v) : json_null());
    }
    json_t *r = ewf_call(ctx, "_value", ewf_seam_args(args, "session.message_api_content", payload));
    if (ewf_is_raise(r)) return r;
    json_t *o = ewf_int(r); json_free(r); return o;
}

/* PoP: ewf_set_user_message_content @ persistence/sqlite/workers/executions.py:set_user_message_content */
static json_t *ewf_set_user_message_content(ewf_ctx_t *ctx, const json_t *args) {
    json_t *payload = json_object();
    const json_t *rid = ewf_get(args, "row_id"), *c = ewf_get(args, "content");
    json_set(payload, "row_id", rid ? json_copy(rid) : json_null());
    json_set(payload, "content", c ? json_copy(c) : json_null());
    const json_t *dm = ewf_get(args, "display_metadata");
    if (dm && dm->type == JSON_OBJECT)
        json_set(payload, "display_metadata", json_copy(dm));
    json_t *r = ewf_call(ctx, "_value", ewf_seam_args(args, "session.user_content", payload));
    if (ewf_is_raise(r)) return r;
    json_t *o = ewf_int(r); json_free(r); return o;
}

/* PoP: ewf_create_session @ persistence/sqlite/workers/executions.py:create_session */
static json_t *ewf_create_session(ewf_ctx_t *ctx, const json_t *args) {
    const json_t *fields = ewf_get(args, "fields");
    if (!ewf_keys_ok(fields, EWF_CREATE_KEYS, sizeof(EWF_CREATE_KEYS) / sizeof(char *),
                     EWF_CREATE_REQ, sizeof(EWF_CREATE_REQ) / sizeof(char *)))
        return ewf_err("AuthorityRefusal", "invalid_params");
    json_t *payload = fields ? json_copy(fields) : json_object();
    json_t *r = ewf_call(ctx, "_value", ewf_seam_args(args, "session.create", payload));
    if (ewf_is_raise(r)) return r;
    json_t *o = json_copy(r); json_free(r); return o; /* SessionId identity */
}

/* PoP: ewf_session_lifecycle @ persistence/sqlite/workers/executions.py:session_lifecycle */
static json_t *ewf_session_lifecycle(ewf_ctx_t *ctx, const json_t *args) {
    json_t *r = ewf_call(ctx, "_value", ewf_seam_args(args, "session.lifecycle", json_object()));
    if (ewf_is_raise(r)) return r;
    return r; /* enum identity (fixture domain = valid member values) */
}

/* PoP: ewf_patch_session_sidecars @ persistence/sqlite/workers/executions.py:patch_session_sidecars */
static json_t *ewf_patch_session_sidecars(ewf_ctx_t *ctx, const json_t *args) {
    json_t *payload = json_object();
    const json_t *p = ewf_get(args, "patch");
    json_set(payload, "patch", (p && p->type == JSON_OBJECT) ? json_copy(p) : (p ? json_copy(p) : json_null()));
    json_t *r = ewf_call(ctx, "_value", ewf_seam_args(args, "session.sidecars", payload));
    if (ewf_is_raise(r)) return r;
    json_free(r); return json_null();
}

/* PoP: ewf_set_session_tool_pin @ persistence/sqlite/workers/executions.py:set_session_tool_pin */
static json_t *ewf_set_session_tool_pin(ewf_ctx_t *ctx, const json_t *args) {
    json_t *payload = json_object();
    const json_t *p = ewf_get(args, "tool_pin");
    json_set(payload, "tool_names", (p && p->type == JSON_OBJECT) ? json_copy(p) : (p ? json_copy(p) : json_null()));
    json_t *r = ewf_call(ctx, "_value", ewf_seam_args(args, "session.tools", payload));
    if (ewf_is_raise(r)) return r;
    json_free(r); return json_null();
}

/* PoP: ewf_append_transcript @ persistence/sqlite/workers/executions.py:append_transcript */
static json_t *ewf_append_transcript(ewf_ctx_t *ctx, const json_t *args) {
    const json_t *opts = ewf_get(args, "options");
    if (!ewf_keys_ok(opts, EWF_APPEND_KEYS, sizeof(EWF_APPEND_KEYS) / sizeof(char *), NULL, 0))
        return ewf_err("AuthorityRefusal", "invalid_params");
    json_t *payload = json_object();
    json_set(payload, "messages", ewf_messages_of(ewf_get(args, "messages")));
    if (opts && opts->type == JSON_OBJECT)
        for (size_t i = 0; i < opts->c.count; i++)
            json_set(payload, opts->c.keys[i], json_copy(opts->c.items[i]));
    json_t *r = ewf_call(ctx, "_persist", ewf_seam_args(args, "transcript.append", payload));
    if (ewf_is_raise(r)) return r;
    json_t *out = json_object();
    const json_t *cnt = ewf_get(r, "count");
    json_set(out, "count", cnt ? json_copy(cnt) : json_null());
    json_set(out, "annotations", ewf_annotations_of(r));
    json_free(r); return out;
}

/* PoP: ewf_compression_watermark @ persistence/sqlite/workers/executions.py:compression_watermark */
static json_t *ewf_compression_watermark(ewf_ctx_t *ctx, const json_t *args) {
    json_t *r = ewf_call(ctx, "_value", ewf_seam_args(args, "compression.watermark", json_object()));
    if (ewf_is_raise(r)) return r;
    json_t *o = ewf_int(r); json_free(r); return o;
}

/* PoP: ewf_compression_boundary @ persistence/sqlite/workers/executions.py:compression_boundary */
static json_t *ewf_compression_boundary(ewf_ctx_t *ctx, const json_t *args) {
    const json_t *peer = ewf_get(args, "peer");
    json_t *payload = json_object();
    const json_t *sk = ewf_get(peer, "session_key"), *src = ewf_get(peer, "source");
    json_set(payload, "session_key", sk ? json_copy(sk) : json_null());
    json_set(payload, "source", src ? json_copy(src) : json_null());
    json_t *r = ewf_call(ctx, "_value", ewf_seam_args(args, "compression.boundary", payload));
    if (ewf_is_raise(r)) return r; /* int|None identity */
    return r;
}

/* PoP: ewf_persist_operation @ persistence/sqlite/workers/executions.py:persist_operation */
static json_t *ewf_persist_operation(ewf_ctx_t *ctx, const json_t *args) {
    char *op = ewf_op(args);
    json_t *r = ewf_call(ctx, "_persist", ewf_seam_args(args, op, ewf_payload_copy(args)));
    if (!ewf_is_raise(r)) { free(op); return r; }
    const char *cls = ewf_raise_cls(r);
    /* except RuntimeError: re-raise unless compression.publish + exact RuntimeError */
    if (!ewf_exc_is(cls, "RuntimeError") || !ewf_eq(op, "compression.publish") ||
        !ewf_eq(ewf_exc_canon(cls), "RuntimeError")) {
        free(op); return r;
    }
    /* raise CompressionPublishRefused(session_id, str(exc)) */
    const char *msg = ewf_raise_msg(r);
    json_t *e = ewf_err("CompressionPublishRefused", msg);
    json_free(r); free(op); return e;
}
