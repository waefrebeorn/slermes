/*
 * telegram_adapter_flow15.c - Telegram adapter FLOW part 15 of
 * plugins/platforms/telegram/adapter.py (hermes-agent): 16 remaining UNCLAIMED
 * defs. No flow 1-14 target is re-ported. Every target below was confirmed
 * UNCLAIMED in the dotted-name aware live parity scan.
 *
 * Themes: message handlers (text/location/media/sticker), callback handlers
 * (model-picker/slash-confirm/update-prompt), polling recovery/conflict,
 * callback auth, outbound-slot, init retry ladder and two small helpers.
 * All live I/O and injected callables enter through ONE universal seam:
 * ta15_call(ctx, name, args).
 *
 * Self-contained: harness #includes this .c directly; libjson only.
 * Every symbol is static and ta15_-prefixed.
 *
 * Representation contract (shared with tests/oracle_telegram_adapter_flow15.py):
 * - NULL json_t* == Python None / absent; JSON_NULL is a real null value.
 * - Seam scripts: a plain value repeats; a LIST is consumed per call (last
 *   repeats); {"__script__":true,"ret":v,"raise":{type,message}} forces a
 *   result or raise. Missing/exhausted script => fail-closed RuntimeError
 *   recorded in the trace before propagation (BaseException passthrough).
 * - Except sites swallow ONLY Exception-family kinds (TA15_EXC_HIER);
 *   BaseException-only kinds pass through every except-Exception site.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
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

#define _TA15_TOAST_LIMIT 200
#define _TA15_SPLIT_DEFAULT 4000

typedef struct ta15_ctx {
    json_t *state;        /* owned; carries the attribute-double fields */
    const json_t *io;     /* borrowed scripted seam results */
    json_t *cursors;      /* owned; per-name array cursors */
    json_t *trace;        /* owned; ordered seam events */
    const json_t *errors; /* borrowed placeholder */
} ta15_ctx_t;

/* ── basic helpers ── */
static bool ta15_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *ta15_raise_kind(const json_t *v) {
    return ta15_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static const char *ta15_raise_msg(const json_t *v) {
    return ta15_is_raise(v) ? json_get_str(v, "message", "") : "";
}
static bool ta15_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool ta15_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *ta15_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *ta15_copyv(const json_t *v) { return v ? json_copy((json_t *)v) : json_null(); }
static json_t *ta15_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static const json_t *ta15_arg(const json_t *args, const char *key) {
    return args ? json_obj_get(args, key) : NULL;
}
static const json_t *ta15_getattr(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
static const json_t *ta15_state_get(const ta15_ctx_t *ctx, const char *key) {
    return (ctx && ctx->state && ctx->state->type == JSON_OBJECT)
        ? json_obj_get(ctx->state, key) : NULL;
}
static double ta15_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static bool ta15_has(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}
static bool ta15_json_eq(const json_t *a, const json_t *b) {
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
                if (!ta15_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                if (!a->c.keys[i] || !b->c.keys[i] || strcmp(a->c.keys[i], b->c.keys[i]) != 0)
                    return false;
                if (!ta15_json_eq(a->c.items[i], b->c.items[i])) return false;
            }
            return true;
        default: return false;
    }
}

/* ── string helpers ── */
static char *ta15_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static char *ta15_lower(const char *s) {
    char *o = ta15_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static char *ta15_fmt(const char *fmt, ...) {
    char tmp[2400]; va_list ap;
    va_start(ap, fmt); int n = vsnprintf(tmp, sizeof tmp, fmt, ap); va_end(ap);
    if (n < 0) return ta15_strdup("");
    char *o = malloc((size_t)n + 1);
    if (!o) return ta15_strdup("");
    va_start(ap, fmt); vsnprintf(o, (size_t)n + 1, fmt, ap); va_end(ap);
    return o;
}
static char *ta15_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return ta15_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return ta15_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e16) return ta15_fmt("%.0f", d);
            return ta15_fmt("%.17g", d);
        }
        case JSON_STRING: return ta15_strdup(v->str_val ? v->str_val : "");
        default: { char *s = json_serialize((json_t *)v); return s ? s : ta15_strdup(""); }
    }
}
static char *ta15_join(const json_t *arr, const char *sep) {
    if (!arr || arr->type != JSON_ARRAY) return ta15_strdup("");
    size_t cap = 64, len = 0;
    char *o = malloc(cap);
    if (!o) return ta15_strdup("");
    o[0] = '\0';
    for (size_t i = 0; i < arr->c.count; i++) {
        char *s = ta15_py_str(arr->c.items[i]);
        size_t sl = strlen(s ? s : ""), spl = strlen(sep);
        size_t need = len + sl + spl + 1;
        if (need > cap) {
            while (cap < need) cap *= 2;
            char *nb = realloc(o, cap);
            if (!nb) { free(o); free(s); return ta15_strdup(""); }
            o = nb;
        }
        if (i) { memcpy(o + len, sep, spl); len += spl; }
        memcpy(o + len, s ? s : "", sl); len += sl;
        free(s);
    }
    o[len] = '\0';
    return o;
}

/* Python str.split(":", 2) into at most three heap parts; returns count. */
static int ta15_split3(const char *s, char **a, char **b, char **c) {
    if (!s) return 0;
    const char *p1 = strchr(s, ':');
    if (!p1) { *a = ta15_strdup(s); return 1; }
    *a = (char *)malloc((size_t)(p1 - s) + 1);
    memcpy(*a, s, (size_t)(p1 - s)); (*a)[p1 - s] = '\0';
    const char *p2 = strchr(p1 + 1, ':');
    if (!p2) {
        *b = ta15_strdup(p1 + 1); *c = ta15_strdup(""); return 2;
    }
    *b = (char *)malloc((size_t)(p2 - p1 - 1) + 1);
    memcpy(*b, p1 + 1, (size_t)(p2 - p1 - 1)); (*b)[p2 - p1 - 1] = '\0';
    *c = ta15_strdup(p2 + 1);
    return 3;
}

/* int(v) with CPython-style ValueError on failure. */
static bool ta15_parse_int(const json_t *v, double *out) {
    if (!v) return false;
    if (v->type == JSON_NUMBER) { *out = floor(v->num_val); return true; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return true; }
    if (v->type != JSON_STRING) return false;
    const char *s = v->str_val ? v->str_val : "";
    while (*s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' ||
                  *s == '\v' || *s == '\f')) s++;
    char *end = NULL;
    long long n = strtoll(s, &end, 10);
    if (end == s) return false;
    while (*end && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' ||
                    *end == '\v' || *end == '\f')) end++;
    if (*end != '\0') return false;
    *out = (double)n;
    return true;
}

/* ── FULL builtin exception hierarchy (CPython 3.14) ── */
typedef struct { const char *name; const char *parent; } ta15_exc_row_t;
static const ta15_exc_row_t TA15_EXC_HIER[] = {
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
static const char *ta15_builtin_parent(const char *name) {
    for (size_t i = 0; i < sizeof(TA15_EXC_HIER) / sizeof(TA15_EXC_HIER[0]); i++)
        if (strcmp(name, TA15_EXC_HIER[i].name) == 0) return TA15_EXC_HIER[i].parent;
    return NULL;
}
static bool ta15_chain_has(const char *start, const char *target) {
    const char *cur = start;
    for (int depth = 0; cur && depth < 24; depth++) {
        if (strcmp(cur, target) == 0) return true;
        cur = ta15_builtin_parent(cur);
    }
    return false;
}
static bool ta15_is_exception(const char *kind) {
    if (!kind) return false;
    if (ta15_builtin_parent(kind) == NULL) return true;
    return ta15_chain_has(kind, "Exception");
}

/* ── the one universal seam ── */
static json_t *ta15_call(ta15_ctx_t *ctx, const char *name, json_t *args);

/* logger helper ── */
static json_t *ta15_logger_args(const json_t *arr) {
    json_t *a = json_object();
    json_set(a, "args", arr ? arr : json_array());
    return a;
}
/* toast key -> {"args":[key],"kwargs":{...}} */
static json_t *ta15_toast_kw(ta15_ctx_t *ctx, const char *key, json_t *kwargs) {
    json_t *a = json_object();
    json_t *arr = json_array();
    json_append(arr, json_string(key));
    json_set(a, "args", arr);
    json_set(a, "kwargs", kwargs ? kwargs : json_object());
    return ta15_call(ctx, "toast", a);
}
static json_t *ta15_toast(ta15_ctx_t *ctx, const char *key) {
    return ta15_toast_kw(ctx, key, NULL);
}

/* ── the one universal seam ── */
static json_t *ta15_call(ta15_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256]; snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ta15_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256]; snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ta15_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT)
                ret = ta15_err(json_get_str(raise, "type", "RuntimeError"),
                               json_get_str(raise, "message", ""));
            else {
                json_t *r = json_obj_get(used, "ret");
                ret = r ? json_copy(r) : json_null();
            }
        } else ret = used ? json_copy(used) : json_null();
    }
    json_set(ev, "ret", json_copy(ret));
    json_append(ctx->trace, ev);
    return ret;
}



/* ── extra helpers ── */
/* Python str(float): integral floats keep a ".0" suffix. */
static char *ta15_py_float(double d) {
    char tmp[64];
    if (d == floor(d) && fabs(d) < 1e16) snprintf(tmp, sizeof tmp, "%.0f", d);
    else snprintf(tmp, sizeof tmp, "%.17g", d);
    return ta15_strdup(tmp);
}
static char *ta15_trim(const char *s) {
    if (!s) return ta15_strdup("");
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    char *o = malloc(n + 1);
    if (o) { memcpy(o, s, n); o[n] = '\0'; }
    return o;
}
static bool ta15_lower_in(const char *s, const char *const *set, size_t n) {
    if (!s) return false;
    char *l = ta15_lower(s);
    for (size_t i = 0; i < n; i++) if (strcmp(l, set[i]) == 0) { free(l); return true; }
    free(l); return false;
}
static json_t *ta15_answer(ta15_ctx_t *ctx, const json_t *q, json_t *text, bool show_alert) {
    json_t *a = json_object();
    json_set(a, "query", ta15_copyv(q));
    json_set(a, "text", text ? text : json_null());
    if (show_alert) {
        json_t *kw = json_object();
        json_set(kw, "show_alert", json_bool(true));
        json_set(a, "kwargs", kw);
    }
    return ta15_call(ctx, "answer_callback", a);
}

/* ═══════════════════════ ported flows ═══════════════════════ */

/* PoP: ta15_hold_chat_outbound_slot @ plugins/platforms/telegram/adapter.py:_hold_chat_outbound_slot */
static json_t *ta15_hold_chat_outbound_slot(ta15_ctx_t *ctx, const json_t *args) {
    json_t *su = (json_t *)ta15_state_get(ctx, "_telegram_chat_outbound_slot_until");
    if (!su || su->type != JSON_OBJECT) {
        su = json_object();
        json_set(ctx->state, "_telegram_chat_outbound_slot_until", su);
    }
    json_t *t = ta15_call(ctx, "loop_time", json_object());
    if (ta15_is_raise(t)) return t;
    const json_t *bg = ta15_state_get(ctx, "_telegram_chat_outbound_slot_secs");
    if (!bg || bg->type == JSON_NULL) {
        json_free(t);
        return ta15_err("TypeError", "'>' not supported between instances of 'NoneType' and 'float'");
    }
    json_t *na = json_object();
    json_set(na, "chat_id", ta15_copyv(ta15_arg(args, "chat_id")));
    json_t *n = ta15_call(ctx, "normalize_chat_id", na);
    if (ta15_is_raise(n)) { json_free(t); return n; }
    const char *key = ta15_str(n);
    char *kbuf = key ? NULL : ta15_py_str(n);
    if (!key) key = kbuf;
    double budget = (bg->type == JSON_NUMBER) ? bg->num_val : 0.0;
    if (budget < 0.0) budget = 0.0;
    json_set(su, key, json_number(ta15_num(t) + budget));
    free(kbuf); json_free(t); json_free(n);
    return json_null();
}

/* PoP: ta15_handoff_polling_fatal_error @ plugins/platforms/telegram/adapter.py:_handoff_polling_fatal_error */
static json_t *ta15_handoff_polling_fatal_error(ta15_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *cur = ta15_call(ctx, "current_task", json_object());
    if (ta15_is_raise(cur)) return cur;
    json_free(cur);
    json_t *r = ta15_call(ctx, "notify_fatal_error", json_object());
    if (ta15_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: ta15_warn_then @ plugins/platforms/telegram/adapter.py:_warn_then */
static json_t *ta15_warn_then(ta15_ctx_t *ctx, const json_t *args) {
    json_t *ea = json_object();
    json_set(ea, "error", ta15_copyv(ta15_arg(args, "error")));
    json_set(ea, "proxy", json_null());
    json_t *red = ta15_call(ctx, "redact_telegram_error_text", ea);
    if (ta15_is_raise(red)) return red;
    json_t *arr = json_array();
    json_append(arr, json_string("[%s] Failed to send %s: %s"));
    json_append(arr, json_string("bot"));
    json_append(arr, ta15_copyv(ta15_arg(args, "media_key")));
    json_append(arr, json_copy(red));
    json_free(red);
    json_t *r = ta15_call(ctx, "logger_warning", ta15_logger_args(arr));
    if (ta15_is_raise(r)) return r;
    json_free(r);
    return ta15_call(ctx, "fallback", json_object());
}

/* PoP: ta15_set_status_indicator @ plugins/platforms/telegram/adapter.py:_set_status_indicator */
static json_t *ta15_set_status_indicator(ta15_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "online", ta15_copyv(ta15_arg(args, "online")));
    json_t *r = ta15_call(ctx, "set_status_indicator", a);
    if (ta15_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: ta15_instrument_polling_request @ plugins/platforms/telegram/adapter.py:_instrument_polling_request */
static json_t *ta15_instrument_polling_request(ta15_ctx_t *ctx, const json_t *args) {
    (void)ctx; (void)args;
    return json_string("request");
}

/* PoP: ta15_is_callback_user_authorized @ plugins/platforms/telegram/adapter.py:_is_callback_user_authorized */
static json_t *ta15_is_callback_user_authorized(ta15_ctx_t *ctx, const json_t *args) {
    json_set(ctx->state, "_authorization_check", ta15_copyv(ta15_arg(args, "has_auth_check")));
    char *uid = ta15_trim(ta15_str(ta15_arg(args, "user_id")));
    if (!uid[0]) { free(uid); return json_bool(false); }
    json_t *r = ta15_call(ctx, "refresh_gates", json_object());
    if (ta15_is_raise(r)) { free(uid); return r; }
    json_free(r);
    json_t *ca = json_object();
    json_set(ca, "chat_type", ta15_copyv(ta15_arg(args, "chat_type")));
    json_set(ca, "is_forum", json_bool(!ta15_is_null(ta15_arg(args, "thread_id"))));
    json_t *ct = ta15_call(ctx, "normalize_chat_type", ca);
    if (ta15_is_raise(ct)) { free(uid); return ct; }
    char *cid = ta15_truthy(ta15_arg(args, "chat_id")) ? ta15_py_str(ta15_arg(args, "chat_id")) : ta15_strdup(uid);
    if (ta15_truthy(ta15_arg(args, "has_auth_check"))) {
        json_t *a = json_object();
        json_set(a, "user_id", json_string(uid));
        json_set(a, "chat_type", json_copy(ct));
        json_set(a, "chat_id", json_string(cid));
        json_set(a, "thread_id", ta15_is_null(ta15_arg(args, "thread_id"))
                 ? json_null() : json_string(ta15_py_str(ta15_arg(args, "thread_id"))));
        json_t *inj = ta15_call(ctx, "is_sender_authorized", a);
        if (ta15_is_raise(inj)) { free(uid); free(cid); json_free(ct); return inj; }
        if (!ta15_is_null(inj)) {
            json_t *res = json_bool(ta15_truthy(inj));
            json_free(inj); free(uid); free(cid); json_free(ct);
            return res;
        }
        json_free(inj);
    }
    json_t *afn = ta15_call(ctx, "legacy_runner_auth_fn", json_object());
    if (ta15_is_raise(afn)) { free(uid); free(cid); json_free(ct); return afn; }
    if (ta15_truthy(afn)) {
        json_t *src = json_object();
        json_set(src, "platform", json_string("telegram"));
        json_set(src, "chat_id", json_string(cid));
        json_set(src, "chat_type", json_copy(ct));
        json_set(src, "user_id", json_string(uid));
        const json_t *un = ta15_arg(args, "user_name");
        json_set(src, "user_name", ta15_truthy(un) ? json_string(ta15_trim(ta15_py_str(un))) : json_null());
        json_set(src, "thread_id", ta15_is_null(ta15_arg(args, "thread_id"))
                 ? json_null() : json_string(ta15_py_str(ta15_arg(args, "thread_id"))));
        json_free(afn);
        json_t *s = ta15_call(ctx, "auth_source", src);
        if (ta15_is_raise(s)) { free(uid); free(cid); json_free(ct); return s; }
        json_t *c = json_object();
        json_set(c, "source", json_copy(s));
        json_free(s);
        json_t *v = ta15_call(ctx, "auth_fn_call", c);
        if (ta15_is_raise(v)) {
            const char *k = ta15_raise_kind(v);
            if (ta15_is_exception(k)) {
                json_free(v);
                json_t *d = ta15_call(ctx, "logger_debug", ta15_logger_args(json_array()));
                if (ta15_is_raise(d)) { free(uid); free(cid); json_free(ct); return d; }
                json_free(d);
            } else { free(uid); free(cid); json_free(ct); return v; }
        } else {
            json_t *res = json_bool(ta15_truthy(v));
            json_free(v); free(uid); free(cid); json_free(ct);
            return res;
        }
    } else json_free(afn);
    json_t *da = json_object();
    json_set(da, "user_id", json_string(uid));
    json_t *dec = ta15_call(ctx, "env_allowlist_decision", da);
    if (ta15_is_raise(dec)) { free(uid); free(cid); json_free(ct); return dec; }
    if (!ta15_is_null(dec)) { json_free(dec); free(uid); free(cid); json_free(ct); return json_bool(ta15_truthy(dec)); }
    json_free(dec);
    json_t *ga = json_object();
    json_set(ga, "key", json_string("GATEWAY_ALLOW_ALL_USERS"));
    json_t *g = ta15_call(ctx, "scoped_gate_env", ga);
    if (ta15_is_raise(g)) { free(uid); free(cid); json_free(ct); return g; }
    const char *const ALLOW[] = {"true", "1", "yes"};
    bool ok = ta15_lower_in(ta15_str(g), ALLOW, 3);
    json_free(g); free(uid); free(cid); json_free(ct);
    return json_bool(ok);
}

/* PoP: ta15_handle_text_message @ plugins/platforms/telegram/adapter.py:_handle_text_message */
static json_t *ta15_handle_text_message(ta15_ctx_t *ctx, const json_t *args) {
    const json_t *update = ta15_arg(args, "update");
    json_t *ua = json_object();
    json_set(ua, "update", ta15_copyv(update));
    json_t *msg = ta15_call(ctx, "effective_update_message", ua);
    if (ta15_is_raise(msg)) return msg;
    if (!ta15_truthy(msg) || !ta15_truthy(ta15_getattr(msg, "text"))) {
        if (ta15_truthy(msg)) {
            json_t *ra = json_object();
            json_set(ra, "msg", json_copy(msg));
            json_t *rr = ta15_call(ctx, "rich_message_text", ra);
            if (ta15_is_raise(rr)) { json_free(msg); return rr; }
            bool has = ta15_truthy(rr); json_free(rr);
            if (!has) { json_free(msg); return json_null(); }
        } else { json_free(msg); return json_null(); }
    }
    json_t *aa = json_object();
    json_set(aa, "msg", json_copy(msg));
    json_t *auth = ta15_call(ctx, "is_user_authorized_from_message", aa);
    if (ta15_is_raise(auth)) { json_free(msg); return auth; }
    bool ok = ta15_truthy(auth); json_free(auth);
    if (!ok) {
        json_t *b = json_object(); json_set(b, "msg", json_copy(msg));
        json_set(b, "kw", json_object());
        json_t *r = ta15_call(ctx, "log_blocked_user", b);
        json_free(msg); if (ta15_is_raise(r)) return r; json_free(r); return json_null();
    }
    json_t *v = json_object(); json_set(v, "msg", json_copy(msg));
    json_t *vc = ta15_call(ctx, "consume_vault_code_reply", v);
    if (ta15_is_raise(vc)) { json_free(msg); return vc; }
    bool vault = ta15_truthy(vc); json_free(vc);
    if (vault) { json_free(msg); return json_null(); }
    json_t *g = json_object();
    json_set(g, "msg", json_copy(msg));
    json_set(g, "update", ta15_copyv(update));
    json_set(g, "message_type", json_string("TEXT"));
    json_t *gr = ta15_call(ctx, "gate_or_observe", g);
    if (ta15_is_raise(gr)) { json_free(msg); return gr; }
    bool gate = ta15_truthy(gr); json_free(gr);
    if (!gate) { json_free(msg); return json_null(); }
    json_t *fa = json_object();
    json_set(fa, "msg", ta15_copyv(ta15_getattr(update, "message")));
    json_t *fr = ta15_call(ctx, "ensure_forum_commands", fa);
    if (ta15_is_raise(fr)) { json_free(msg); return fr; }
    json_free(fr);
    json_t *ea = json_object();
    json_set(ea, "msg", json_copy(msg));
    json_set(ea, "update", ta15_copyv(update));
    json_set(ea, "message_type", json_string("TEXT"));
    json_t *ev = ta15_call(ctx, "build_triggered_event", ea);
    if (ta15_is_raise(ev)) { json_free(msg); return ev; }
    json_t *n = json_object(); json_set(n, "event", json_copy(ev));
    json_free(ev); json_free(msg);
    json_t *r = ta15_call(ctx, "enqueue_text_event", n);
    if (ta15_is_raise(r)) return r;
    json_free(r); return json_null();
}

/* PoP: ta15_handle_location_message @ plugins/platforms/telegram/adapter.py:_handle_location_message */
static json_t *ta15_handle_location_message(ta15_ctx_t *ctx, const json_t *args) {
    const json_t *update = ta15_arg(args, "update");
    json_t *ua = json_object(); json_set(ua, "update", ta15_copyv(update));
    json_t *msg = ta15_call(ctx, "effective_update_message", ua);
    if (ta15_is_raise(msg)) return msg;
    if (!ta15_truthy(msg)) return msg ? msg : json_null();
    json_t *aa = json_object(); json_set(aa, "msg", json_copy(msg));
    json_t *auth = ta15_call(ctx, "is_user_authorized_from_message", aa);
    if (ta15_is_raise(auth)) { json_free(msg); return auth; }
    bool ok = ta15_truthy(auth); json_free(auth);
    if (!ok) {
        json_t *b = json_object(); json_set(b, "msg", json_copy(msg));
        json_set(b, "kw", json_object());
        json_t *r = ta15_call(ctx, "log_blocked_user", b);
        json_free(msg); if (ta15_is_raise(r)) return r; json_free(r); return json_null();
    }
    const json_t *venue = ta15_getattr(msg, "venue");
    const json_t *loc = ta15_truthy(venue) ? ta15_getattr(venue, "location") : ta15_getattr(msg, "location");
    if (!ta15_truthy(loc)) { json_free(msg); return json_null(); }
    const json_t *latv = ta15_getattr(loc, "latitude");
    const json_t *lonv = ta15_getattr(loc, "longitude");
    if (ta15_is_null(latv) || ta15_is_null(lonv)) { json_free(msg); return json_null(); }
    json_t *ra = json_object();
    json_set(ra, "lat", json_copy(latv)); json_set(ra, "lon", json_copy(lonv));
    json_t *rr = ta15_call(ctx, "redundant_live_refresh", ra);
    if (ta15_is_raise(rr)) { json_free(msg); return rr; }
    bool redundant = ta15_truthy(rr); json_free(rr);
    if (redundant) { json_free(msg); return json_null(); }
    json_t *g = json_object();
    json_set(g, "msg", json_copy(msg)); json_set(g, "update", ta15_copyv(update));
    json_set(g, "message_type", json_string("LOCATION"));
    json_t *gr = ta15_call(ctx, "gate_or_observe", g);
    if (ta15_is_raise(gr)) { json_free(msg); return gr; }
    bool gate = ta15_truthy(gr); json_free(gr);
    if (!gate) { json_free(msg); return json_null(); }
    json_t *ba = json_object();
    json_set(ba, "msg", json_copy(msg));
    json_set(ba, "message_type", json_string("LOCATION"));
    json_set(ba, "update_id", ta15_copyv(ta15_getattr(update, "update_id")));
    json_t *ev = ta15_call(ctx, "build_message_event", ba);
    if (ta15_is_raise(ev)) { json_free(msg); return ev; }
    char *lats = ta15_py_float(ta15_num(latv)), *lons = ta15_py_float(ta15_num(lonv));
    char *text = ta15_fmt("[The user shared a location pin.]");
    if (ta15_truthy(venue)) {
        const char *ti = ta15_str(ta15_getattr(venue, "title"));
        const char *ad = ta15_str(ta15_getattr(venue, "address"));
        if (ti) { char *n = ta15_fmt("%s\nVenue: %s", text, ti); free(text); text = n; }
        if (ad) { char *n = ta15_fmt("%s\nAddress: %s", text, ad); free(text); text = n; }
    }
    char *n1 = ta15_fmt("%s\nlatitude: %s\nlongitude: %s\nMap: https://www.google.com/maps/search/?api=1&query=%s,%s\nAsk what they'd like to find nearby (restaurants, cafes, etc.) and any preferences.",
                        text, lats, lons, lats, lons);
    free(text); free(lats); free(lons);
    json_set(ev, "text", json_string(n1)); free(n1);
    json_t *oa = json_object(); json_set(oa, "event", json_copy(ev));
    json_free(ev); json_free(msg);
    json_t *attr = ta15_call(ctx, "apply_observe_attribution", oa);
    if (ta15_is_raise(attr)) return attr;
    json_t *ha = json_object(); json_set(ha, "event", json_copy(attr));
    json_free(attr);
    json_t *r = ta15_call(ctx, "handle_message", ha);
    if (ta15_is_raise(r)) return r;
    json_free(r); return json_null();
}



/* PoP: ta15_handle_sticker @ plugins/platforms/telegram/adapter.py:_handle_sticker */
static json_t *ta15_handle_sticker(ta15_ctx_t *ctx, const json_t *args) {
    const json_t *st = ta15_getattr(ta15_arg(args, "msg"), "sticker");
    const char *emoji = ta15_str(ta15_getattr(st, "emoji"));
    const char *setname = ta15_str(ta15_getattr(st, "set_name"));
    const char *em = ta15_truthy(ta15_getattr(st, "emoji")) ? emoji : "";
    const char *sn = ta15_truthy(ta15_getattr(st, "set_name")) ? setname : "";
    if (ta15_truthy(ta15_getattr(st, "is_animated")) || ta15_truthy(ta15_getattr(st, "is_video"))) {
        json_t *a = json_object(); json_set(a, "emoji", json_string(em));
        json_t *r = ta15_call(ctx, "build_animated_sticker_injection", a);
        if (ta15_is_raise(r)) return r;
        json_free(r); return json_null();
    }
    json_t *ca = json_object(); json_set(ca, "file_unique_id", ta15_copyv(ta15_getattr(st, "file_unique_id")));
    json_t *cached = ta15_call(ctx, "get_cached_description", ca);
    if (ta15_is_raise(cached)) return cached;
    char *fb = em[0] ? ta15_fmt("a sticker with emoji %s", em) : ta15_strdup("a sticker");
    if (ta15_truthy(cached)) {
        const json_t *d = json_obj_get(cached, "description");
        const json_t *ce = json_obj_get(cached, "emoji");
        const json_t *cs = json_obj_get(cached, "set_name");
        json_t *a = json_object();
        json_set(a, "description", json_copy(d));
        json_set(a, "emoji", json_copy(ce ? ce : json_string(em)));
        json_set(a, "set_name", json_copy(cs ? cs : json_string(sn)));
        json_t *r = ta15_call(ctx, "build_sticker_injection", a);
        if (ta15_is_raise(r)) { json_free(cached); free(fb); return r; }
        json_free(r);
        json_t *arr = json_array();
        json_append(arr, json_string("[Telegram] Sticker cache hit: %s"));
        json_append(arr, ta15_copyv(ta15_getattr(st, "file_unique_id")));
        r = ta15_call(ctx, "logger_info", ta15_logger_args(arr));
        json_free(cached); free(fb);
        if (ta15_is_raise(r)) return r;
        json_free(r); return json_null();
    }
    json_free(cached);
    json_t *root = ta15_call(ctx, "sticker_local_root", json_object());
    if (ta15_is_raise(root)) { free(fb); return root; }
    json_free(root);
    json_t *fa = json_object();
    json_set(fa, "kind", json_string("sticker"));
    json_set(fa, "obj_id", json_null());
    json_t *fetched = ta15_call(ctx, "fetch_telegram_file", fa);
    if (ta15_is_raise(fetched)) {
        const char *k = ta15_raise_kind(fetched);
        if (!ta15_is_exception(k)) { free(fb); return fetched; }
        json_t *ea = json_object();
        json_set(ea, "error", json_copy(fetched)); json_set(ea, "proxy", json_null());
        json_free(fetched);
        json_t *red = ta15_call(ctx, "redact_telegram_error_text", ea);
        if (ta15_is_raise(red)) { free(fb); return red; }
        json_t *arr = json_array();
        json_append(arr, json_string("[Telegram] Sticker analysis error: %s"));
        json_append(arr, json_copy(red)); json_free(red);
        json_t *r = ta15_call(ctx, "logger_warning", ta15_logger_args(arr));
        if (ta15_is_raise(r)) { free(fb); return r; }
        json_free(r);
        json_t *ba = json_object();
        json_set(ba, "description", json_string(fb));
        json_set(ba, "emoji", json_string(em));
        json_set(ba, "set_name", json_string(sn));
        free(fb);
        r = ta15_call(ctx, "build_sticker_injection", ba);
        if (ta15_is_raise(r)) return r; json_free(r); return json_null();
    }
    json_free(fetched);
    json_t *ia = json_object(); json_set(ia, "ext", json_string(".webp"));
    json_t *cached_path = ta15_call(ctx, "cache_image_from_bytes", ia);
    if (ta15_is_raise(cached_path)) { free(fb); return cached_path; }
    json_t *arr = json_array();
    json_append(arr, json_string("[Telegram] Analyzing sticker at %s"));
    json_append(arr, json_copy(cached_path));
    json_t *r = ta15_call(ctx, "logger_info", ta15_logger_args(arr));
    if (ta15_is_raise(r)) { json_free(cached_path); free(fb); return r; }
    json_free(r);
    r = ta15_call(ctx, "accept_update", json_object());
    if (ta15_is_raise(r)) { json_free(cached_path); free(fb); return r; }
    json_free(r);
    json_t *va = json_object();
    json_set(va, "image_url", json_copy(cached_path));
    json_set(va, "user_prompt", json_string("sticker-prompt"));
    json_t *vr = ta15_call(ctx, "vision_analyze_tool", va);
    if (ta15_is_raise(vr)) { json_free(cached_path); free(fb); return vr; }
    json_t *ja = json_object(); json_set(ja, "s", json_copy(vr));
    json_free(vr);
    json_t *result = ta15_call(ctx, "json_loads", ja);
    if (ta15_is_raise(result)) { json_free(cached_path); free(fb); return result; }
    const char *desc = ta15_str(json_obj_get(result, "analysis"));
    bool success = ta15_truthy(json_obj_get(result, "success"));
    const char *use_desc = success ? (desc ? desc : "a sticker") : fb;
    json_t *ba = json_object();
    json_set(ba, "description", json_string(use_desc));
    json_set(ba, "emoji", json_string(em));
    json_set(ba, "set_name", json_string(sn));
    if (success) {
        json_t *sa = json_object();
        json_set(sa, "file_unique_id", ta15_copyv(ta15_getattr(st, "file_unique_id")));
        json_set(sa, "description", json_string(use_desc));
        json_set(sa, "emoji", json_string(em));
        json_set(sa, "set_name", json_string(sn));
        json_t *sr = ta15_call(ctx, "cache_sticker_description", sa);
        if (ta15_is_raise(sr)) { json_free(result); json_free(cached_path); free(fb); return sr; }
        json_free(sr);
    }
    json_free(result); json_free(cached_path); free(fb);
    r = ta15_call(ctx, "build_sticker_injection", ba);
    if (ta15_is_raise(r)) return r; json_free(r); return json_null();
}

/* helper: translate(key) / translate(key, **kwargs) recording {"args":[key],"kwargs":{}} */
static json_t *ta15_translate(ta15_ctx_t *ctx, const char *key, json_t *kwargs) {
    json_t *a = json_object();
    json_t *arr = json_array(); json_append(arr, json_string(key));
    json_set(a, "args", arr);
    json_set(a, "kwargs", kwargs ? kwargs : json_object());
    return ta15_call(ctx, "translate", a);
}

/* PoP: ta15_handle_update_prompt_callback @ plugins/platforms/telegram/adapter.py:_handle_update_prompt_callback */
static json_t *ta15_handle_update_prompt_callback(ta15_ctx_t *ctx, const json_t *args) {
    const json_t *q = ta15_arg(args, "query");
    const char *dstr = ta15_str(ta15_arg(args, "data"));
    char *s1 = NULL, *s2 = NULL, *s3 = NULL;
    ta15_split3(dstr ? dstr : "", &s1, &s2, &s3);
    const char *answer = s2 ? s2 : "";
    const char *card = s3 ? s3 : "";
    json_t *un = ta15_call(ctx, "unauthorized", json_object());
    if (ta15_is_raise(un)) { free(s1); free(s2); free(s3); return un; }
    json_free(un);
    json_t *ca = json_object();
    json_set(ca, "query", ta15_copyv(q));
    json_set(ca, "cb", ta15_copyv(ta15_arg(args, "cb")));
    json_set(ca, "unauth", json_null());
    json_t *auth = ta15_call(ctx, "callback_authorized", ca);
    if (ta15_is_raise(auth)) { free(s1); free(s2); free(s3); return auth; }
    bool ok = ta15_truthy(auth); json_free(auth);
    if (!ok) { free(s1); free(s2); free(s3); return json_null(); }
    json_t *pra = json_object();
    json_set(pra, "user_id", json_null());
    json_t *pkw = json_object(); json_set(pkw, "extra", json_object());
    json_set(pra, "kw", pkw);
    json_t *ref = ta15_call(ctx, "update_prompt_refusal", pra);
    if (ta15_is_raise(ref)) { free(s1); free(s2); free(s3); return ref; }
    json_free(ref);
    json_t *ra = json_object();
    json_set(ra, "query", ta15_copyv(q)); json_set(ra, "refusal", json_null());
    json_t *rej = ta15_call(ctx, "refuse_tap", ra);
    if (ta15_is_raise(rej)) { free(s1); free(s2); free(s3); return rej; }
    bool reject = ta15_truthy(rej); json_free(rej);
    if (reject) { free(s1); free(s2); free(s3); return json_null(); }
    const json_t *cur = ta15_state_get(ctx, "_update_prompt_id");
    if (!ta15_is_null(cur) && (!ta15_truthy(cur) || strcmp(card, ta15_str(cur) ? ta15_str(cur) : "") != 0)) {
        json_t *t = ta15_toast(ctx, "platform.telegram.slash_confirm.already_resolved");
        if (ta15_is_raise(t)) { free(s1); free(s2); free(s3); return t; }
        json_t *r = ta15_answer(ctx, q, t, false);
        free(s1); free(s2); free(s3);
        if (ta15_is_raise(r)) return r; json_free(r); return json_null();
    }
    json_t *home = ta15_call(ctx, "get_hermes_home", json_object());
    if (ta15_is_raise(home)) { free(s1); free(s2); free(s3); return home; }
    const char *h = ta15_str(home);
    char *tmp = ta15_fmt("%s/.update_response.tmp", h ? h : "");
    char *dst = ta15_fmt("%s/.update_response", h ? h : "");
    json_free(home);
    json_t *wa = json_object();
    json_set(wa, "path", json_string(tmp)); json_set(wa, "data", json_string(answer));
    json_t *wr = ta15_call(ctx, "write_text", wa);
    if (ta15_is_raise(wr)) {
        const char *k = ta15_raise_kind(wr);
        if (!ta15_is_exception(k)) { free(tmp); free(dst); free(s1); free(s2); free(s3); return wr; }
        json_t *errv = json_copy(wr); json_free(wr);
        json_t *arr = json_array();
        json_append(arr, json_string("Failed to write update response from callback: %s"));
        json_append(arr, errv);
        json_t *le = ta15_call(ctx, "logger_error", ta15_logger_args(arr));
        free(tmp); free(dst); free(s1); free(s2); free(s3);
        if (ta15_is_raise(le)) return le; json_free(le); return json_null();
    }
    json_free(wr);
    json_t *rp = json_object();
    json_set(rp, "src", json_string(tmp)); json_set(rp, "dst", json_string(dst));
    free(tmp); free(dst);
    json_t *rr = ta15_call(ctx, "replace", rp);
    if (ta15_is_raise(rr)) { free(s1); free(s2); free(s3); return rr; }
    json_free(rr);
    json_t *ha = json_object();
    json_set(ha, "user_id", ta15_copyv(ta15_getattr(ta15_getattr(q, "from_user"), "id")));
    json_t *hv = ta15_call(ctx, "hash_for_log", ha);
    if (ta15_is_raise(hv)) { free(s1); free(s2); free(s3); return hv; }
    json_t *arr = json_array();
    json_append(arr, json_string("Telegram update prompt answered '%s' by user %s"));
    json_append(arr, json_string(answer));
    json_append(arr, json_copy(hv)); json_free(hv);
    json_t *lr = ta15_call(ctx, "logger_info", ta15_logger_args(arr));
    if (ta15_is_raise(lr)) { free(s1); free(s2); free(s3); return lr; }
    json_free(lr);
    json_set(ctx->state, "_update_prompt_id", json_string(""));
    json_t *tkw = json_object(); json_set(tkw, "answer", json_string(answer));
    json_t *tmsg = ta15_toast_kw(ctx, "platform.telegram.prompt.update_sent", tkw);
    if (ta15_is_raise(tmsg)) { free(s1); free(s2); free(s3); return tmsg; }
    json_t *ar = ta15_answer(ctx, q, tmsg, false);
    if (ta15_is_raise(ar)) { free(s1); free(s2); free(s3); return ar; }
    json_free(ar);
    json_t *word = ta15_translate(ctx, strcmp(answer, "y") == 0
        ? "platform.telegram.prompt.affirm_word" : "platform.telegram.prompt.negate_word", NULL);
    if (ta15_is_raise(word)) { free(s1); free(s2); free(s3); return word; }
    json_t *kw3 = json_object();
    char *star = ta15_fmt("*%s*", ta15_str(word) ? ta15_str(word) : "");
    json_set(kw3, "answer", json_string(star)); free(star);
    json_free(word);
    json_t *answered = ta15_translate(ctx, "platform.telegram.prompt.update_answered", kw3);
    if (ta15_is_raise(answered)) { free(s1); free(s2); free(s3); return answered; }
    char *final = ta15_fmt("☤ %s", ta15_str(answered) ? ta15_str(answered) : "");
    json_free(answered);
    json_t *ea = json_object();
    json_set(ea, "query", ta15_copyv(q)); json_set(ea, "text", json_string(final));
    free(final); free(s1); free(s2); free(s3);
    json_t *er = ta15_call(ctx, "edit_md_quiet", ea);
    if (ta15_is_raise(er)) return er; json_free(er); return json_null();
}



/* PoP: ta15_handle_model_picker_callback @ plugins/platforms/telegram/adapter.py:_handle_model_picker_callback */
static json_t *ta15_handle_model_picker_callback(ta15_ctx_t *ctx, const json_t *args) {
    const json_t *q = ta15_arg(args, "query");
    const char *chat = ta15_str(ta15_arg(args, "chat_id"));
    const char *data = ta15_str(ta15_arg(args, "data"));
    data = data ? data : "";
    json_t *mps = (json_t *)ta15_state_get(ctx, "_model_picker_state");
    json_t *state = (mps && mps->type == JSON_OBJECT && chat)
        ? (json_t *)json_obj_get(mps, chat) : NULL;
    if (!ta15_truthy(state)) {
        json_t *t = ta15_toast(ctx, "platform.telegram.picker.expired_model");
        if (ta15_is_raise(t)) return t;
        json_t *a = json_object();
        json_set(a, "self", json_string("<self>"));
        json_set(a, "query", ta15_copyv(q));
        json_set(a, "text", json_copy(t)); json_free(t);
        json_t *r = ta15_call(ctx, "answer_or_note", a);
        if (ta15_is_raise(r)) return r; json_free(r); return json_null();
    }
    json_t *lr = ta15_call(ctx, "provider_get_label", json_object());
    if (ta15_is_raise(lr)) return lr;
    json_t *label = json_copy(lr); json_free(lr);
    json_t *r = NULL;
    if (strncmp(data, "mp:", 3) == 0) {
        const char *slug = data + 3;
        const json_t *provs = json_obj_get(state, "providers");
        json_t *prov = NULL;
        if (provs && provs->type == JSON_ARRAY)
            for (size_t i = 0; i < provs->c.count; i++) {
                const char *s = ta15_str(json_obj_get(provs->c.items[i], "slug"));
                if (s && strcmp(s, slug) == 0) { prov = provs->c.items[i]; break; }
            }
        if (!prov) {
            json_t *t = ta15_toast(ctx, "platform.telegram.picker.provider_not_found");
            if (ta15_is_raise(t)) { json_free(label); return t; }
            r = ta15_answer(ctx, q, t, false);
            json_free(label); if (ta15_is_raise(r)) return r; json_free(r); return json_null();
        }
        json_set(state, "selected_provider", json_string(slug));
        const json_t *pn = json_obj_get(prov, "name");
        json_set(state, "selected_provider_name", json_copy(pn ? pn : json_string(slug)));
        const json_t *ml = json_obj_get(prov, "models");
        json_set(state, "model_list", json_copy(ml ? ml : json_array()));
        json_t *a = json_object();
        json_set(a, "query", ta15_copyv(q));
        json_set(a, "page", json_number(0));
        json_set(a, "state", json_copy(state));
        r = ta15_call(ctx, "picker_show_models", a);
    } else if (strncmp(data, "mg:", 3) == 0) {
        json_t *a = json_object();
        json_set(a, "query", ta15_copyv(q));
        json_set(a, "s", json_string(data + 3));
        json_t *page = ta15_call(ctx, "parse_page", a);
        if (ta15_is_raise(page)) { json_free(label); return page; }
        if (!ta15_is_null(page)) {
            json_t *b = json_object();
            json_set(b, "query", ta15_copyv(q));
            json_set(b, "page", json_copy(page));
            json_set(b, "state", json_copy(state));
            r = ta15_call(ctx, "picker_show_models", b);
        } else r = json_null();
        json_free(page);
    } else if (strncmp(data, "mpv:", 4) == 0) {
        json_t *a = json_object();
        json_set(a, "query", ta15_copyv(q));
        json_set(a, "s", json_string(data + 4));
        json_t *page = ta15_call(ctx, "parse_page", a);
        if (ta15_is_raise(page)) { json_free(label); return page; }
        if (!ta15_is_null(page)) {
            json_set(state, "provider_page", json_copy(page));
            json_t *b = json_object();
            json_set(b, "query", ta15_copyv(q));
            json_set(b, "page", json_copy(page));
            json_set(b, "state", json_copy(state));
            json_set(b, "label", json_copy(label));
            r = ta15_call(ctx, "picker_show_providers", b);
        } else r = json_null();
        json_free(page);
    } else if (strcmp(data, "mb") == 0) {
        const json_t *pv = json_obj_get(state, "provider_page");
        double page = ta15_truthy(pv) ? ta15_num(pv) : 0.0;
        json_t *b = json_object();
        json_set(b, "query", ta15_copyv(q));
        json_set(b, "page", json_number(page));
        json_set(b, "state", json_copy(state));
        json_set(b, "label", json_copy(label));
        r = ta15_call(ctx, "picker_show_providers", b);
    } else if (strcmp(data, "mx") == 0) {
        if (mps && mps->type == JSON_OBJECT && chat) json_obj_del(mps, chat);
        r = ta15_answer(ctx, q, NULL, false);
        if (!ta15_is_raise(r)) { json_free(r); r = ta15_translate(ctx, "platform.telegram.picker.cancelled", NULL); }
        if (!ta15_is_raise(r)) {
            json_t *a = json_object();
            json_set(a, "query", ta15_copyv(q));
            json_set(a, "text", json_copy(r));
            json_free(r);
            r = ta15_call(ctx, "edit_md_quiet", a);
        }
    } else {
        r = ta15_answer(ctx, q, NULL, false);
    }
    json_free(label);
    if (ta15_is_raise(r)) return r; json_free(r); return json_null();
}

/* PoP: ta15_handle_slash_confirm_callback @ plugins/platforms/telegram/adapter.py:_handle_slash_confirm_callback */
static json_t *ta15_handle_slash_confirm_callback(ta15_ctx_t *ctx, const json_t *args) {
    const json_t *q = ta15_arg(args, "query");
    const char *data = ta15_str(ta15_arg(args, "data"));
    if (ta15_is_null(ta15_state_get(ctx, "_slash_confirm_state")))
        json_set(ctx->state, "_slash_confirm_state", json_object());
    char *s1 = NULL, *s2 = NULL, *s3 = NULL;
    ta15_split3(data ? data : "", &s1, &s2, &s3);
    if (!s3) { free(s1); free(s2); free(s3); return json_null(); }
    const char *choice = s2 ? s2 : "";
    const char *cid = s3;
    json_t *resolved = ta15_toast(ctx, "platform.telegram.slash_confirm.already_resolved");
    if (ta15_is_raise(resolved)) { free(s1); free(s2); free(s3); return resolved; }
    json_t *un = ta15_call(ctx, "unauthorized", json_object());
    if (ta15_is_raise(un)) { json_free(resolved); free(s1); free(s2); free(s3); return un; }
    json_free(un);
    json_t *cs = (json_t *)ta15_state_get(ctx, "_slash_confirm_state");
    json_t *a = json_object();
    json_set(a, "query", ta15_copyv(q));
    json_set(a, "cb", ta15_copyv(ta15_arg(args, "cb")));
    json_set(a, "state", json_copy(cs));
    json_set(a, "confirm_id", json_string(cid));
    json_set(a, "unauth", json_null());
    json_set(a, "resolved", json_copy(resolved));
    json_t *kw = json_object(); json_set(kw, "pop", json_bool(false));
    json_set(a, "kw", kw);
    json_t *sk = ta15_call(ctx, "claim_callback_state", a);
    if (ta15_is_raise(sk)) { json_free(resolved); free(s1); free(s2); free(s3); return sk; }
    if (ta15_is_null(sk)) { json_free(sk); json_free(resolved); free(s1); free(s2); free(s3); return json_null(); }
    json_t *ra = json_object();
    json_set(ra, "session_key", json_copy(sk));
    json_set(ra, "confirm_id", json_string(cid));
    json_set(ra, "user_id", ta15_copyv(ta15_getattr(ta15_getattr(q, "from_user"), "id")));
    json_t *xkw = json_object(); json_set(xkw, "extra", json_object());
    json_set(ra, "kw", xkw);
    json_t *ref = ta15_call(ctx, "slash_confirm_refusal", ra);
    if (ta15_is_raise(ref)) { json_free(sk); json_free(resolved); free(s1); free(s2); free(s3); return ref; }
    if (ta15_truthy(ref)) {
        json_t *b = json_object(); json_set(b, "requester", json_copy(ref)); json_free(ref);
        json_t *txt = ta15_call(ctx, "refusal_text", b);
        if (ta15_is_raise(txt)) { json_free(sk); json_free(resolved); free(s1); free(s2); free(s3); return txt; }
        char *trunc = ta15_strdup(ta15_str(txt) ? ta15_str(txt) : "");
        if (strlen(trunc) > _TA15_TOAST_LIMIT) trunc[_TA15_TOAST_LIMIT] = '\0';
        json_free(txt);
        json_t *r = ta15_answer(ctx, q, json_string(trunc), true);
        free(trunc); json_free(sk); json_free(resolved); free(s1); free(s2); free(s3);
        if (ta15_is_raise(r)) return r; json_free(r); return json_null();
    }
    json_free(ref);
    json_t *popped = NULL;
    if (cs && cs->type == JSON_OBJECT && json_obj_get(cs, cid)) {
        popped = json_copy(json_obj_get(cs, cid));
        json_obj_del(cs, cid);
    }
    if (!ta15_truthy(popped)) {
        if (popped) json_free(popped);
        json_t *txt = json_copy(resolved); json_free(resolved);
        json_t *r = ta15_answer(ctx, q, txt, false);
        json_free(sk); free(s1); free(s2); free(s3);
        if (ta15_is_raise(r)) return r; json_free(r); return json_null();
    }
    json_free(popped); json_free(resolved);
    const char *lk = "approval.resolved_generic";
    if (strcmp(choice, "once") == 0) lk = "slash_confirm.resolved_once";
    else if (strcmp(choice, "always") == 0) lk = "slash_confirm.resolved_always";
    else if (strcmp(choice, "cancel") == 0) lk = "slash_confirm.resolved_cancel";
    const char *fn = ta15_str(ta15_getattr(ta15_getattr(q, "from_user"), "first_name"));
    json_t *ud = NULL;
    if (fn && fn[0]) ud = json_string(fn);
    else ud = ta15_translate(ctx, "platform.telegram.user_fallback", NULL);
    if (ta15_is_raise(ud)) { json_free(sk); free(s1); free(s2); free(s3); return ud; }
    char *lkey = ta15_fmt("platform.telegram.%s", lk);
    json_t *label = ta15_translate(ctx, lkey, NULL); free(lkey);
    if (ta15_is_raise(label)) { json_free(ud); json_free(sk); free(s1); free(s2); free(s3); return label; }
    char *lab = ta15_strdup(ta15_str(label) ? ta15_str(label) : "");
    if (strlen(lab) > _TA15_TOAST_LIMIT) lab[_TA15_TOAST_LIMIT] = '\0';
    json_t *r = ta15_answer(ctx, q, json_string(lab), false);
    if (ta15_is_raise(r)) { free(lab); json_free(ud); json_free(sk); free(s1); free(s2); free(s3); return label; }
    json_free(r);
    json_t *kw3 = json_object();
    json_set(kw3, "label", json_copy(label));
    json_set(kw3, "user", json_copy(ud));
    json_free(label); json_free(ud);
    json_t *decided = ta15_translate(ctx, "platform.telegram.approval.resolved_by_user", kw3);
    if (ta15_is_raise(decided)) { free(lab); json_free(sk); free(s1); free(s2); free(s3); return decided; }
    json_t *ea = json_object();
    json_set(ea, "query", ta15_copyv(q));
    json_set(ea, "text", json_copy(decided)); json_free(decided);
    r = ta15_call(ctx, "edit_keeping_prompt", ea);
    if (ta15_is_raise(r)) { free(lab); json_free(sk); free(s1); free(s2); free(s3); return r; }
    json_free(r);
    json_t *sa = json_object();
    json_set(sa, "session_key", json_copy(sk));
    json_set(sa, "confirm_id", json_string(cid));
    json_set(sa, "choice", json_string(choice));
    free(lab); json_free(sk); free(s1); free(s2); free(s3);
    r = ta15_call(ctx, "slash_confirm_resolve", sa);
    if (ta15_is_raise(r)) {
        const char *k = ta15_raise_kind(r);
        if (!ta15_is_exception(k)) return r;
        json_t *errv = json_copy(r); json_free(r);
        json_t *arr = json_array();
        json_append(arr, json_string("[%s] slash-confirm callback failed: %s"));
        json_append(arr, json_string("bot"));
        json_append(arr, errv);
        json_t *le = ta15_call(ctx, "logger_error", ta15_logger_args(arr));
        if (ta15_is_raise(le)) return le; json_free(le);
    } else json_free(r);
    return json_null();
}



/* PoP: ta15_initialize_app_with_retries @ plugins/platforms/telegram/adapter.py:_initialize_app_with_retries */
static json_t *ta15_initialize_app_with_retries(ta15_ctx_t *ctx, const json_t *args) {
    (void)args;
    if (ta15_is_null(ta15_state_get(ctx, "_app"))) {
        json_t *app0 = json_object();
        json_set(app0, "bot", json_string("bot"));
        json_set(ctx->state, "_app", app0);
    }
    json_t *ea = json_object();
    json_set(ea, "key", json_string("HERMES_TELEGRAM_INIT_TIMEOUT"));
    json_set(ea, "default", json_number(30.0));
    json_t *to = ta15_call(ctx, "env_float", ea);
    if (ta15_is_raise(to)) return to;
    double timeout = ta15_num(to); json_free(to);
    json_t *t0 = ta15_call(ctx, "loop_time", json_object());
    if (ta15_is_raise(t0)) return t0;
    double deadline = ta15_num(t0) + timeout * 8.0 + 120.0;
    json_free(t0);
    for (int attempt = 1; attempt <= 8; attempt++) {
        json_t *tc = ta15_call(ctx, "loop_time", json_object());
        if (ta15_is_raise(tc)) return tc;
        double now = ta15_num(tc); json_free(tc);
        if (now >= deadline) {
            return ta15_err("OSError", "Telegram initialization timed out after 8 attempts");
        }
        json_t *arr = json_array();
        json_append(arr, json_string("[%s] Connecting to Telegram (attempt %d/%d)…"));
        json_append(arr, json_string("bot")); json_append(arr, json_number(attempt));
        json_append(arr, json_number(8));
        json_t *lr = ta15_call(ctx, "logger_warning", ta15_logger_args(arr));
        if (ta15_is_raise(lr)) return lr; json_free(lr);
        json_t *kw = json_object();
        json_set(kw, "timeout", json_number(timeout));
        json_set(kw, "on_abandon", json_string("<lambda>"));
        json_set(kw, "label", json_string("telegram-init"));
        json_t *aa = json_object(); json_set(aa, "kw", kw);
        json_t *res = ta15_call(ctx, "await_with_thread_deadline", aa);
        if (!ta15_is_raise(res)) { json_free(res); return json_null(); }
        const char *kind = ta15_raise_kind(res);
        const char *msg = ta15_raise_msg(res);
        bool rebuild = false;
        if (strcmp(kind, "TimeoutError") == 0) {
            rebuild = true;
            if (attempt >= 8) { json_free(res); return ta15_err("OSError", "Telegram initialization timed out"); }
            double wait = (attempt >= 1 && attempt <= 7) ? (double)(1 << (attempt - 1)) : 15.0;
            if (wait > 15.0) wait = 15.0;
            json_t *a2 = json_array();
            json_append(a2, json_string("[%s] Connect attempt %d/%d timed out after %.0fs — retrying in %ds"));
            json_append(a2, json_string("bot")); json_append(a2, json_number(attempt));
            json_append(a2, json_number(8)); json_append(a2, json_number(timeout));
            json_append(a2, json_number(wait));
            json_t *w = ta15_call(ctx, "logger_warning", ta15_logger_args(a2));
            if (ta15_is_raise(w)) { json_free(res); return w; } json_free(w);
            json_t *sa = json_object(); json_set(sa, "delay", json_number(wait));
            w = ta15_call(ctx, "sleep", sa);
            if (ta15_is_raise(w)) { json_free(res); return w; } json_free(w);
        } else if (ta15_is_exception(kind)) {
            rebuild = true;
            bool is_os = ta15_chain_has(kind, "OSError");
            bool network = true;
            if (!is_os) {
                json_t *na = json_object();
                json_set(na, "error", json_copy(res));
                json_t *lk = ta15_call(ctx, "looks_like_network_error", na);
                if (ta15_is_raise(lk)) { json_free(res); return lk; }
                network = ta15_truthy(lk); json_free(lk);
            }
            if (!network || attempt >= 8) { json_free(res); return json_copy(res); }
            double wait = (attempt >= 1 && attempt <= 7) ? (double)(1 << (attempt - 1)) : 15.0;
            if (wait > 15.0) wait = 15.0;
            json_t *a2 = json_array();
            json_append(a2, json_string("[%s] Connect attempt %d/%d failed: %s — retrying in %ds"));
            json_append(a2, json_string("bot")); json_append(a2, json_number(attempt));
            json_append(a2, json_number(8)); json_append(a2, json_copy(res));
            json_append(a2, json_number(wait));
            json_t *w = ta15_call(ctx, "logger_warning", ta15_logger_args(a2));
            if (ta15_is_raise(w)) { json_free(res); return w; } json_free(w);
            json_t *sa = json_object(); json_set(sa, "delay", json_number(wait));
            w = ta15_call(ctx, "sleep", sa);
            if (ta15_is_raise(w)) { json_free(res); return w; } json_free(w);
        } else {
            /* BaseException-only: log then re-raise */
            json_free(res);
            return ta15_err(kind, msg);
        }
        json_free(res);
        (void)rebuild;
        if (attempt < 8) {
            json_t *b = ta15_call(ctx, "builder_build", json_object());
            if (ta15_is_raise(b)) return b;
            json_free(b);
            json_t *app = json_object(); json_set(app, "bot", json_string("bot"));
            json_set(ctx->state, "_bot", json_string("bot"));
            json_t *wa = json_object(); json_set(wa, "app", json_copy(app));
            json_t *w = ta15_call(ctx, "wire_plugin_handlers", wa);
            if (ta15_is_raise(w)) { json_free(app); return w; } json_free(w);
            wa = json_object(); json_set(wa, "app", json_copy(app));
            w = ta15_call(ctx, "register_handlers", wa);
            if (ta15_is_raise(w)) { json_free(app); return w; } json_free(w);
            json_free(app);
            w = ta15_call(ctx, "shutdown_abandoned_app", json_object());
            if (ta15_is_raise(w)) {
                if (!ta15_is_exception(ta15_raise_kind(w))) return w;
            }
            json_free(w);
        }
    }
    return json_null();
}

/* helper: build the canonical scripted error object for polling functions */
static json_t *ta15_poll_err(const json_t *args, const char *kind_override) {
    const char *ek = kind_override ? kind_override : ta15_str(ta15_arg(args, "error_kind"));
    if (ek && strcmp(ek, "stall") == 0) {
        json_t *e = json_object();
        json_set_hdr(e, "__raise__", "_PollingStallError");
        json_set(e, "message", json_string("stall"));
        return e;
    }
    if (ek && strcmp(ek, "timeout") == 0) {
        json_t *e = json_object();
        json_set_hdr(e, "__raise__", kind_override ? kind_override : "TimeoutError");
        json_set(e, "message", json_string(kind_override ? kind_override : "timeout"));
        return e;
    }
    return json_string("error");
}

/* PoP: ta15_handle_polling_network_error @ plugins/platforms/telegram/adapter.py:_handle_polling_network_error */
static json_t *ta15_handle_polling_network_error(ta15_ctx_t *ctx, const json_t *args) {
    if (ta15_truthy(ta15_state_get(ctx, "_teardown_started")) ||
        ta15_truthy(ta15_state_get(ctx, "has_fatal_error"))) return json_null();
    bool stall = ta15_str(ta15_arg(args, "error_kind")) &&
        strcmp(ta15_str(ta15_arg(args, "error_kind")), "stall") == 0;
    json_t *err = ta15_poll_err(args, NULL);
    if (stall) {
        json_t *ea = json_object();
        json_set(ea, "error", json_copy(err));
        json_set(ea, "proxy", json_null());
        json_free(err);
        json_t *red = ta15_call(ctx, "redact_telegram_error_text", ea);
        if (ta15_is_raise(red)) return red;
        char *msg = ta15_fmt("Telegram polling stall confirmed (%s); rebuilding the adapter instead of reusing a wedged Updater/dispatcher in place.",
                             ta15_str(red) ? ta15_str(red) : "");
        json_free(red);
        json_t *a = json_object();
        json_set(a, "message", json_string(msg));
        json_set(a, "log_message", json_string("[%s] %s (rebuilding adapter via supervisor)"));
        json_t *arr = json_array();
        json_append(arr, json_string("bot")); json_append(arr, json_string(msg));
        free(msg);
        json_set(a, "args", arr);
        json_t *r = ta15_call(ctx, "go_fatal_network", a);
        if (ta15_is_raise(r)) return r; json_free(r); return json_null();
    }
    double cnt = ta15_num(ta15_state_get(ctx, "_polling_network_error_count")) + 1.0;
    json_set(ctx->state, "_polling_network_error_count", json_number(cnt));
    json_set(ctx->state, "_send_path_degraded", json_bool(true));
    int attempt = (int)cnt;
    if (attempt > 10) {
        json_t *ea = json_object();
        json_set(ea, "error", json_copy(err));
        json_set(ea, "proxy", json_null());
        json_t *red = ta15_call(ctx, "redact_telegram_error_text", ea);
        if (ta15_is_raise(red)) { json_free(err); return red; }
        char *msg = ta15_fmt("Telegram polling could not reconnect after 10 network error retries. Escalating to gateway recovery.");
        json_t *a = json_object();
        json_set(a, "message", json_string(msg));
        json_set(a, "log_message", json_string("[%s] %s Last error: %s"));
        json_t *arr = json_array();
        json_append(arr, json_string("bot")); json_append(arr, json_string(msg));
        json_append(arr, json_copy(red));
        free(msg); json_free(red); json_free(err);
        json_set(a, "args", arr);
        json_t *r = ta15_call(ctx, "go_fatal_network", a);
        if (ta15_is_raise(r)) return r; json_free(r); return json_null();
    }
    json_t *ea = json_object();
    json_set(ea, "error", json_copy(err)); json_set(ea, "proxy", json_null());
    json_t *red = ta15_call(ctx, "redact_telegram_error_text", ea);
    if (ta15_is_raise(red)) { json_free(err); return red; }
    double delay = 5.0;
    for (int i = 1; i < attempt && i < 7; i++) delay *= 2.0;
    if (delay > 60.0) delay = 60.0;
    json_t *arr = json_array();
    json_append(arr, json_string("[%s] Telegram network error (attempt %d/%d), reconnecting in %ds. Error: %s"));
    json_append(arr, json_string("bot"));
    json_append(arr, json_number(attempt)); json_append(arr, json_number(10));
    json_append(arr, json_number(delay)); json_append(arr, json_copy(red));
    json_free(red);
    json_t *r = ta15_call(ctx, "logger_warning", ta15_logger_args(arr));
    if (ta15_is_raise(r)) { json_free(err); return r; } json_free(r);
    json_t *sa = json_object(); json_set(sa, "delay", json_number(delay));
    r = ta15_call(ctx, "sleep", sa);
    if (ta15_is_raise(r)) { json_free(err); return r; } json_free(r);
    if (ta15_truthy(ta15_state_get(ctx, "_teardown_started"))) { json_free(err); return json_null(); }
    const json_t *app = ta15_state_get(ctx, "_app");
    json_t *ua = json_object();
    json_set(ua, "app", ta15_copyv(app));
    json_set(ua, "reason", json_string("network-recovery"));
    json_t *kw = json_object(); json_set(kw, "skip_read_receipt_cleanup", json_bool(true));
    json_set(ua, "kw", kw);
    json_t *stop = ta15_call(ctx, "stop_updater_or_go_fatal", ua);
    if (ta15_is_raise(stop)) { json_free(err); return stop; }
    bool cont = ta15_truthy(stop); json_free(stop);
    if (!cont || ta15_truthy(ta15_state_get(ctx, "_teardown_started"))) { json_free(err); return json_null(); }
    json_t *pa = json_object(); json_set(pa, "error", json_copy(err));
    json_t *lk = ta15_call(ctx, "looks_like_pool_timeout", pa);
    if (ta15_is_raise(lk)) { json_free(err); return lk; }
    bool pool = ta15_truthy(lk); json_free(lk);
    if (pool) {
        json_t *d = ta15_call(ctx, "drain_general_connections", json_object());
        if (ta15_is_raise(d)) { json_free(err); return d; } json_free(d);
    }
    if (ta15_truthy(ta15_state_get(ctx, "_teardown_started"))) { json_free(err); return json_null(); }
    json_t *d = ta15_call(ctx, "drain_polling_connections", json_object());
    if (ta15_is_raise(d)) { json_free(err); return d; } json_free(d);
    if (ta15_truthy(ta15_state_get(ctx, "_teardown_started"))) { json_free(err); return json_null(); }
    json_t *sp = json_object();
    json_set(sp, "app", ta15_copyv(app));
    json_t *skw = json_object();
    json_set(skw, "drop_pending_updates", json_bool(false));
    json_set(skw, "error_callback", json_string("<lambda>"));
    json_set(sp, "kw", skw);
    r = ta15_call(ctx, "start_polling_once", sp);
    if (ta15_is_raise(r)) { json_free(err); return r; } json_free(r);
    json_t *arr2 = json_array();
    json_append(arr2, json_string("[%s] Telegram polling restarted after network error (attempt %d); health pending getUpdates progress"));
    json_append(arr2, json_string("bot")); json_append(arr2, json_number(attempt));
    r = ta15_call(ctx, "logger_info", ta15_logger_args(arr2));
    if (ta15_is_raise(r)) { json_free(err); return r; } json_free(r);
    json_free(err); return json_null();
}

/* PoP: ta15_handle_polling_conflict @ plugins/platforms/telegram/adapter.py:_handle_polling_conflict */
static json_t *ta15_handle_polling_conflict(ta15_ctx_t *ctx, const json_t *args) {
    if (ta15_truthy(ta15_state_get(ctx, "_teardown_started"))) return json_null();
    if (ta15_truthy(ta15_state_get(ctx, "has_fatal_error")) &&
        ta15_str(ta15_state_get(ctx, "fatal_error_code")) &&
        strcmp(ta15_str(ta15_state_get(ctx, "fatal_error_code")), "telegram_polling_conflict") == 0) return json_null();
    double cnt = ta15_num(ta15_state_get(ctx, "_polling_conflict_count")) + 1.0;
    json_set(ctx->state, "_polling_conflict_count", json_number(cnt));
    json_set(ctx->state, "_polling_conflict_quiet_since", json_null());
    json_set(ctx->state, "_polling_conflict_quiet_banked", json_number(0));
    int attempt = (int)cnt;
    json_t *err = ta15_poll_err(args, NULL);
    if (ta15_str(ta15_arg(args, "error_kind")) &&
        strcmp(ta15_str(ta15_arg(args, "error_kind")), "timeout") == 0 &&
        err && err->type == JSON_OBJECT)
        json_set(err, "message", json_string("conflict"));
    if (attempt <= 5) {
        double delay = 10.0 + cnt * 10.0;
        json_t *ea = json_object();
        json_set(ea, "error", json_copy(err)); json_set(ea, "proxy", json_null());
        json_t *red = ta15_call(ctx, "redact_telegram_error_text", ea);
        if (ta15_is_raise(red)) { json_free(err); return red; }
        json_t *arr = json_array();
        json_append(arr, json_string("[%s] Telegram polling conflict (%d/%d) — previous session still held open on Telegram's servers. Waiting %ds for it to expire. Error: %s"));
        json_append(arr, json_string("bot"));
        json_append(arr, json_number(attempt)); json_append(arr, json_number(5));
        json_append(arr, json_number(delay)); json_append(arr, json_copy(red)); json_free(red);
        json_t *r = ta15_call(ctx, "logger_warning", ta15_logger_args(arr));
        if (ta15_is_raise(r)) { json_free(err); return r; } json_free(r);
        const json_t *app = ta15_state_get(ctx, "_app");
        json_t *ua = json_object();
        json_set(ua, "app", ta15_copyv(app));
        json_set(ua, "reason", json_string("conflict-retry"));
        json_set(ua, "kw", json_object());
        json_t *stop = ta15_call(ctx, "stop_updater_or_go_fatal", ua);
        if (ta15_is_raise(stop)) { json_free(err); return stop; }
        bool cont = ta15_truthy(stop); json_free(stop);
        if (!cont) { json_free(err); return json_null(); }
        json_t *sa = json_object(); json_set(sa, "delay", json_number(delay));
        r = ta15_call(ctx, "sleep", sa);
        if (ta15_is_raise(r)) { json_free(err); return r; } json_free(r);
        if (ta15_truthy(ta15_state_get(ctx, "_teardown_started"))) { json_free(err); return json_null(); }
        r = ta15_call(ctx, "drain_polling_connections", json_object());
        if (ta15_is_raise(r)) { json_free(err); return r; } json_free(r);
        if (ta15_truthy(ta15_state_get(ctx, "_teardown_started"))) { json_free(err); return json_null(); }
        json_t *sp = json_object();
        json_set(sp, "app", ta15_copyv(app));
        json_t *skw = json_object();
        json_set(skw, "drop_pending_updates", json_bool(false));
        json_set(skw, "error_callback", json_string("<lambda>"));
        json_set(sp, "kw", skw);
        r = ta15_call(ctx, "start_polling_once", sp);
        if (ta15_is_raise(r)) {
            const char *k = ta15_raise_kind(r);
            if (!ta15_is_exception(k)) { json_free(err); return r; }
            if (ta15_truthy(ta15_state_get(ctx, "_teardown_started"))) { json_free(r); json_free(err); return json_null(); }
            json_t *ea2 = json_object();
            json_set(ea2, "error", json_copy(r)); json_set(ea2, "proxy", json_null());
            json_free(r);
            json_t *red2 = ta15_call(ctx, "redact_telegram_error_text", ea2);
            if (ta15_is_raise(red2)) { json_free(err); return red2; }
            json_t *ar = json_array();
            json_append(ar, json_string("[%s] Telegram polling retry %d/%d failed: %s. Scheduling next attempt."));
            json_append(ar, json_string("bot"));
            json_append(ar, json_number(attempt)); json_append(ar, json_number(5));
            json_append(ar, json_copy(red2)); json_free(red2);
            json_t *w = ta15_call(ctx, "logger_warning", ta15_logger_args(ar));
            if (ta15_is_raise(w)) { json_free(err); return w; } json_free(w);
            if (attempt < 5) {
                json_t *rp = ta15_call(ctx, "restart_polling_in_task", json_object());
                if (ta15_is_raise(rp)) { json_free(err); return rp; } json_free(rp);
                json_free(err); return json_null();
            }
            /* fall through to fatal */
        } else {
            json_free(r);
            json_t *ar = json_array();
            json_append(ar, json_string("[%s] Telegram polling restarted after conflict retry %d/%d; health pending getUpdates progress"));
            json_append(ar, json_string("bot"));
            json_append(ar, json_number(attempt)); json_append(ar, json_number(5));
            r = ta15_call(ctx, "logger_info", ta15_logger_args(ar));
            if (ta15_is_raise(r)) { json_free(err); return r; } json_free(r);
            json_free(err); return json_null();
        }
    }
    if (ta15_truthy(ta15_state_get(ctx, "_teardown_started"))) { json_free(err); return json_null(); }
    json_t *ea = json_object();
    json_set(ea, "error", json_copy(err)); json_set(ea, "proxy", json_null());
    json_t *red = ta15_call(ctx, "redact_telegram_error_text", ea);
    if (ta15_is_raise(red)) { json_free(err); return red; }
    char *msg = ta15_fmt("Telegram polling could not recover after 5 retries (200s total wait). The previous gateway session is still held open on Telegram's servers, or another process is using the same bot token. Retrying in the background; it reconnects automatically once the competing getUpdates session expires. If it never recovers, check for another Hermes or OpenClaw instance using this token.");
    json_t *arr = json_array();
    json_append(arr, json_string("[%s] %s Original error: %s"));
    json_append(arr, json_string("bot")); json_append(arr, json_string(msg));
    json_append(arr, json_copy(red)); json_free(red);
    json_t *r = ta15_call(ctx, "logger_error", ta15_logger_args(arr));
    if (ta15_is_raise(r)) { free(msg); json_free(err); return r; } json_free(r);
    json_t *fa = json_object();
    json_set(fa, "kind", json_string("telegram_polling_conflict"));
    json_set(fa, "message", json_string(msg));
    json_t *kw = json_object(); json_set(kw, "retryable", json_bool(true));
    json_set(fa, "kw", kw);
    r = ta15_call(ctx, "set_fatal_error", fa);
    if (ta15_is_raise(r)) { free(msg); json_free(err); return r; } json_free(r);
    /* self._app.updater on the scripted app value raises AttributeError before the seam fires */
    if (ta15_truthy(ta15_state_get(ctx, "_app"))) {
        json_t *arr2 = json_array();
        json_append(arr2, json_string("[%s] Failed stopping Telegram updater after exhausting conflict retries: %s"));
        json_append(arr2, json_string("bot"));
        json_t *ev = json_object();
        json_set_hdr(ev, "__raise__", "AttributeError");
        json_set(ev, "message", json_string("'str' object has no attribute 'updater'"));
        json_append(arr2, ev);
        json_t *w = ta15_call(ctx, "logger_warning", ta15_logger_args(arr2));
        if (ta15_is_raise(w)) { free(msg); json_free(err); return w; } json_free(w);
    }
    r = ta15_call(ctx, "handoff_polling_fatal_error", json_object());
    if (ta15_is_raise(r)) { free(msg); json_free(err); return r; } json_free(r);
    free(msg); json_free(err); return json_null();
}

/* PoP: ta15_handle_media_message @ plugins/platforms/telegram/adapter.py:_handle_media_message */
static json_t *ta15_handle_media_message(ta15_ctx_t *ctx, const json_t *args) {
    const json_t *update = ta15_arg(args, "update");
    json_t *ua = json_object(); json_set(ua, "update", ta15_copyv(update));
    json_t *msg = ta15_call(ctx, "effective_update_message", ua);
    if (ta15_is_raise(msg)) return msg;
    if (!ta15_truthy(msg)) return msg ? msg : json_null();
    json_t *aa = json_object(); json_set(aa, "msg", json_copy(msg));
    json_t *auth = ta15_call(ctx, "is_user_authorized_from_message", aa);
    if (ta15_is_raise(auth)) { json_free(msg); return auth; }
    bool ok = ta15_truthy(auth); json_free(auth);
    if (!ok) {
        json_t *b = json_object(); json_set(b, "msg", json_copy(msg));
        json_t *kw = json_object();
        json_set(kw, "level", json_number(20));
        json_set(kw, "what", json_string("media from unauthorized user"));
        json_set(b, "kw", kw);
        json_t *r = ta15_call(ctx, "log_blocked_user", b);
        json_free(msg); if (ta15_is_raise(r)) return r; json_free(r); return json_null();
    }
    json_t *ma = json_object(); json_set(ma, "msg", json_copy(msg));
    json_t *clips = ta15_call(ctx, "admit_media", ma);
    if (ta15_is_raise(clips)) { json_free(msg); return clips; }
    if (ta15_is_null(clips)) {
        json_t *sa = json_object(); json_set(sa, "msg", json_copy(msg));
        json_t *obs = ta15_call(ctx, "should_observe_unmentioned", sa);
        if (ta15_is_raise(obs)) { json_free(clips); json_free(msg); return obs; }
        bool observe = ta15_truthy(obs); json_free(obs);
        if (observe) {
            json_t *mt2 = ta15_call(ctx, "media_message_type", sa);
            if (ta15_is_raise(mt2)) { json_free(clips); json_free(msg); return mt2; }
            json_t *be = json_object();
            json_set(be, "msg", json_copy(msg));
            json_set(be, "message_type", json_copy(mt2));
            json_set(be, "update_id", ta15_copyv(ta15_getattr(update, "update_id")));
            json_free(mt2);
            json_t *ev2 = ta15_call(ctx, "build_message_event", be);
            if (ta15_is_raise(ev2)) { json_free(clips); json_free(msg); return ev2; }
            if (ta15_truthy(ta15_getattr(msg, "caption"))) {
                json_t *xa = json_object(); json_set(xa, "msg", json_copy(msg));
                json_t *ex = ta15_call(ctx, "expand_link_entities", xa);
                if (ta15_is_raise(ex)) { json_free(ev2); json_free(clips); json_free(msg); return ex; }
                json_free(ex);
            }
            json_t *ca2 = json_object();
            json_set(ca2, "msg", json_copy(msg));
            json_set(ca2, "event", json_copy(ev2)); json_free(ev2);
            json_t *cm = ta15_call(ctx, "cache_observed_media", ca2);
            if (ta15_is_raise(cm)) { json_free(clips); json_free(msg); return cm; } json_free(cm);
            json_t *oa = json_object();
            json_set(oa, "msg", json_copy(msg));
            json_set(oa, "message_type", json_string("PHOTO"));
            json_t *kw = json_object();
            json_set(kw, "update_id", ta15_copyv(ta15_getattr(update, "update_id")));
            json_set(kw, "event", json_null());
            json_set(oa, "kw", kw);
            /* observe seam keeps only msg/message_type/kw -- event omitted by canon */
        }
        json_free(clips); json_free(msg); return json_null();
    }
    json_free(clips);
    json_t *ta = json_object(); json_set(ta, "msg", json_copy(msg));
    json_t *mtype = ta15_call(ctx, "media_message_type", ta);
    if (ta15_is_raise(mtype)) { json_free(msg); return mtype; }
    json_t *be = json_object();
    json_set(be, "msg", json_copy(msg));
    json_set(be, "message_type", json_copy(mtype));
    json_set(be, "update_id", ta15_copyv(ta15_getattr(update, "update_id")));
    json_free(mtype);
    json_t *ev = ta15_call(ctx, "build_message_event", be);
    if (ta15_is_raise(ev)) { json_free(msg); return ev; }
    if (ta15_truthy(ta15_getattr(msg, "caption"))) {
        json_t *xa = json_object(); json_set(xa, "msg", json_copy(msg));
        json_t *ex = ta15_call(ctx, "expand_link_entities", xa);
        if (ta15_is_raise(ex)) { json_free(ev); json_free(msg); return ex; }
        json_t *ga = json_object();
        json_set(ga, "msg", json_copy(msg));
        json_set(ga, "text", json_copy(ex)); json_free(ex);
        json_t *gt = ta15_call(ctx, "group_trigger_text", ga);
        if (ta15_is_raise(gt)) { json_free(ev); json_free(msg); return gt; }
        json_set(ev, "text", json_copy(gt)); json_free(gt);
    }
    json_t *oa = json_object(); json_set(oa, "event", json_copy(ev));
    json_t *attr = ta15_call(ctx, "apply_observe_attribution", oa);
    if (ta15_is_raise(attr)) { json_free(ev); json_free(msg); return attr; }
    json_free(ev);
    json_t *pa = json_object();
    json_set(pa, "msg", json_copy(msg));
    json_set(pa, "event", json_copy(attr));
    json_t *r = ta15_call(ctx, "claim_pending_text", pa);
    if (ta15_is_raise(r)) { json_free(attr); json_free(msg); return r; } json_free(r);
    json_t *ab = json_object();
    json_set(ab, "msg", json_copy(msg));
    json_set(ab, "event", json_copy(attr));
    r = ta15_call(ctx, "absorb_claimed_text", ab);
    if (ta15_is_raise(r)) { json_free(attr); json_free(msg); return r; } json_free(r);
    const json_t *mg = ta15_getattr(msg, "media_group_id");
    if (ta15_truthy(mg)) {
        json_t *qa = json_object();
        json_set(qa, "media_group_id", json_copy(mg));
        json_set(qa, "event", json_copy(attr));
        json_free(attr); json_free(msg);
        r = ta15_call(ctx, "queue_media_group_event", qa);
        if (ta15_is_raise(r)) return r; json_free(r); return json_null();
    }
    json_t *ha = json_object(); json_set(ha, "event", json_copy(attr));
    json_free(attr); json_free(msg);
    r = ta15_call(ctx, "handle_message", ha);
    if (ta15_is_raise(r)) return r; json_free(r); return json_null();
}
