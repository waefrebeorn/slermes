/*
 * telegram_adapter_flow13.c - Telegram adapter FLOW part 13 of
 * plugins/platforms/telegram/adapter.py (hermes-agent): the remainder batch
 * left after telegram_adapter_flow{,2,3,6,7,8,9,10,11,12}. Every target below
 * was confirmed UNCLAIMED; no flow 1-12 target is re-ported.
 *
 * Themes: polling-connection drain, admission failure, message editing,
 * forum command registration, inbound event enqueue/flush, bot-mention
 * filtering, and pure helpers. All live I/O and injected callables enter
 * through ONE universal seam: ta13_call(ctx,name,args).
 *
 * Self-contained: harness #includes this .c directly; libjson only.
 * Every symbol is static and ta13_-prefixed.
 *
 * Representation contract (shared with tests/oracle_telegram_adapter_flow13.py):
 * - NULL json_t* == Python None / absent; JSON_NULL is a real null value.
 * - Seam scripts: a plain value repeats; a LIST is consumed per call (last
 *   repeats); {"__script__":true,"ret":v,"raise":{type,message}} forces a
 *   result or raise. Missing/exhausted script => fail-closed RuntimeError
 *   recorded in the trace before propagation (BaseException passthrough).
 * - Except sites swallow ONLY Exception-family kinds (TA13_EXC_HIER);
 *   BaseException-only kinds pass through every except-Exception site.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _TEXT_SEND_DEADLINE 60.0

typedef struct ta13_ctx {
    json_t *state;        /* owned; carries the attribute-double fields */
    const json_t *io;     /* borrowed scripted seam results */
    json_t *cursors;      /* owned; per-name array cursors */
    json_t *trace;        /* owned; ordered seam events */
    const json_t *errors; /* borrowed exception-graph registry (unused placeholder) */
} ta13_ctx_t;

/* ── basic helpers ── */
static bool ta13_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *ta13_raise_kind(const json_t *v) {
    return ta13_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static bool ta13_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool ta13_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *ta13_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *ta13_copyv(const json_t *v) { return v ? json_copy((json_t *)v) : json_null(); }
static json_t *ta13_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static const json_t *ta13_arg(const json_t *args, const char *key) {
    return args ? json_obj_get(args, key) : NULL;
}
static const json_t *ta13_getattr(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
static const json_t *ta13_state_get(const ta13_ctx_t *ctx, const char *key) {
    return (ctx && ctx->state && ctx->state->type == JSON_OBJECT)
        ? json_obj_get(ctx->state, key) : NULL;
}
static double ta13_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static bool ta13_has(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}
static bool ta13_json_eq(const json_t *a, const json_t *b) {
    if (a == b) return true;
    if (!a || !b || a->type != b->type) return false;
    switch (a->type) {
        case JSON_NULL: return true;
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return a->str_val && b->str_val && strcmp(a->str_val, b->str_val) == 0;
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!ta13_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                if (!a->c.keys[i] || !b->c.keys[i] || strcmp(a->c.keys[i], b->c.keys[i]) != 0)
                    return false;
                if (!ta13_json_eq(a->c.items[i], b->c.items[i])) return false;
            }
            return true;
        default: return false;
    }
}

/* ── string helpers ── */
static char *ta13_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static char *ta13_lower(const char *s) {
    char *o = ta13_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static char *ta13_fmt(const char *fmt, ...) {
    char tmp[2400]; va_list ap;
    va_start(ap, fmt); int n = vsnprintf(tmp, sizeof tmp, fmt, ap); va_end(ap);
    if (n < 0) return ta13_strdup("");
    char *o = malloc((size_t)n + 1);
    if (!o) return ta13_strdup("");
    va_start(ap, fmt); vsnprintf(o, (size_t)n + 1, fmt, ap); va_end(ap);
    return o;
}
/* Python str() of a JSON scalar; integral doubles render as ints. */
static char *ta13_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return ta13_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return ta13_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e16) return ta13_fmt("%.0f", d);
            return ta13_fmt("%.17g", d);
        }
        case JSON_STRING: return ta13_strdup(v->str_val ? v->str_val : "");
        default: { char *s = json_serialize((json_t *)v); return s ? s : ta13_strdup(""); }
    }
}
/* join the py_str of each array item with a separator */
static char *ta13_join(const json_t *arr, const char *sep) {
    if (!arr || arr->type != JSON_ARRAY) return ta13_strdup("");
    size_t cap = 64, len = 0;
    char *o = malloc(cap);
    if (!o) return ta13_strdup("");
    o[0] = '\0';
    for (size_t i = 0; i < arr->c.count; i++) {
        char *s = ta13_py_str(arr->c.items[i]);
        size_t sl = strlen(s ? s : ""), spl = strlen(sep);
        size_t need = len + sl + spl + 1;
        if (need > cap) {
            while (cap < need) cap *= 2;
            char *nb = realloc(o, cap);
            if (!nb) { free(o); free(s); return ta13_strdup(""); }
            o = nb;
        }
        if (i) { memcpy(o + len, sep, spl); len += spl; }
        memcpy(o + len, s ? s : "", sl); len += sl;
        free(s);
    }
    o[len] = '\0';
    return o;
}
/* sorted insert of a numeric value into a state JSON array (ints) */
static void ta13_reg_add(ta13_ctx_t *ctx, const char *key, double val) {
    json_t *old = (json_t *)ta13_state_get(ctx, key);
    json_t *na = json_array();
    bool inserted = false;
    if (old && old->type == JSON_ARRAY) {
        for (size_t i = 0; i < old->c.count; i++) {
            if (!inserted && ta13_num(old->c.items[i]) > val) {
                json_append(na, json_number(val)); inserted = true;
            }
            json_append(na, json_copy(old->c.items[i]));
        }
    }
    if (!inserted) json_append(na, json_number(val));
    /* insertion-sort the whole array (input state may be unsorted) */
    for (size_t i = 1; i < na->c.count; i++) {
        size_t j = i;
        while (j > 0 && ta13_num(na->c.items[j - 1]) > ta13_num(na->c.items[j])) {
            json_t *tmp = na->c.items[j - 1];
            na->c.items[j - 1] = na->c.items[j];
            na->c.items[j] = tmp;
            j--;
        }
    }
    json_set(ctx->state, key, na);
}

/* ── FULL builtin exception hierarchy (CPython 3.14) ── */
typedef struct { const char *name; const char *parent; } ta13_exc_row_t;
static const ta13_exc_row_t TA13_EXC_HIER[] = {
    {"ArithmeticError", "Exception"}, {"AssertionError", "Exception"},
    {"AttributeError", "Exception"}, {"BaseException", "object"},
    {"BaseExceptionGroup", "BaseException"}, {"BlockingIOError", "OSError"},
    {"BrokenPipeError", "ConnectionError"}, {"BufferError", "Exception"},
    {"BytesWarning", "Warning"}, {"CancelledError", "BaseException"},
    {"ChildProcessError", "OSError"},
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
static const char *ta13_builtin_parent(const char *name) {
    for (size_t i = 0; i < sizeof(TA13_EXC_HIER) / sizeof(TA13_EXC_HIER[0]); i++)
        if (strcmp(name, TA13_EXC_HIER[i].name) == 0) return TA13_EXC_HIER[i].parent;
    return NULL;
}
static bool ta13_chain_has(const char *start, const char *target) {
    const char *cur = start;
    for (int depth = 0; cur && depth < 24; depth++) {
        if (strcmp(cur, target) == 0) return true;
        cur = ta13_builtin_parent(cur);
    }
    return false;
}
static bool ta13_is_exception(const char *kind) {
    if (!kind) return false;
    if (ta13_builtin_parent(kind) == NULL) return true; /* custom = Exception subclass */
    return ta13_chain_has(kind, "Exception");
}

/* ── logger helper ── */
static json_t *ta13_logger_args(json_t *arr) {
    json_t *a = json_object();
    json_set(a, "args", arr ? arr : json_array());
    return a;
}

/* ── the one universal seam ──
 * args is ADOPTED into the trace event; return is a fresh owned node. */
static json_t *ta13_call(ta13_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ta13_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ta13_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = ta13_err(json_get_str(raise, "type", "RuntimeError"),
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

/* PoP: ta13_drain_polling_connections @ plugins/platforms/telegram/adapter.py:_drain_polling_connections */
static json_t *ta13_drain_polling_connections(ta13_ctx_t *ctx, const json_t *args) {
    (void)args;
    const char *ak = ta13_str(ta13_arg(args, "app_kind"));
    const char *rk = ta13_str(ta13_arg(args, "req_kind"));
    /* LIVE returns early only when _app is None (app_kind "none") or
     * _app.bot is None (app_kind "no_bot"); every other app_kind runs the
     * normal shutdown / bounded_request_step / initialize drain flow. */
    if (ak && (strcmp(ak, "none") == 0 || strcmp(ak, "no_bot") == 0)) return json_null();
    if (rk && strcmp(rk, "missing") == 0) return json_null();
    /* polling_req.shutdown() */
    json_t *sh = ta13_call(ctx, "req_shutdown", json_object());
    if (ta13_is_raise(sh)) return sh;
    json_t *ba = json_object();
    json_set(ba, "awaitable", json_copy(sh));
    json_set(ba, "message", json_string("Polling request shutdown failed/timed out (non-fatal)"));
    json_free(sh);
    json_t *step = ta13_call(ctx, "bounded_request_step", ba);
    if (ta13_is_raise(step)) return step;
    bool ok1 = ta13_truthy(step); json_free(step);
    if (!ok1) {
        json_t *oa = json_object();
        json_set(oa, "req", ta13_copyv(ta13_arg(args, "req")));
        json_t *o = ta13_call(ctx, "orphan_and_rebuild_polling_client", oa);
        if (ta13_is_raise(o)) return o;
        json_free(o);
    }
    /* polling_req.initialize() */
    json_t *ini = ta13_call(ctx, "req_initialize", json_object());
    if (ta13_is_raise(ini)) return ini;
    json_t *bb = json_object();
    json_set(bb, "awaitable", json_copy(ini));
    json_set(bb, "message", json_string("Polling request re-initialize failed/timed out (non-fatal)"));
    json_free(ini);
    json_t *step2 = ta13_call(ctx, "bounded_request_step", bb);
    if (ta13_is_raise(step2)) return step2;
    bool ok2 = ta13_truthy(step2); json_free(step2);
    if (ok2) {
        json_t *arr = json_array();
        json_append(arr, json_string("[%s] Polling request pool drained before reconnect"));
        json_append(arr, json_string("bot"));
        json_t *d = ta13_call(ctx, "logger_debug", ta13_logger_args(arr));
        if (ta13_is_raise(d)) return d;
        json_free(d);
    } else {
        json_t *oa = json_object();
        json_set(oa, "req", ta13_copyv(ta13_arg(args, "req")));
        json_t *o = ta13_call(ctx, "orphan_and_rebuild_polling_client", oa);
        if (ta13_is_raise(o)) return o;
        json_free(o);
    }
    return json_null();
}

/* helper: claim double __setattr__ for _fail_update_preparation */
static json_t *ta13_claim_set_failed(ta13_ctx_t *ctx, const json_t *claim) {
    json_t *a = json_object();
    json_set(a, "claim", ta13_copyv(claim));
    json_set(a, "name", json_string("failed"));
    json_set(a, "value", json_bool(true));
    return ta13_call(ctx, "claim_set_failed", a);
}

/* PoP: ta13_fail_update_preparation @ plugins/platforms/telegram/adapter.py:_fail_update_preparation */
static json_t *ta13_fail_update_preparation(ta13_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *adm = ta13_state_get(ctx, "_update_admission");
    if (ta13_is_null(adm)) return json_null();
    json_t *claim = ta13_call(ctx, "admission_get", json_object());
    if (ta13_is_raise(claim)) return claim;
    if (ta13_is_null(claim)) { json_free(claim); return json_null(); }
    json_t *r = ta13_claim_set_failed(ctx, claim);
    json_free(claim);
    if (ta13_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: ta13_edit_text @ plugins/platforms/telegram/adapter.py:_edit_text */
static json_t *ta13_edit_text(ta13_ctx_t *ctx, const json_t *args) {
    const json_t *cid = ta13_arg(args, "chat_id");
    const json_t *mid = ta13_arg(args, "message_id");
    const json_t *txt = ta13_arg(args, "text");
    const json_t *pm = ta13_arg(args, "parse_mode");
    json_t *na = json_object();
    json_set(na, "chat_id", ta13_copyv(cid));
    json_t *norm = ta13_call(ctx, "normalize_telegram_chat_id", na);
    if (ta13_is_raise(norm)) return norm;
    json_t *kwargs = json_object();
    json_set(kwargs, "chat_id", json_copy(norm));
    json_free(norm);
    json_set(kwargs, "message_id", ta13_copyv(mid));
    json_set(kwargs, "text", ta13_copyv(txt));
    json_t *lpk = ta13_call(ctx, "link_preview_kwargs", json_object());
    if (ta13_is_raise(lpk)) { json_free(kwargs); return lpk; }
    if (lpk && lpk->type == JSON_OBJECT) {
        for (size_t i = 0; i < lpk->c.count; i++)
            json_set(kwargs, lpk->c.keys[i], json_copy(lpk->c.items[i]));
    }
    json_free(lpk);
    if (!ta13_is_null(pm)) json_set(kwargs, "parse_mode", ta13_copyv(pm));
    json_t *em = ta13_call(ctx, "edit_message_text", json_copy(kwargs));
    if (ta13_is_raise(em)) { json_free(kwargs); return em; }
    json_t *wa = json_object();
    json_set(wa, "awaitable", json_copy(em));
    json_free(em);
    json_set(wa, "timeout", json_number(_TEXT_SEND_DEADLINE));
    json_set(wa, "label", json_string("telegram-send"));
    json_set(wa, "dump_on_blocked_loop", json_bool(false));
    json_t *w = ta13_call(ctx, "await_with_thread_deadline", wa);
    if (ta13_is_raise(w)) { json_free(kwargs); return w; }
    json_free(w); json_free(kwargs);
    return json_null();
}

/* PoP: ta13_edit_markdown_or_plain @ plugins/platforms/telegram/adapter.py:_edit_markdown_or_plain */
static json_t *ta13_edit_markdown_or_plain(ta13_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "chat_id", ta13_copyv(ta13_arg(args, "chat_id")));
    json_set(a, "message_id", ta13_copyv(ta13_arg(args, "message_id")));
    json_set(a, "text", ta13_copyv(ta13_arg(args, "formatted")));
    json_set(a, "parse_mode", json_string("MARKDOWN_V2"));
    json_t *r = ta13_call(ctx, "edit_text", a);
    if (!ta13_is_raise(r)) { json_free(r); return json_bool(false); }
    const char *k = ta13_raise_kind(r);
    if (k && !ta13_is_exception(k)) return r; /* BaseException passthrough */
    const char *msg = json_get_str(r, "message", "");
    char *low = ta13_lower(msg ? msg : "");
    if (low && strstr(low, "not modified")) { free(low); json_free(r); return json_bool(true); }
    if (low && strstr(low, "retry after")) { free(low); return r; }
    free(low);
    json_t *ra = json_object();
    json_set(ra, "text", json_copy(r));
    json_t *rd = ta13_call(ctx, "redact", ra);
    if (ta13_is_raise(rd)) { json_free(r); return rd; }
    json_t *arr = json_array();
    json_append(arr, json_copy(ta13_copyv(ta13_arg(args, "warn_fmt"))));
    json_append(arr, json_string("bot"));
    json_append(arr, json_copy(rd));
    json_free(rd); json_free(r);
    json_t *lw = ta13_call(ctx, "logger_warning", ta13_logger_args(arr));
    if (ta13_is_raise(lw)) return lw;
    json_free(lw);
    json_t *b = json_object();
    json_set(b, "chat_id", ta13_copyv(ta13_arg(args, "chat_id")));
    json_set(b, "message_id", ta13_copyv(ta13_arg(args, "message_id")));
    json_set(b, "text", ta13_copyv(ta13_arg(args, "plain")));
    json_t *r2 = ta13_call(ctx, "edit_text", b);
    if (ta13_is_raise(r2)) return r2;
    json_free(r2);
    return json_bool(false);
}

/* PoP: ta13_edit_overflow_split @ plugins/platforms/telegram/adapter.py:_edit_overflow_split */
static json_t *ta13_edit_overflow_split(ta13_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "self", json_string("<self>"));
    json_set(a, "chat_id", ta13_copyv(ta13_arg(args, "chat_id")));
    json_set(a, "message_id", ta13_copyv(ta13_arg(args, "message_id")));
    json_set(a, "content", ta13_copyv(ta13_arg(args, "content")));
    json_set(a, "finalize", json_bool(json_get_bool(args, "finalize", false)));
    json_set(a, "metadata", ta13_copyv(ta13_arg(args, "metadata")));
    return ta13_call(ctx, "edit_overflow_split", a);
}

/* helper: suppress-Exception wrapper for _edit_result_text second call */
static json_t *ta13_edit_result_text_second(ta13_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "query", ta13_copyv(ta13_arg(args, "query")));
    json_set(a, "text", ta13_copyv(ta13_arg(args, "result_text")));
    json_set(a, "parse_mode", json_null());
    json_set(a, "reply_markup", json_null());
    json_t *r = ta13_call(ctx, "query_edit_message_text", a);
    if (ta13_is_raise(r)) {
        if (ta13_is_exception(ta13_raise_kind(r))) { json_free(r); return json_null(); }
        return r;
    }
    json_free(r);
    return json_null();
}

/* PoP: ta13_edit_result_text @ plugins/platforms/telegram/adapter.py:_edit_result_text */
static json_t *ta13_edit_result_text(ta13_ctx_t *ctx, const json_t *args) {
    json_t *fa = json_object();
    json_set(fa, "content", ta13_copyv(ta13_arg(args, "result_text")));
    json_t *fmt = ta13_call(ctx, "format_message", fa);
    if (ta13_is_raise(fmt)) {
        if (!ta13_is_exception(ta13_raise_kind(fmt))) return fmt; /* BaseException */
        json_free(fmt);
        return ta13_edit_result_text_second(ctx, args);
    }
    json_t *a = json_object();
    json_set(a, "query", ta13_copyv(ta13_arg(args, "query")));
    json_set(a, "text", json_copy(fmt));
    json_free(fmt);
    json_set(a, "parse_mode", json_string("MARKDOWN_V2"));
    json_set(a, "reply_markup", json_null());
    json_t *r = ta13_call(ctx, "query_edit_message_text", a);
    if (ta13_is_raise(r)) {
        if (!ta13_is_exception(ta13_raise_kind(r))) return r;
        json_free(r);
        return ta13_edit_result_text_second(ctx, args);
    }
    json_free(r);
    return json_null();
}

/* PoP: ta13_edit_md_quiet @ plugins/platforms/telegram/adapter.py:_edit_md_quiet */
static json_t *ta13_edit_md_quiet(ta13_ctx_t *ctx, const json_t *args) {
    json_t *fa = json_object();
    json_set(fa, "content", ta13_copyv(ta13_arg(args, "text_md")));
    json_t *fmt = ta13_call(ctx, "format_message", fa);
    if (ta13_is_raise(fmt)) {
        if (ta13_is_exception(ta13_raise_kind(fmt))) { json_free(fmt); return json_null(); }
        return fmt;
    }
    json_t *a = json_object();
    json_set(a, "query", ta13_copyv(ta13_arg(args, "query")));
    json_set(a, "text", json_copy(fmt));
    json_free(fmt);
    json_set(a, "parse_mode", json_string("MARKDOWN_V2"));
    json_set(a, "reply_markup", json_null());
    json_t *r = ta13_call(ctx, "query_edit_message_text", a);
    if (ta13_is_raise(r)) {
        if (ta13_is_exception(ta13_raise_kind(r))) { json_free(r); return json_null(); }
        return r;
    }
    json_free(r);
    return json_null();
}

/* PoP: ta13_explicit_bot_mentions_exclude_self @ plugins/platforms/telegram/adapter.py:_explicit_bot_mentions_exclude_self */
static json_t *ta13_explicit_bot_mentions_exclude_self(ta13_ctx_t *ctx, const json_t *args) {
    const json_t *msg = ta13_arg(args, "message");
    bool present = json_get_bool(args, "bot_present", true);
    json_t *username = NULL;
    if (present) {
        username = ta13_call(ctx, "current_bot_username", json_object());
        if (ta13_is_raise(username)) return username;
        if (ta13_is_null(username)) { json_free(username); return json_bool(false); }
    } else {
        return json_bool(false);
    }
    json_t *ta = json_object();
    json_set(ta, "message", ta13_copyv(msg));
    json_set(ta, "bot_username", json_copy(username));
    json_t *target = ta13_call(ctx, "foreign_command_target", ta);
    if (ta13_is_raise(target)) { json_free(username); return target; }
    bool has_target = ta13_truthy(target);
    json_t *addressed = json_array();
    if (has_target) {
        json_append(addressed, json_copy(target));
    } else {
        json_t *la = json_object();
        json_set(la, "message", ta13_copyv(msg));
        json_t *handles = ta13_call(ctx, "leading_bot_handles", la);
        if (ta13_is_raise(handles)) { json_free(target); json_free(username); json_free(addressed); return handles; }
        if (handles && handles->type == JSON_ARRAY) {
            for (size_t i = 0; i < handles->c.count; i++) {
                const json_t *h = handles->c.items[i];
                bool keep = false;
                char *hs = ta13_py_str(h);
                const char *us = ta13_str(username);
                if (us && hs && strcmp(hs, us) == 0) keep = true;
                else {
                    json_t *fa = json_object();
                    json_set(fa, "handle", json_copy(h));
                    json_t *fm = ta13_call(ctx, "foreign_handle_fullmatch", fa);
                    if (ta13_is_raise(fm)) { free(hs); json_free(handles); json_free(target); json_free(username); json_free(addressed); return fm; }
                    keep = ta13_truthy(fm); json_free(fm);
                }
                free(hs);
                if (keep) json_append(addressed, json_copy(h));
            }
        }
        json_free(handles);
    }
    json_free(target);
    if (addressed->c.count == 0) { json_free(addressed); json_free(username); return json_bool(false); }
    if (!has_target) {
        json_t *ea = json_object();
        json_set(ea, "message", ta13_copyv(msg));
        json_set(ea, "bot_username", json_copy(username));
        json_t *mentions = ta13_call(ctx, "extract_bot_mention_usernames", ea);
        if (ta13_is_raise(mentions)) { json_free(addressed); json_free(username); return mentions; }
        bool found = false;
        const char *us = ta13_str(username);
        if (mentions && mentions->type == JSON_ARRAY && us) {
            for (size_t i = 0; i < mentions->c.count; i++) {
                char *ms = ta13_py_str(mentions->c.items[i]);
                if (ms && strcmp(ms, us) == 0) { found = true; free(ms); break; }
                free(ms);
            }
        }
        json_free(mentions);
        if (found) { json_free(addressed); json_free(username); return json_bool(false); }
    }
    json_t *sc = ta13_call(ctx, "schedule_bot_identity_recheck", json_object());
    if (ta13_is_raise(sc)) { json_free(addressed); json_free(username); return sc; }
    json_free(sc);
    if (json_get_bool(args, "log_skip", true)) {
        json_t *ca = json_object();
        json_set(ca, "message", ta13_copyv(msg));
        json_t *cs = ta13_call(ctx, "chat_id_str", ca);
        if (ta13_is_raise(cs)) { json_free(addressed); json_free(username); return cs; }
        char *joined = ta13_join(addressed, ", ");
        /* prefix each handle with @ : rebuild */
        json_t *ats = json_array();
        for (size_t i = 0; i < addressed->c.count; i++) {
            char *hs = ta13_py_str(addressed->c.items[i]);
            char *at = ta13_fmt("@%s", hs ? hs : "");
            json_append(ats, json_string(at));
            free(at); free(hs);
        }
        char *joined2 = ta13_join(ats, ", ");
        free(joined);
        json_free(ats);
        json_t *arr = json_array();
        json_append(arr, json_string("[%s] Skipping message %s in chat %s: addressed to other bot(s) %s"));
        json_append(arr, json_string("bot"));
        json_append(arr, ta13_copyv(ta13_getattr(msg, "message_id")));
        json_append(arr, json_copy(cs));
        json_append(arr, json_string(joined2 ? joined2 : ""));
        free(joined2);
        json_t *li = ta13_call(ctx, "logger_info", ta13_logger_args(arr));
        if (ta13_is_raise(li)) { json_free(cs); json_free(addressed); json_free(username); return li; }
        json_free(li); json_free(cs);
    }
    json_free(addressed); json_free(username);
    return json_bool(true);
}

/* PoP: ta13_ensure_forum_commands @ plugins/platforms/telegram/adapter.py:_ensure_forum_commands */
static json_t *ta13_ensure_forum_commands(ta13_ctx_t *ctx, const json_t *args) {
    if (!ta13_state_get(ctx, "_forum_command_registered"))
        json_set(ctx->state, "_forum_command_registered", json_array());
    json_t *ent = ta13_call(ctx, "forum_lock_aenter", json_object());
    if (ta13_is_raise(ent)) return ent;
    json_free(ent);
    json_t *result = NULL;
    bool do_exit = false;
    const json_t *msg = ta13_arg(args, "message");
    const json_t *chat = ta13_getattr(msg, "chat");
    bool proceed = chat && ta13_truthy(ta13_getattr(chat, "is_forum"));
    if (!proceed) goto done;
    double cid = ta13_num(ta13_getattr(chat, "id"));
    json_t *reg = (json_t *)ta13_state_get(ctx, "_forum_command_registered");
    for (size_t i = 0; reg && reg->type == JSON_ARRAY && i < reg->c.count; i++) {
        if (ta13_num(reg->c.items[i]) == cid) goto done;
    }
    json_t *mx = ta13_call(ctx, "menu_max_commands", json_object());
    if (ta13_is_raise(mx)) { result = mx; goto fail; }
    double maxc = ta13_num(mx); json_free(mx);
    json_t *ta = json_object();
    json_set(ta, "func", json_string("telegram_menu_commands"));
    json_set(ta, "max_commands", json_number(maxc));
    json_t *th = ta13_call(ctx, "to_thread", ta);
    if (ta13_is_raise(th)) { result = th; goto fail; }
    json_t *menu = (th && th->type == JSON_ARRAY && th->c.count > 0) ? (json_t *)json_copy(th->c.items[0]) : json_null();
    json_free(th);
    json_t *bc_a = json_object();
    json_set(bc_a, "menu_commands", menu);
    json_t *cmds = ta13_call(ctx, "bot_commands", bc_a);
    if (ta13_is_raise(cmds)) { result = cmds; goto fail; }
    json_t *sa = json_object();
    json_set(sa, "commands", json_copy(cmds));
    json_t *scope = json_object();
    json_set(scope, "chat_id", json_number(cid));
    json_set(sa, "scope", scope);
    json_t *sm = ta13_call(ctx, "set_my_commands", sa);
    if (ta13_is_raise(sm)) { json_free(cmds); result = sm; goto fail; }
    json_free(sm);
    ta13_reg_add(ctx, "_forum_command_registered", cid);
    json_t *li_arr = json_array();
    json_append(li_arr, json_string("[%s] Lazy-registered %d commands for forum chat %s"));
    json_append(li_arr, json_string("bot"));
    json_append(li_arr, json_number((double)json_len(cmds)));
    json_free(cmds);
    json_append(li_arr, json_number(cid));
    json_t *li = ta13_call(ctx, "logger_info", ta13_logger_args(li_arr));
    if (ta13_is_raise(li)) { result = li; goto fail; }
    json_free(li);
    goto done;

fail:
    if (result && ta13_is_exception(ta13_raise_kind(result))) {
        json_t *ra = json_object();
        json_set(ra, "text", json_copy(result));
        json_t *rd = ta13_call(ctx, "redact", ra);
        if (ta13_is_raise(rd)) { result = rd; goto done; }
        json_t *arr = json_array();
        json_append(arr, json_string("[%s] Forum command lazy-registration failed: %s"));
        json_append(arr, json_string("bot"));
        json_append(arr, json_copy(rd));
        json_free(rd);
        json_t *lw = ta13_call(ctx, "logger_warning", ta13_logger_args(arr));
        if (ta13_is_raise(lw)) result = lw;
        json_free(result);
        result = NULL;
    }
    /* BaseException only: propagate after exit */
done:
    {
        json_t *ex = ta13_call(ctx, "forum_lock_aexit", json_object());
        if (ta13_is_raise(ex) && !result) { result = ex; }
        else if (ta13_is_raise(ex)) json_free(ex);
    }
    (void)do_exit;
    return result ? result : json_null();
}

/* PoP: ta13_enqueue_text_event @ plugins/platforms/telegram/adapter.py:_enqueue_text_event */
static json_t *ta13_enqueue_text_event(ta13_ctx_t *ctx, const json_t *args) {
    const json_t *ev = ta13_arg(args, "event");
    json_t *na = json_object();
    json_set(na, "event", ta13_copyv(ev));
    json_t *r = ta13_call(ctx, "note_arrival", na);
    if (ta13_is_raise(r)) return r;
    json_free(r);
    json_t *dr = ta13_call(ctx, "should_drop_delayed_delivery", json_object());
    if (ta13_is_raise(dr)) return dr;
    bool drop = ta13_truthy(dr); json_free(dr);
    if (drop) {
        json_t *ha = json_object();
        json_set(ha, "event", ta13_copyv(ev));
        json_set(ha, "where", json_string("text-enqueue"));
        json_t *h = ta13_call(ctx, "hold_inbound_event", ha);
        if (ta13_is_raise(h)) return h;
        json_free(h);
        return json_null();
    }
    json_t *ka = json_object();
    json_set(ka, "event", ta13_copyv(ev));
    json_t *key = ta13_call(ctx, "text_batch_key", ka);
    if (ta13_is_raise(key)) return key;
    char *kstr = ta13_py_str(key);
    const json_t *batches = ta13_state_get(ctx, "_pending_text_batches");
    const json_t *existing = (batches && batches->type == JSON_OBJECT && kstr) ? json_obj_get(batches, kstr) : NULL;
    const json_t *before = ta13_getattr(existing, "text");
    json_t *ma = json_object();
    json_set(ma, "existing", ta13_copyv(existing));
    json_set(ma, "event", ta13_copyv(ev));
    json_t *merged = ta13_call(ctx, "merged_typed_text", ma);
    if (ta13_is_raise(merged)) { free(kstr); return merged; }
    json_t *sa = json_object();
    json_set(sa, "event", ta13_copyv(ev));
    json_t *sup = ta13_call(ctx, "super_enqueue_text_event", sa);
    if (ta13_is_raise(sup)) { free(kstr); json_free(merged); return sup; }
    json_free(sup);
    if (!ta13_is_null(merged) && !ta13_is_null(existing) && !ta13_is_null(before)) {
        const json_t *et = ta13_getattr(existing, "text");
        if (!ta13_json_eq(et, before)) {
            json_t *ra = json_object();
            json_set(ra, "existing", ta13_copyv(existing));
            json_set(ra, "text", json_copy(merged));
            json_t *rec = ta13_call(ctx, "record_typed_text", ra);
            if (ta13_is_raise(rec)) { free(kstr); json_free(merged); return rec; }
            json_free(rec);
        }
    }
    free(kstr); json_free(merged);
    json_t *ac = ta13_call(ctx, "accept_update", json_object());
    if (ta13_is_raise(ac)) return ac;
    json_free(ac);
    return json_null();
}

/* PoP: ta13_enqueue_photo_event @ plugins/platforms/telegram/adapter.py:_enqueue_photo_event */
static json_t *ta13_enqueue_photo_event(ta13_ctx_t *ctx, const json_t *args) {
    if (!ta13_state_get(ctx, "_pending_photo_batch_tasks"))
        json_set(ctx->state, "_pending_photo_batch_tasks", json_object());
    const json_t *ev = ta13_arg(args, "event");
    json_t *na = json_object();
    json_set(na, "event", ta13_copyv(ev));
    json_t *r = ta13_call(ctx, "note_arrival", na);
    if (ta13_is_raise(r)) return r;
    json_free(r);
    json_t *dr = ta13_call(ctx, "should_drop_delayed_delivery", json_object());
    if (ta13_is_raise(dr)) return dr;
    bool drop = ta13_truthy(dr); json_free(dr);
    if (drop) {
        json_t *ha = json_object();
        json_set(ha, "event", ta13_copyv(ev));
        json_set(ha, "where", json_string("photo-enqueue"));
        json_t *h = ta13_call(ctx, "hold_inbound_event", ha);
        if (ta13_is_raise(h)) return h;
        json_free(h);
        return json_null();
    }
    json_t *ma = json_object();
    {
        const json_t *pb = ta13_state_get(ctx, "_pending_photo_batches");
        json_set(ma, "pending", ta13_is_null(pb) ? json_object() : ta13_copyv(pb));
    }
    json_set(ma, "batch_key", ta13_copyv(ta13_arg(args, "batch_key")));
    json_set(ma, "event", ta13_copyv(ev));
    json_t *key = ta13_call(ctx, "merge_into_pending", ma);
    if (ta13_is_raise(key)) return key;
    json_t *ac = ta13_call(ctx, "accept_update", json_object());
    if (ta13_is_raise(ac)) { json_free(key); return ac; }
    json_free(ac);
    char *kstr = ta13_py_str(key);
    const json_t *tasks = ta13_state_get(ctx, "_pending_photo_batch_tasks");
    const json_t *prior = (tasks && tasks->type == JSON_OBJECT && kstr) ? json_obj_get(tasks, kstr) : NULL;
    if (!ta13_is_null(prior)) {
        json_t *da = json_object();
        json_set(da, "task", ta13_copyv(prior));
        json_t *done = ta13_call(ctx, "task_done", da);
        if (ta13_is_raise(done)) { free(kstr); json_free(key); return done; }
        bool d = ta13_truthy(done); json_free(done);
        if (!d) {
            json_t *ca = json_object();
            json_set(ca, "task", ta13_copyv(prior));
            json_t *c = ta13_call(ctx, "task_cancel", ca);
            if (ta13_is_raise(c)) { free(kstr); json_free(key); return c; }
            json_free(c);
        }
    }
    json_t *fa = json_object();
    json_set(fa, "batch_key", json_copy(key));
    json_t *marker = ta13_call(ctx, "flush_photo_batch", fa);
    if (ta13_is_raise(marker)) { free(kstr); json_free(key); return marker; }
    json_t *ca = json_object();
    json_set(ca, "coro", json_copy(marker));
    json_free(marker);
    json_t *task = ta13_call(ctx, "create_task", ca);
    if (ta13_is_raise(task)) { free(kstr); json_free(key); return task; }
    json_t *tasksw = (json_t *)ta13_state_get(ctx, "_pending_photo_batch_tasks");
    if (!tasksw || tasksw->type != JSON_OBJECT) {
        tasksw = json_object();
        json_set(ctx->state, "_pending_photo_batch_tasks", tasksw);
    }
    json_set(tasksw, kstr ? kstr : "", json_copy(task));
    json_free(task); free(kstr); json_free(key);
    return json_null();
}

/* PoP: ta13_flush_buffered @ plugins/platforms/telegram/adapter.py:_flush_buffered */
static json_t *ta13_flush_buffered(ta13_ctx_t *ctx, const json_t *args) {
    const char *key = ta13_str(ta13_arg(args, "key"));
    const char *where = ta13_str(ta13_arg(args, "where"));
    json_t *cur = ta13_call(ctx, "current_task", json_object());
    if (ta13_is_raise(cur)) return cur;
    json_t *committed = (json_t *)ta13_state_get(ctx, "_committed_flush_tasks");
    if (!committed || committed->type != JSON_OBJECT) {
        committed = json_object();
        json_set(ctx->state, "_committed_flush_tasks", committed);
    }
    char *comp = ta13_fmt("%s|%s", where ? where : "", key ? key : "");
    json_t *pending = json_copy(ta13_arg(args, "pending"));
    json_t *tasks = json_copy(ta13_arg(args, "tasks"));
    json_t *event = NULL;
    json_t *result = NULL;
    /* await sleep */
    json_t *sa = json_object();
    json_set(sa, "delay", ta13_copyv(ta13_arg(args, "delay")));
    json_t *sl = ta13_call(ctx, "sleep", sa);
    if (ta13_is_raise(sl)) { result = sl; goto cancel_check; }
    json_free(sl);
    {
        const json_t *owner = (tasks && tasks->type == JSON_OBJECT && key) ? json_obj_get(tasks, key) : NULL;
        if (!ta13_is_null(owner) && !ta13_json_eq(owner, cur)) goto fin;
    }
    if (pending && pending->type == JSON_OBJECT && key) {
        event = json_obj_get(pending, key);
        if (event) { event = json_copy(event); json_obj_del(pending, key); }
        else event = NULL;
    }
    if (ta13_is_null(event)) goto fin;
    if (tasks && tasks->type == JSON_OBJECT && key) json_obj_del(tasks, key);
    {
        const json_t *prior = json_obj_get(committed, comp);
        json_set(committed, comp, json_copy(cur));
        if (!ta13_is_null(prior)) {
            json_t *da = json_object();
            json_set(da, "task", ta13_copyv(prior));
            json_t *done = ta13_call(ctx, "task_done", da);
            if (ta13_is_raise(done)) { result = done; goto cancel_check; }
            bool d = ta13_truthy(done); json_free(done);
            if (!d) {
                json_t *wa = json_object();
                json_t *tarr = json_array();
                json_append(tarr, ta13_copyv(prior));
                json_set(wa, "tasks", tarr);
                json_t *w = ta13_call(ctx, "asyncio_wait", wa);
                if (ta13_is_raise(w)) { result = w; goto cancel_check; }
                json_free(w);
            }
        }
    }
    {
        json_t *dr = ta13_call(ctx, "should_drop_delayed_delivery", json_object());
        if (ta13_is_raise(dr)) { result = dr; goto cancel_check; }
        bool drop = ta13_truthy(dr); json_free(dr);
        if (drop) {
            json_t *ha = json_object();
            json_set(ha, "event", json_copy(event));
            char *w2 = ta13_fmt("%s-flush", where ? where : "");
            json_set(ha, "where", json_string(w2));
            free(w2);
            json_t *h = ta13_call(ctx, "hold_inbound_event", ha);
            if (ta13_is_raise(h)) { result = h; goto cancel_check; }
            json_free(h);
            json_free(event); event = NULL;
            goto fin;
        }
    }
    if (json_get_bool(args, "log_fn", false)) {
        json_t *la = json_object();
        json_set(la, "event", json_copy(event));
        json_t *l = ta13_call(ctx, "log_fn", la);
        if (ta13_is_raise(l)) { result = l; goto cancel_check; }
        json_free(l);
    }
    {
        json_t *ha = json_object();
        json_set(ha, "event", json_copy(event));
        json_t *h = ta13_call(ctx, "handle_message", ha);
        if (ta13_is_raise(h)) { result = h; goto cancel_check; }
        json_free(h);
    }
    json_free(event); event = NULL;
    goto fin;

cancel_check:
    if (result && ta13_is_raise(result)) {
        const char *k = ta13_raise_kind(result);
        if (k && strcmp(k, "CancelledError") == 0 && !ta13_is_null(event)) {
            json_t *ha = json_object();
            json_set(ha, "event", json_copy(event));
            char *w2 = ta13_fmt("%s-flush-cancelled", where ? where : "");
            json_set(ha, "where", json_string(w2));
            free(w2);
            json_t *h = ta13_call(ctx, "hold_inbound_event", ha);
            if (ta13_is_raise(h)) { json_free(result); result = h; }
            else json_free(h);
        }
    }
fin:
    {
        const json_t *cv = json_obj_get(committed, comp);
        if (cv && ta13_json_eq(cv, cur)) json_obj_del(committed, comp);
        const json_t *tv = (tasks && tasks->type == JSON_OBJECT && key) ? json_obj_get(tasks, key) : NULL;
        if (tv && ta13_json_eq(tv, cur)) json_obj_del(tasks, key);
    }
    json_free(event);
    json_free(pending); json_free(tasks);
    free(comp);
    json_free(cur);
    return result ? result : json_null();
}

/* PoP: ta13_edit_revision @ plugins/platforms/telegram/adapter.py:_edit_revision */
static json_t *ta13_edit_revision(ta13_ctx_t *ctx, const json_t *args) {
    const json_t *msg = args;
    const json_t *ed = ta13_getattr(msg, "edit_date");
    if (ta13_is_null(ed) || ed->type != JSON_OBJECT || !ta13_has(ed, "__ts__"))
        return json_null();
    const json_t *uid = ta13_arg(args, "update_id");
    if (!ta13_is_null(uid)) {
        char *s = ta13_fmt("u%s", ta13_py_str(uid));
        json_t *r = json_string(s ? s : "u");
        free(s);
        return r;
    }
    const json_t *tx = ta13_getattr(msg, "text");
    const json_t *cap = ta13_getattr(msg, "caption");
    const char *text = (!ta13_is_null(tx) && ta13_str(tx)) ? ta13_str(tx)
                     : ((!ta13_is_null(cap) && ta13_str(cap)) ? ta13_str(cap) : "");
    json_t *ts = ta13_call(ctx, "edit_timestamp", json_object());
    if (ta13_is_raise(ts)) return ts;
    double tsv = ta13_num(ts); json_free(ts);
    json_t *ha = json_object();
    json_set(ha, "text", json_string(text ? text : ""));
    json_t *hx = ta13_call(ctx, "sha256_hexdigest", ha);
    if (ta13_is_raise(hx)) return hx;
    const char *hexs = ta13_str(hx);
    char hex16[17];
    if (hexs) { size_t n = strlen(hexs) < 16 ? strlen(hexs) : 16; memcpy(hex16, hexs, n); hex16[n] = '\0'; }
    else hex16[0] = '\0';
    json_free(hx);
    char *out = ta13_fmt("%.0f.%s", floor(tsv), hex16);
    json_t *r = json_string(out ? out : "");
    free(out);
    return r;
}

/* PoP: ta13_text_batch_key @ plugins/platforms/telegram/adapter.py:_text_batch_key */
static json_t *ta13_text_batch_key(ta13_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "event", ta13_copyv(ta13_arg(args, "event")));
    json_t *s1 = ta13_call(ctx, "super_text_batch_key", a);
    if (ta13_is_raise(s1)) return s1;
    json_t *b = json_object();
    json_set(b, "event", ta13_copyv(ta13_arg(args, "event")));
    json_t *s2 = ta13_call(ctx, "shared_sender_suffix", b);
    if (ta13_is_raise(s2)) { json_free(s1); return s2; }
    char *x = ta13_py_str(s1), *y = ta13_py_str(s2);
    char *out = ta13_fmt("%s%s", x ? x : "", y ? y : "");
    free(x); free(y); json_free(s1); json_free(s2);
    json_t *r = json_string(out ? out : "");
    free(out);
    return r;
}

/* PoP: ta13_mark_connected @ plugins/platforms/telegram/adapter.py:_mark_connected */
static json_t *ta13_mark_connected(ta13_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_set(ctx->state, "_drop_delayed_deliveries", json_bool(false));
    json_t *r = ta13_call(ctx, "super_mark_connected", json_object());
    if (ta13_is_raise(r)) return r;
    json_free(r);
    json_t *s = ta13_call(ctx, "schedule_held_inbound_redispatch", json_object());
    if (ta13_is_raise(s)) return s;
    json_free(s);
    return json_null();
}

/* PoP: ta13_mark_disconnected @ plugins/platforms/telegram/adapter.py:_mark_disconnected */
static json_t *ta13_mark_disconnected(ta13_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_set(ctx->state, "_drop_delayed_deliveries", json_bool(true));
    json_t *r = ta13_call(ctx, "super_mark_disconnected", json_object());
    if (ta13_is_raise(r)) return r;
    json_free(r);
    return json_null();
}
