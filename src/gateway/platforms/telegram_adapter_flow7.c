/*
 * telegram_adapter_flow7.c - Telegram adapter FLOW part 7 of
 * plugins/platforms/telegram/adapter.py (hermes-agent): the ERROR -
 * CLASSIFICATION / exception-graph / sender-attribution decision surface left
 * UNCLAIMED after telegram_adapter_flow{,2,3,6}.c.  Ported targets (all
 * PoP-claimed here, all confirmed UNCLAIMED before this file was written):
 *   _iter_exception_graph, _exception_graph_matches, _looks_like_auth_error,
 *   _looks_like_network_error, _looks_like_connect_timeout,
 *   _looks_like_pool_timeout, _is_thread_not_found_error, _is_bad_request_error,
 *   _is_reply_anchor_missing_error, _is_timed_out, _is_rich_capability_error,
 *   _is_rich_fallback_error, _should_retry_without_dm_topic_reply_anchor,
 *   _is_own_message, _sender_is_other_bot, _redact_telegram_error_text.
 *
 * Decision sequences only: every live I/O / injected callable enters through
 * ONE universal seam: ta7_call(ctx, name, args).  The harness wires it to a
 * recording stub fed the case's scripted results (in.io); the oracle installs
 * recording stubs emitting the identical event shape {"name","args","ret"}
 * from the SAME scripts.  Claimed siblings / unported collaborators are
 * seamed; pure leaves (exception-graph DFS, class-name/text predicates) are
 * transcribed inline (no seam, no PoP re-claim).
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and ta7_-prefixed.
 *
 * SEAM NAME SET (must equal oracle SEAM_NAMES exactly):
 *   _exception_graph_matches, _dm_topic_fallback, _is_bad_request_error,
 *   _is_rich_capability_error, _is_reply_anchor_missing_error,
 *   describe_proxy_error, redact_sensitive_text.
 *
 * Representation contract (shared with tests/oracle_telegram_adapter_flow7.py):
 * - NULL json_t* == Python None / absent; a JSON_NULL node is a real null
 *   VALUE.  Exception specs: {"__raise__": cls, "message": str(exc)}.
 * - in.errors: id -> {"class","message","base", ...attrs,
 *   "cause"?:eid, "context"?:eid}.  Error arguments are eids; seam args that
 *   carry a live exception use its canon spec {"__raise__","message"}.
 * - Adapter state (in.state) carries the attribute-double fields; a DIRECT
 *   attribute read (self._bot, ...) raises AttributeError
 *   "'TelegramAdapter' object has no attribute '<k>'" when ABSENT.  getattr
 *   with a default reads json_obj_get with default (no raise).
 * - Nested doubles (message / from_user / metadata) are plain attribute
 *   doubles (SimpleNamespace on the oracle side).
 * - Seam scripts: a plain value repeats; a LIST is consumed per call (last
 *   value repeats); a {"__script__":true,"ret":v,"raise":{type,message}}
 *   wrapper forces a result or raise.  Missing / exhausted script => fail-closed
 *   RuntimeError recorded in the trace before propagation.
 * - Except sites swallow ONLY their named families via the FULL builtin
 *   exception hierarchy (TA7_EXC_HIER); BaseException-only kinds
 *   (SystemExit/KeyboardInterrupt/GeneratorExit/BaseExceptionGroup) pass
 *   through every except-Exception site.
 * - All `from telegram.error import ...` sites take the ImportError fallback
 *   branch (telegram is not importable in the oracle namespace), so only the
 *   class-name / message / builtin-OSError predicates are transcribed.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── universal seam context ── */
typedef struct ta7_ctx {
    json_t *state;        /* owned; carries the attribute-double fields */
    const json_t *io;     /* borrowed scripted seam results */
    json_t *cursors;      /* owned; per-name array cursors */
    json_t *trace;        /* owned; ordered seam events */
    const json_t *errors; /* borrowed exception-graph registry */
} ta7_ctx_t;

/* ── basic helpers ── */
static bool ta7_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *ta7_raise_kind(const json_t *v) {
    return ta7_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static bool ta7_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool ta7_present(const json_t *v) { return v && v->type != JSON_NULL; }
static bool ta7_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *ta7_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *ta7_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static const char *ta7_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (d == floor(d) && fabs(d) < 1e16) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}
static const json_t *ta7_arg(const json_t *args, const char *key) {
    return args ? json_obj_get(args, key) : NULL;
}
static const json_t *ta7_getattr(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
static const json_t *ta7_attr(const json_t *obj, const char *key,
                              const char *tname, json_t **err) {
    char msg[220];
    if (!obj || obj->type == JSON_NULL) {
        snprintf(msg, sizeof msg, "'NoneType' object has no attribute '%s'", key);
        *err = ta7_err("AttributeError", msg);
        return NULL;
    }
    if (obj->type != JSON_OBJECT) {
        snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", ta7_type_name(obj), key);
        *err = ta7_err("AttributeError", msg);
        return NULL;
    }
    const json_t *v = json_obj_get(obj, key);
    if (!v) {
        snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", tname, key);
        *err = ta7_err("AttributeError", msg);
        return NULL;
    }
    return v;
}

/* ── string helpers ── */
static char *ta7_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static char *ta7_lower(const char *s) {
    char *o = ta7_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}

/* ── FULL builtin exception hierarchy (CPython 3.14) ── */
typedef struct { const char *name; const char *parent; } ta7_exc_row_t;
static const ta7_exc_row_t TA7_EXC_HIER[] = {
    {"ArithmeticError", "Exception"}, {"AssertionError", "Exception"},
    {"AttributeError", "Exception"}, {"BaseException", "object"},
    {"BaseExceptionGroup", "BaseException"}, {"BlockingIOError", "OSError"},
    {"BrokenPipeError", "ConnectionError"}, {"BufferError", "Exception"},
    {"BytesWarning", "Warning"}, {"ChildProcessError", "OSError"},
    {"ConnectionAbortedError", "ConnectionError"}, {"ConnectionError", "OSError"},
    {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
    {"DeprecationWarning", "Warning"}, {"EOFError", "Exception"},
    {"EncodingWarning", "Warning"}, {"Exception", "BaseException"},
    {"ExceptionGroup", "BaseExceptionGroup"}, {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"}, {"FloatingPointError", "ArithmeticError"},
    {"FutureWarning", "Warning"}, {"GeneratorExit", "BaseException"},
    {"ImportError", "Exception"}, {"ImportWarning", "Warning"},
    {"IndentationError", "SyntaxError"}, {"IndexError", "LookupError"},
    {"InterruptedError", "OSError"}, {"IsADirectoryError", "OSError"},
    {"KeyError", "LookupError"}, {"KeyboardInterrupt", "BaseException"},
    {"LookupError", "Exception"}, {"MemoryError", "Exception"},
    {"ModuleNotFoundError", "ImportError"}, {"NameError", "Exception"},
    {"NotADirectoryError", "OSError"}, {"NotImplementedError", "RuntimeError"},
    {"OSError", "Exception"}, {"OverflowError", "ArithmeticError"},
    {"PendingDeprecationWarning", "Warning"}, {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"}, {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"}, {"ReferenceError", "Exception"},
    {"ResourceWarning", "Warning"}, {"RuntimeError", "Exception"},
    {"RuntimeWarning", "Warning"}, {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"}, {"SyntaxError", "Exception"},
    {"SyntaxWarning", "Warning"}, {"SystemError", "Exception"},
    {"SystemExit", "BaseException"}, {"TabError", "IndentationError"},
    {"TimeoutError", "OSError"}, {"TypeError", "Exception"},
    {"UnboundLocalError", "NameError"}, {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"}, {"UnicodeError", "ValueError"},
    {"UnicodeTranslateError", "UnicodeError"}, {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"}, {"ValueError", "Exception"}, {"Warning", "Exception"},
    {"ZeroDivisionError", "ArithmeticError"}, {"_IncompleteInputError", "SyntaxError"},
};
static const char *ta7_builtin_parent(const char *name) {
    for (size_t i = 0; i < sizeof(TA7_EXC_HIER) / sizeof(TA7_EXC_HIER[0]); i++)
        if (strcmp(name, TA7_EXC_HIER[i].name) == 0) return TA7_EXC_HIER[i].parent;
    return NULL;
}
static bool ta7_chain_has(const char *start, const char *target) {
    const char *cur = start;
    for (int depth = 0; cur && depth < 24; depth++) {
        if (strcmp(cur, target) == 0) return true;
        cur = ta7_builtin_parent(cur);
    }
    return false;
}
static bool ta7_is_exception(const char *kind) {
    if (!kind) return false;
    if (ta7_builtin_parent(kind) == NULL) return true;
    return ta7_chain_has(kind, "Exception");
}

/* ── exception-graph registry ── */
static const json_t *ta7_err_obj(const ta7_ctx_t *ctx, const char *eid) {
    if (!eid || !ctx->errors || ctx->errors->type != JSON_OBJECT) return NULL;
    return json_obj_get(ctx->errors, eid);
}
static const char *ta7_err_class(const ta7_ctx_t *ctx, const char *eid) {
    const json_t *e = ta7_err_obj(ctx, eid);
    return e ? json_get_str(e, "class", "Exception") : NULL;
}
static const char *ta7_err_text(const ta7_ctx_t *ctx, const char *eid) {
    const json_t *e = ta7_err_obj(ctx, eid);
    return e ? json_get_str(e, "message", "") : "";
}
static const json_t *ta7_err_attr(const ta7_ctx_t *ctx, const char *eid, const char *key) {
    const json_t *e = ta7_err_obj(ctx, eid);
    return e ? json_obj_get(e, key) : NULL;
}
static const char *ta7_err_link(const ta7_ctx_t *ctx, const char *eid, const char *key) {
    return ta7_str(ta7_err_attr(ctx, eid, key));
}
/* canon(live exception) == {"__raise__": class, "message": str(exc)} */
static json_t *ta7_canon_err(const ta7_ctx_t *ctx, const char *eid) {
    if (!eid) return json_null();
    json_t *o = json_object();
    const char *cls = ta7_err_class(ctx, eid);
    json_set_hdr(o, "__raise__", cls ? cls : "Exception");
    json_set_hdr(o, "message", ta7_err_text(ctx, eid));
    return o;
}
/* DFS (cycle-safe) graph traversal; fills ids[] with eids in yield order.
 * stack.extend(cause, context) then pop -> context is visited before cause. */
static size_t ta7_graph(const ta7_ctx_t *ctx, const char *eid,
                        const char **ids, size_t cap) {
    const char *stack[256]; size_t sp = 0;
    const char *seen[256]; size_t ns = 0; size_t out = 0;
    if (eid && sp < 256) stack[sp++] = eid;
    while (sp && out < cap) {
        const char *cur = stack[--sp];
        bool dup = false;
        for (size_t i = 0; i < ns; i++) if (strcmp(seen[i], cur) == 0) { dup = true; break; }
        if (dup) continue;
        if (ns < 256) seen[ns++] = cur;
        ids[out++] = cur;
        const char *cause = ta7_err_link(ctx, cur, "cause");
        const char *context = ta7_err_link(ctx, cur, "context");
        if (cause && sp < 256) stack[sp++] = cause;
        if (context && sp < 256) stack[sp++] = context;
    }
    return out;
}

/* ── the one universal seam ──
 * args is ADOPTED into the trace event; the return is a fresh owned node. */
static json_t *ta7_call(ta7_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ta7_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ta7_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = ta7_err(json_get_str(raise, "type", "RuntimeError"),
                              json_get_str(raise, "message", ""));
            } else {
                json_t *r = json_obj_get(used, "ret");
                ret = r ? json_copy(r) : json_null();
            }
        } else {
            ret = used ? json_copy(used) : json_null();
        }
    }
    json_set(ev, "ret", json_copy(ret));
    json_append(ctx->trace, ev);
    return ret;
}

/* ═══════════════════════ ported flows ═══════════════════════ */

/* PoP: ta7_iter_exception_graph @ plugins/platforms/telegram/adapter.py:_iter_exception_graph */
static json_t *ta7_iter_exception_graph(ta7_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *eid = ta7_str(ta7_arg(args, "error"));
    const char *ids[256];
    size_t n = ta7_graph(ctx, eid, ids, 256);
    json_t *out = json_array();
    for (size_t i = 0; i < n; i++) {
        json_t *spec = json_object();
        const char *cls = ta7_err_class(ctx, ids[i]);
        json_set_hdr(spec, "__raise__", cls ? cls : "Exception");
        json_set_hdr(spec, "message", ta7_err_text(ctx, ids[i]));
        json_append(out, spec);
    }
    return out;
}

/* PoP: ta7_exception_graph_matches @ plugins/platforms/telegram/adapter.py:_exception_graph_matches */
static json_t *ta7_exception_graph_matches(ta7_ctx_t *ctx, const json_t *args) {
    const char *eid = ta7_str(ta7_arg(args, "error"));
    const char *nm = ta7_str(ta7_arg(args, "name_marker"));
    const json_t *tms = ta7_arg(args, "text_markers");
    const char *ids[256];
    size_t n = ta7_graph(ctx, eid, ids, 256);
    for (size_t i = 0; i < n; i++) {
        char *name_lo = ta7_lower(ta7_err_class(ctx, ids[i]) ? ta7_err_class(ctx, ids[i]) : "Exception");
        char *text_lo = ta7_lower(ta7_err_text(ctx, ids[i]));
        bool hit = nm && name_lo && strstr(name_lo, nm) != NULL;
        if (!hit && tms && tms->type == JSON_ARRAY) {
            for (size_t j = 0; j < json_len(tms); j++) {
                const char *m = ta7_str(json_get(tms, j));
                if (m && m[0] && text_lo && strstr(text_lo, m) != NULL) { hit = true; break; }
            }
        }
        free(name_lo); free(text_lo);
        if (hit) return json_bool(true);
    }
    (void)ctx;
    return json_bool(false);
}

/* PoP: ta7_looks_like_auth_error @ plugins/platforms/telegram/adapter.py:_looks_like_auth_error */
static json_t *ta7_looks_like_auth_error(ta7_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *eid = ta7_str(ta7_arg(args, "error"));
    char *nm = ta7_lower(ta7_err_class(ctx, eid) ? ta7_err_class(ctx, eid) : "Exception");
    bool hit = nm && (strcmp(nm, "invalidtoken") == 0 || strcmp(nm, "forbidden") == 0);
    free(nm);
    return json_bool(hit);
}

/* PoP: ta7_looks_like_network_error @ plugins/platforms/telegram/adapter.py:_looks_like_network_error */
static json_t *ta7_looks_like_network_error(ta7_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *eid = ta7_str(ta7_arg(args, "error"));
    char *nm = ta7_lower(ta7_err_class(ctx, eid) ? ta7_err_class(ctx, eid) : "Exception");
    static const char *const neg[] = {"badrequest", "invalidtoken", "forbidden", "retryafter"};
    static const char *const pos[] = {"networkerror", "timedout", "connectionerror"};
    bool neg_hit = false, pos_hit = false;
    for (size_t i = 0; i < 4; i++) if (nm && strcmp(nm, neg[i]) == 0) { neg_hit = true; break; }
    if (neg_hit) { free(nm); return json_bool(false); }
    for (size_t i = 0; i < 3; i++) if (nm && strcmp(nm, pos[i]) == 0) { pos_hit = true; break; }
    if (pos_hit) { free(nm); return json_bool(true); }
    free(nm);
    const char *cls = ta7_err_class(ctx, eid);
    return json_bool(cls && ta7_chain_has(cls, "OSError"));
}

/* PoP: ta7_looks_like_connect_timeout @ plugins/platforms/telegram/adapter.py:_looks_like_connect_timeout */
static json_t *ta7_looks_like_connect_timeout(ta7_ctx_t *ctx, const json_t *args) {
    const char *eid = ta7_str(ta7_arg(args, "error"));
    json_t *a = json_object();
    json_set(a, "error", ta7_canon_err(ctx, eid));
    json_set_hdr(a, "name_marker", "connecttimeout");
    json_t *tms = json_array();
    json_append(tms, json_string("connect timeout"));
    json_append(tms, json_string("connect timed out"));
    json_set(a, "text_markers", tms);
    return ta7_call(ctx, "_exception_graph_matches", a);
}

/* PoP: ta7_looks_like_pool_timeout @ plugins/platforms/telegram/adapter.py:_looks_like_pool_timeout */
static json_t *ta7_looks_like_pool_timeout(ta7_ctx_t *ctx, const json_t *args) {
    const char *eid = ta7_str(ta7_arg(args, "error"));
    const char *ids[256];
    size_t n = ta7_graph(ctx, eid, ids, 256);
    for (size_t i = 0; i < n; i++) {
        char *nm = ta7_lower(ta7_err_class(ctx, ids[i]) ? ta7_err_class(ctx, ids[i]) : "Exception");
        char *tx = ta7_lower(ta7_err_text(ctx, ids[i]));
        bool hit = nm && strstr(nm, "pooltimeout") != NULL;
        if (!hit && tx && strstr(tx, "pool timeout") != NULL) hit = true;
        if (!hit && tx && strstr(tx, "connection pool") != NULL && strstr(tx, "occupied") != NULL) hit = true;
        free(nm); free(tx);
        if (hit) return json_bool(true);
    }
    return json_bool(false);
}

/* PoP: ta7_is_thread_not_found_error @ plugins/platforms/telegram/adapter.py:_is_thread_not_found_error */
static json_t *ta7_is_thread_not_found_error(ta7_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *eid = ta7_str(ta7_arg(args, "error"));
    char *tx = ta7_lower(ta7_err_text(ctx, eid));
    bool hit = tx && strstr(tx, "thread not found") != NULL;
    free(tx);
    return json_bool(hit);
}

/* PoP: ta7_is_bad_request_error @ plugins/platforms/telegram/adapter.py:_is_bad_request_error */
static json_t *ta7_is_bad_request_error(ta7_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *eid = ta7_str(ta7_arg(args, "error"));
    char *nm = ta7_lower(ta7_err_class(ctx, eid) ? ta7_err_class(ctx, eid) : "Exception");
    size_t n = nm ? strlen(nm) : 0;
    bool hit = nm && (strcmp(nm, "badrequest") == 0 ||
                      (n >= 10 && strcmp(nm + n - 10, "badrequest") == 0));
    free(nm);
    return json_bool(hit);
}

/* PoP: ta7_is_reply_anchor_missing_error @ plugins/platforms/telegram/adapter.py:_is_reply_anchor_missing_error */
static json_t *ta7_is_reply_anchor_missing_error(ta7_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *eid = ta7_str(ta7_arg(args, "error"));
    char *tx = ta7_lower(ta7_err_text(ctx, eid));
    static const char *const markers[] = {"message to be replied not found", "reply message not found"};
    bool hit = false;
    for (size_t i = 0; i < 2; i++) if (tx && strstr(tx, markers[i]) != NULL) { hit = true; break; }
    free(tx);
    return json_bool(hit);
}

/* PoP: ta7_is_timed_out @ plugins/platforms/telegram/adapter.py:_is_timed_out */
static json_t *ta7_is_timed_out(ta7_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *eid = ta7_str(ta7_arg(args, "exc"));
    char *tx = ta7_lower(ta7_err_text(ctx, eid));
    bool hit = tx && strstr(tx, "timed out") != NULL;
    free(tx);
    return json_bool(hit);
}

/* PoP: ta7_is_rich_capability_error @ plugins/platforms/telegram/adapter.py:_is_rich_capability_error */
static json_t *ta7_is_rich_capability_error(ta7_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *eid = ta7_str(ta7_arg(args, "exc"));
    const char *cls = ta7_err_class(ctx, eid);
    char *nm = ta7_lower(cls ? cls : "Exception");
    bool hit = nm && (strcmp(nm, "endpointnotfound") == 0 || strcmp(nm, "invalidtoken") == 0);
    if (!hit && cls) {
        hit = ta7_chain_has(cls, "AttributeError") || ta7_chain_has(cls, "TypeError") ||
              ta7_chain_has(cls, "NotImplementedError");
    }
    const json_t *ec = ta7_err_attr(ctx, eid, "error_code");
    if (!hit && ec && ec->type == JSON_NUMBER && ec->num_val == 404) hit = true;
    char *tx = ta7_lower(ta7_err_text(ctx, eid));
    if (!hit && tx) {
        bool method = strstr(tx, "method") != NULL || strstr(tx, "endpoint") != NULL;
        bool gone = strstr(tx, "not found") != NULL || strstr(tx, "does not exist") != NULL;
        if (method && gone) hit = true;
        if (!hit && strstr(tx, "no such method") != NULL) hit = true;
    }
    free(nm); free(tx);
    return json_bool(hit);
}

/* PoP: ta7_is_rich_fallback_error @ plugins/platforms/telegram/adapter.py:_is_rich_fallback_error */
static json_t *ta7_is_rich_fallback_error(ta7_ctx_t *ctx, const json_t *args) {
    const char *eid = ta7_str(ta7_arg(args, "exc"));
    json_t *a1 = json_object();
    json_set(a1, "error", ta7_canon_err(ctx, eid));
    json_t *b1 = ta7_call(ctx, "_is_bad_request_error", a1);
    if (ta7_is_raise(b1)) return b1;
    if (ta7_truthy(b1)) { json_free(b1); return json_bool(true); }
    json_free(b1);
    json_t *a2 = json_object();
    json_set(a2, "exc", ta7_canon_err(ctx, eid));
    json_t *b2 = ta7_call(ctx, "_is_rich_capability_error", a2);
    if (ta7_is_raise(b2)) return b2;
    if (ta7_truthy(b2)) { json_free(b2); return json_bool(true); }
    json_free(b2);
    char *tx = ta7_lower(ta7_err_text(ctx, eid));
    bool hit = tx && (strstr(tx, "unsupported") != NULL || strstr(tx, "not implemented") != NULL);
    free(tx);
    return json_bool(hit);
}

/* PoP: ta7_should_retry_without_dm_topic_reply_anchor @ plugins/platforms/telegram/adapter.py:_should_retry_without_dm_topic_reply_anchor */
static json_t *ta7_should_retry_without_dm_topic_reply_anchor(ta7_ctx_t *ctx, const json_t *args) {
    const char *eid = ta7_str(ta7_arg(args, "error"));
    const json_t *meta = ta7_arg(args, "metadata");
    const json_t *rt = ta7_arg(args, "reply_to_message_id");
    json_t *a1 = json_object();
    json_set(a1, "metadata", meta ? json_copy(meta) : json_null());
    json_t *b1 = ta7_call(ctx, "_dm_topic_fallback", a1);
    if (ta7_is_raise(b1)) return b1;
    if (!ta7_truthy(b1)) { json_free(b1); return json_bool(false); }
    json_free(b1);
    json_t *a2 = json_object();
    json_set(a2, "error", ta7_canon_err(ctx, eid));
    json_t *b2 = ta7_call(ctx, "_is_bad_request_error", a2);
    if (ta7_is_raise(b2)) return b2;
    if (!ta7_truthy(b2)) { json_free(b2); return json_bool(false); }
    json_free(b2);
    if (ta7_is_null(rt)) return json_bool(false);
    json_t *a3 = json_object();
    json_set(a3, "error", ta7_canon_err(ctx, eid));
    return ta7_call(ctx, "_is_reply_anchor_missing_error", a3);
}

/* PoP: ta7_is_own_message @ plugins/platforms/telegram/adapter.py:_is_own_message */
static json_t *ta7_is_own_message(ta7_ctx_t *ctx, const json_t *args) {
    const json_t *msg = ta7_arg(args, "message");
    json_t *err = NULL;
    const json_t *bot = ta7_attr(ctx->state, "_bot", "TelegramAdapter", &err);
    if (err) return err;
    if (!ta7_truthy(bot)) return json_bool(false);
    const json_t *from_user = ta7_getattr(msg, "from_user");
    if (ta7_is_null(from_user)) return json_bool(false);
    const json_t *bot_id = ta7_getattr(bot, "id");
    const json_t *user_id = ta7_getattr(from_user, "id");
    bool ok = ta7_present(bot_id) && ta7_present(user_id) &&
              bot_id->type == user_id->type &&
              ((bot_id->type == JSON_NUMBER && bot_id->num_val == user_id->num_val) ||
               (bot_id->type == JSON_STRING && user_id->type == JSON_STRING &&
                strcmp(bot_id->str_val ? bot_id->str_val : "", user_id->str_val ? user_id->str_val : "") == 0));
    return json_bool(ok);
}

/* PoP: ta7_sender_is_other_bot @ plugins/platforms/telegram/adapter.py:_sender_is_other_bot */
static json_t *ta7_sender_is_other_bot(ta7_ctx_t *ctx, const json_t *args) {
    const json_t *msg = ta7_arg(args, "message");
    const json_t *sender = ta7_getattr(msg, "from_user");
    if (ta7_is_null(sender) || !ta7_truthy(ta7_getattr(sender, "is_bot"))) return json_bool(false);
    json_t *err = NULL;
    const json_t *bot = ta7_attr(ctx->state, "_bot", "TelegramAdapter", &err);
    if (err) return err;
    const json_t *bot_id = ta7_getattr(bot, "id");
    const json_t *sender_id = ta7_getattr(sender, "id");
    bool ok = ta7_is_null(bot_id) || ta7_is_null(sender_id) ||
              !(bot_id->type == sender_id->type &&
                ((bot_id->type == JSON_NUMBER && bot_id->num_val == sender_id->num_val) ||
                 (bot_id->type == JSON_STRING && sender_id->type == JSON_STRING &&
                  strcmp(bot_id->str_val ? bot_id->str_val : "", sender_id->str_val ? sender_id->str_val : "") == 0)));
    return json_bool(ok);
}

/* PoP: ta7_redact_telegram_error_text @ plugins/platforms/telegram/adapter.py:_redact_telegram_error_text */
static json_t *ta7_redact_telegram_error_text(ta7_ctx_t *ctx, const json_t *args) {
    const char *eid = ta7_str(ta7_arg(args, "error"));
    const json_t *proxy = ta7_arg(args, "proxy_url");
    const char *proxy_s = ta7_str(proxy);
    bool has_err = ta7_present(ta7_arg(args, "error"));
    if (proxy_s && proxy_s[0] && has_err) {
        json_t *a = json_object();
        json_set(a, "error", ta7_canon_err(ctx, eid));
        json_set(a, "proxy_url", proxy ? json_copy(proxy) : json_null());
        json_t *shown = ta7_call(ctx, "describe_proxy_error", a);
        if (ta7_is_raise(shown)) return shown;
        if (ta7_truthy(shown)) return shown;
        json_free(shown);
    }
    char *text = has_err ? ta7_strdup(ta7_err_text(ctx, eid)) : ta7_strdup("");
    if (!text || text[0] == '\0') {
        free(text);
        if (!has_err) return json_string("");
        const char *cls = ta7_err_class(ctx, eid);
        size_t n = cls ? strlen(cls) + 3 : 3;
        char *buf = malloc(n);
        if (buf) snprintf(buf, n, "<%s>", cls ? cls : "Exception");
        json_t *r = json_string(buf ? buf : "<Exception>");
        free(buf);
        return r;
    }
    json_t *a2 = json_object();
    json_set_hdr(a2, "text", text);
    json_set(a2, "force", json_bool(true));
    free(text);
    json_t *r = ta7_call(ctx, "redact_sensitive_text", a2);
    if (ta7_is_raise(r)) {
        if (ta7_is_exception(ta7_raise_kind(r))) { json_free(r); return json_string("<telegram error redacted>"); }
        return r; /* BaseException passthrough — caller owns it */
    }
    return r;
}
