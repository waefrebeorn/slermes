/*
 * telegram_adapter_flow16.c - Telegram adapter FLOW part 16 of
 * plugins/platforms/telegram/adapter.py (hermes-agent): 16 remaining UNCLAIMED
 * defs. No flow 1-15 target is re-ported. Every target below was confirmed
 * UNCLAIMED in the dotted-name aware live parity scan.
 *
 * Themes: intake auth, callback auth resolution, blocked-user logging, command
 * menu hashing, batching keys, mention detection, stale-prompt notice, group
 * observation, platform-update fanout, polling-client recovery, paged keyboard,
 * config persistence, photo batching, picker edit, rich-draft gate, task
 * restart. All live I/O and injected callables enter through ONE universal
 * seam: ta16_call(ctx, name, args).
 *
 * Self-contained: harness #includes this .c directly; libjson only.
 * Every symbol is static and ta16_-prefixed.
 *
 * Representation contract (shared with tests/oracle_telegram_adapter_flow16.py):
 * - NULL json_t* == Python None / absent; JSON_NULL is a real null value.
 * - Seam scripts: a plain value repeats; a LIST is consumed per call (last
 *   repeats); {"__script__":true,"ret":v,"raise":{type,message}} forces a
 *   result or raise. Missing/exhausted script => fail-closed RuntimeError
 *   recorded in the trace before propagation (BaseException passthrough).
 * - Except sites swallow ONLY Exception-family kinds (TA16_EXC_HIER);
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

typedef struct ta16_ctx {
    json_t *state;        /* owned; carries the attribute-double fields */
    const json_t *io;     /* borrowed scripted seam results */
    json_t *cursors;      /* owned; per-name array cursors */
    json_t *trace;        /* owned; ordered seam events */
    const json_t *errors; /* borrowed placeholder */
} ta16_ctx_t;

/* ── basic helpers ── */
static bool ta16_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *ta16_raise_kind(const json_t *v) {
    return ta16_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static const char *ta16_raise_msg(const json_t *v) {
    return ta16_is_raise(v) ? json_get_str(v, "message", "") : "";
}
static bool ta16_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool ta16_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *ta16_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static double ta16_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static json_t *ta16_copyv(const json_t *v) { return v ? json_copy((json_t *)v) : json_null(); }
static bool ta16_json_eq(const json_t *a, const json_t *b) {
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
                if (!ta16_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                if (!a->c.keys[i] || !b->c.keys[i] || strcmp(a->c.keys[i], b->c.keys[i]) != 0)
                    return false;
                if (!ta16_json_eq(a->c.items[i], b->c.items[i])) return false;
            }
            return true;
        default: return false;
    }
}
static json_t *ta16_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static const json_t *ta16_arg(const json_t *args, const char *key) {
    return args ? json_obj_get(args, key) : NULL;
}
static const json_t *ta16_state_get(const ta16_ctx_t *ctx, const char *key) {
    return (ctx && ctx->state && ctx->state->type == JSON_OBJECT)
        ? json_obj_get(ctx->state, key) : NULL;
}
static bool ta16_state_truthy(const ta16_ctx_t *ctx, const char *key) {
    return ta16_truthy(ta16_state_get(ctx, key));
}
static void ta16_state_set(ta16_ctx_t *ctx, const char *key, const json_t *v) {
    json_set(ctx->state, key, v ? json_copy((json_t *)v) : json_null());
}

/* ── string helpers ── */
static char *ta16_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static char *ta16_lower(const char *s) {
    char *o = ta16_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static char *ta16_fmt(const char *fmt, ...) {
    char tmp[2400]; va_list ap;
    va_start(ap, fmt); int n = vsnprintf(tmp, sizeof tmp, fmt, ap); va_end(ap);
    if (n < 0) return ta16_strdup("");
    char *o = malloc((size_t)n + 1);
    if (!o) return ta16_strdup("");
    va_start(ap, fmt); vsnprintf(o, (size_t)n + 1, fmt, ap); va_end(ap);
    return o;
}
static char *ta16_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return ta16_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return ta16_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e16) return ta16_fmt("%.0f", d);
            return ta16_fmt("%.17g", d);
        }
        case JSON_STRING: return ta16_strdup(v->str_val ? v->str_val : "");
        default: { char *s = json_serialize((json_t *)v); return s ? s : ta16_strdup(""); }
    }
}
static char *ta16_trim(const char *s) {
    if (!s) return ta16_strdup("");
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) n--;
    char *o = malloc(n + 1);
    if (o) { memcpy(o, s, n); o[n] = '\0'; }
    return o;
}
static bool ta16_lower_eq_trim(const char *s, const char *want_lower) {
    if (!s) return false;
    char *t = ta16_trim(s);
    char *l = ta16_lower(t ? t : "");
    bool eq = l && strcmp(l, want_lower) == 0;
    free(t); free(l);
    return eq;
}

/* Python repr of a string (single-quoted); fixture domain is plain ascii. */
static char *ta16_repr_str(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 3);
    if (!o) return ta16_strdup("''");
    size_t k = 0;
    o[k++] = '\'';
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (c == '\'' || c == '\\') o[k++] = '\\';
        o[k++] = c;
    }
    o[k++] = '\''; o[k] = '\0';
    return o;
}
/* Python repr of a JSON value, restricted to the menu-hash fixture domain
 * (strings, numbers, bool, null, arrays). Tuples are represented by arrays. */
static char *ta16_repr(const json_t *v, bool tuple) {
    if (!v || v->type == JSON_NULL) return ta16_strdup("None");
    switch (v->type) {
        case JSON_STRING: return ta16_repr_str(v->str_val);
        case JSON_BOOL: return ta16_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e16) return ta16_fmt("%.0f", d);
            return ta16_fmt("%.17g", d);
        }
        case JSON_ARRAY: {
            size_t cap = 64, len = 0;
            char *o = malloc(cap);
            if (!o) return ta16_strdup("[]");
            o[0] = '\0';
            char open = tuple ? '(' : '[', close = tuple ? ')' : ']';
            size_t k = 0;
            o[k++] = open;
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) { o[k++] = ','; o[k++] = ' '; }
                char *e = ta16_repr(v->c.items[i], false);
                size_t el = strlen(e ? e : "");
                while (k + el + 4 > cap) { cap *= 2; o = realloc(o, cap); }
                memcpy(o + k, e ? e : "", el); k += el; free(e);
            }
            if (tuple && v->c.count == 1) o[k++] = ',';
            o[k++] = close; o[k] = '\0';
            return o;
        }
        default: return ta16_strdup("");
    }
}

/* ── FULL builtin exception hierarchy (CPython 3.14) ── */
typedef struct { const char *name; const char *parent; } ta16_exc_row_t;
static const ta16_exc_row_t TA16_EXC_HIER[] = {
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
static const char *ta16_builtin_parent(const char *name) {
    for (size_t i = 0; i < sizeof(TA16_EXC_HIER) / sizeof(TA16_EXC_HIER[0]); i++)
        if (strcmp(name, TA16_EXC_HIER[i].name) == 0) return TA16_EXC_HIER[i].parent;
    return NULL;
}
static bool ta16_chain_has(const char *start, const char *target) {
    const char *cur = start;
    for (int depth = 0; cur && depth < 24; depth++) {
        if (strcmp(cur, target) == 0) return true;
        cur = ta16_builtin_parent(cur);
    }
    return false;
}
static bool ta16_is_exception(const char *kind) {
    if (!kind) return false;
    if (ta16_builtin_parent(kind) == NULL) return true; /* custom class -> Exception */
    return ta16_chain_has(kind, "Exception");
}
/* TelegramError is an oracle-side synthetic Exception subclass. */
static bool ta16_is_exception_kind(const json_t *ret) {
    return ta16_is_exception(ta16_raise_kind(ret));
}

/* ── the one universal seam ── */
static json_t *ta16_call(ta16_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256]; snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ta16_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256]; snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ta16_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT)
                ret = ta16_err(json_get_str(raise, "type", "RuntimeError"),
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
/* convenience: single-key args */
static json_t *ta16_args1(const char *k, const json_t *v) {
    json_t *a = json_object();
    json_set(a, k, v ? json_copy((json_t *)v) : json_null());
    return a;
}
/* convenience: string-key args */
static json_t *ta16_args_s(const char *k, const char *v) {
    json_t *a = json_object();
    json_set_hdr(a, k, v ? v : "");
    return a;
}
/* convenience: logger args {"args":[...],"kwargs":{...}} */
static json_t *ta16_logger_args(json_t *arr, bool exc_info) {
    json_t *a = json_object();
    json_set(a, "args", arr ? arr : json_array());
    json_t *kw = json_object();
    if (exc_info) json_set(kw, "exc_info", json_bool(true));
    json_set(a, "kwargs", kw);
    return a;
}

/* ═══════════════════════ ported flows ═══════════════════════ */

/* PoP: ta16_is_user_authorized_from_message @ plugins/platforms/telegram/adapter.py:_is_user_authorized_from_message */
static json_t *ta16_is_user_authorized_from_message(ta16_ctx_t *ctx, const json_t *args) {
    json_t *r = ta16_call(ctx, "refresh_gates", json_object());
    if (ta16_is_raise(r)) return r;
    json_t *a = json_object();
    json_set(a, "message", ta16_copyv(ta16_arg(args, "message")));
    r = ta16_call(ctx, "source_from_message", a);
    if (ta16_is_raise(r)) return r;
    const char *uid = ta16_str(json_obj_get(r, "user_id"));
    if (!uid || !uid[0]) { json_free(r); return json_bool(true); }
    const char *ctype = ta16_str(json_obj_get(r, "chat_type"));
    bool is_group = ctype && (strcmp(ctype, "group") == 0 || strcmp(ctype, "forum") == 0 ||
                              strcmp(ctype, "channel") == 0);
    const char *keys[2]; int nkeys = 1;
    keys[0] = "allow_from";
    if (is_group) { keys[1] = "group_allow_from"; nkeys = 2; }
    const json_t *extra = ta16_state_get(ctx, "config_extra");
    bool any_non_null = false;
    for (int i = 0; i < nkeys; i++) {
        const json_t *v = (extra && extra->type == JSON_OBJECT) ? json_obj_get(extra, keys[i]) : NULL;
        if (!ta16_is_null(v)) any_non_null = true;
    }
    json_t *allow = json_array();
    /* Python: None if all(v is None ...) else set().union(map(...)) -- the
     * map is never consumed when all values are None, so no coerce seam. */
    if (any_non_null) for (int i = 0; i < nkeys; i++) {
        const json_t *v = (extra && extra->type == JSON_OBJECT) ? json_obj_get(extra, keys[i]) : NULL;
        json_t *ca = json_object();
        json_set(ca, "v", ta16_copyv(v));
        json_t *set = ta16_call(ctx, "coerce_allow_set", ca);
        if (ta16_is_raise(set)) { json_free(r); json_free(allow); return set; }
        if (set && set->type == JSON_ARRAY)
            for (size_t j = 0; j < set->c.count; j++)
                json_append(allow, json_copy(set->c.items[j]));
        json_free(set);
    }
    bool grant = false;
    if (any_non_null) {
        for (size_t j = 0; j < allow->c.count; j++) {
            const char *e = ta16_str(allow->c.items[j]);
            if (e && (strcmp(e, uid) == 0 || strcmp(e, "*") == 0)) { grant = true; break; }
        }
    }
    if (grant) { json_free(r); json_free(allow); return json_bool(true); }
    /* instance-level callable override (suppressed Exception only) */
    int authorized = -1; /* -1 unset, 1 true, 0 false */
    if (ta16_state_truthy(ctx, "_is_callback_user_authorized")) {
        json_t *a2 = json_object();
        json_set(a2, "user_id", json_string(uid));
        json_set(a2, "chat_id", ta16_copyv(json_obj_get(r, "chat_id")));
        json_set(a2, "chat_type", ta16_copyv(json_obj_get(r, "chat_type")));
        json_set(a2, "thread_id", ta16_copyv(json_obj_get(r, "thread_id")));
        json_set(a2, "user_name", ta16_copyv(json_obj_get(r, "user_name")));
        json_t *cb = ta16_call(ctx, "callback_auth_call", a2);
        if (ta16_is_raise(cb)) {
            if (!ta16_is_exception_kind(cb)) { json_free(r); json_free(allow); return cb; }
            /* suppressed */
        } else authorized = ta16_truthy(cb) ? 1 : 0;
        json_free(cb);
    }
    if (authorized == -1) {
        json_t *af = ta16_call(ctx, "legacy_runner_auth_fn", json_object());
        if (ta16_is_raise(af)) { json_free(r); json_free(allow); return af; }
        bool has_callback = !ta16_is_null(ta16_state_get(ctx, "_authorization_check"));
        bool has_auth_fn = !ta16_is_null(af);
        if (has_callback || has_auth_fn) {
            if (!any_non_null) {
                json_t *ec = ta16_call(ctx, "auth_env_configured", json_object());
                if (ta16_is_raise(ec)) { json_free(af); json_free(r); json_free(allow); return ec; }
                if (!ta16_truthy(ec)) { json_free(af); json_free(r); json_free(allow); return json_bool(true); }
                json_free(ec);
            }
            if (has_callback) {
                json_t *a3 = json_object();
                json_set(a3, "user_id", json_string(uid));
                json_set(a3, "chat_type", ta16_copyv(json_obj_get(r, "chat_type")));
                json_set(a3, "chat_id", ta16_copyv(json_obj_get(r, "chat_id")));
                json_set(a3, "is_bot", ta16_copyv(json_obj_get(r, "is_bot")));
                json_set(a3, "thread_id", ta16_copyv(json_obj_get(r, "thread_id")));
                json_t *d = ta16_call(ctx, "is_sender_authorized", a3);
                if (ta16_is_raise(d)) { json_free(af); json_free(r); json_free(allow); return d; }
                if (!ta16_is_null(d)) authorized = ta16_truthy(d) ? 1 : 0;
                json_free(d);
            }
            if (authorized == -1 && has_auth_fn) {
                json_t *a4 = json_object();
                json_set(a4, "source", ta16_copyv(r));
                json_t *d = ta16_call(ctx, "auth_fn_call", a4);
                if (ta16_is_raise(d)) {
                    if (!ta16_is_exception_kind(d)) { json_free(af); json_free(r); json_free(allow); return d; }
                    json_t *la = json_array();
                    json_append(la, json_string("[Telegram] Falling back to env-only auth for user %s"));
                    json_append(la, json_string(uid));
                    json_t *lg = ta16_logger_args(la, true);
                    json_t *lr = ta16_call(ctx, "logger_debug", lg);
                    json_free(lr);
                } else authorized = ta16_truthy(d) ? 1 : 0;
                json_free(d);
            }
        }
        json_free(af);
    }
    if (authorized == -1) {
        json_t *ea = json_object();
        json_set(ea, "user_id", json_string(uid));
        json_t *e = ta16_call(ctx, "env_allowlist_decision", ea);
        if (ta16_is_raise(e)) { json_free(r); json_free(allow); return e; }
        if (!ta16_is_null(e)) authorized = ta16_truthy(e) ? 1 : 0;
        json_free(e);
        if (authorized == -1 && !any_non_null) { json_free(r); json_free(allow); return json_bool(true); }
    }
    if (authorized == 1) { json_free(r); json_free(allow); return json_bool(true); }
    json_t *a5 = json_object();
    json_set(a5, "source", ta16_copyv(r));
    json_free(r); json_free(allow);
    return ta16_call(ctx, "pass_unauthorized_dm", a5);
}

/* PoP: ta16_legacy_runner_auth_fn @ plugins/platforms/telegram/adapter.py:_legacy_runner_auth_fn */
static json_t *ta16_legacy_runner_auth_fn(ta16_ctx_t *ctx, const json_t *args) {
    (void)ctx; (void)args;
    const json_t *v = NULL;
    /* read the state key set by the harness: _runner_auth */
    if (ctx->state && ctx->state->type == JSON_OBJECT)
        v = json_obj_get((json_t *)ctx->state, "_runner_auth");
    if (ta16_truthy(v)) return json_string("auth_fn");
    return json_null();
}

/* PoP: ta16_log_blocked_user @ plugins/platforms/telegram/adapter.py:_log_blocked_user */
static json_t *ta16_log_blocked_user(ta16_ctx_t *ctx, const json_t *args) {
    double level = json_get_num(args, "level", 30.0);
    const char *what = json_get_str(args, "what", "unauthorized user");
    json_t *arr = json_array();
    json_append(arr, json_string(what));
    json_t *fu = ta16_arg(args, "from_user");
    json_t *ch = ta16_arg(args, "chat");
    json_append(arr, (fu && fu->type == JSON_OBJECT) ? ta16_copyv(json_obj_get(fu, "id")) : json_null());
    json_append(arr, (ch && ch->type == JSON_OBJECT) ? ta16_copyv(json_obj_get(ch, "id")) : json_null());
    json_t *a = json_object();
    json_set(a, "level", json_number(level));
    json_set(a, "args", arr);
    json_t *r = ta16_call(ctx, "logger_log", a);
    json_free(r);
    return json_null();
}

/* PoP: ta16_menu_fingerprint @ plugins/platforms/telegram/adapter.py:_menu_fingerprint */
static json_t *ta16_menu_fingerprint(ta16_ctx_t *ctx, const json_t *args) {
    json_t *lr = ta16_call(ctx, "get_language", json_object());
    if (ta16_is_raise(lr)) return lr;
    const char *lang = ta16_str(lr) ? ta16_str(lr) : "";
    size_t cap = 128, len = 0;
    char *payload = malloc(cap);
    if (!payload) { json_free(lr); return json_null(); }
    char *lrep = ta16_repr_str(lang);
    snprintf(payload, cap, "[%s, [", lrep ? lrep : "''");
    len = strlen(payload);
    free(lrep);
    const json_t *menu = ta16_arg(args, "menu_commands");
    if (menu && menu->type == JSON_ARRAY) {
        for (size_t i = 0; i < menu->c.count; i++) {
            if (i) { if (len + 2 >= cap) { cap *= 2; payload = realloc(payload, cap); } strcat(payload, ", "); len += 2; }
            char *row = ta16_repr(menu->c.items[i], true);
            size_t rl = strlen(row ? row : "");
            while (len + rl + 4 > cap) { cap *= 2; payload = realloc(payload, cap); }
            memcpy(payload + len, row ? row : "", rl); len += rl; payload[len] = '\0';
            free(row);
        }
    }
    if (len + 2 >= cap) { cap *= 2; payload = realloc(payload, cap); }
    strcat(payload, "]]");
    json_t *a = json_object();
    json_set_hdr(a, "payload", payload);
    free(payload);
    json_free(lr);
    json_t *r = ta16_call(ctx, "sha256_hex", a);
    return r;
}

/* PoP: ta16_merge_into_pending @ plugins/platforms/telegram/adapter.py:_merge_into_pending */
static json_t *ta16_merge_into_pending(ta16_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "event", ta16_copyv(ta16_arg(args, "event")));
    json_t *r = ta16_call(ctx, "resolve_routing", a);
    if (ta16_is_raise(r)) return r;
    json_free(r);
    json_t *b = json_object();
    json_set(b, "pending", ta16_copyv(ta16_arg(args, "pending")));
    json_set(b, "key", ta16_copyv(ta16_arg(args, "key")));
    json_set(b, "event", ta16_copyv(ta16_arg(args, "event")));
    json_set(b, "merge_caption", json_bool(ta16_state_truthy(ctx, "_merge_caption")));
    json_set_hdr(b, "route_key", "<lambda>");
    json_set_hdr(b, "reroute", "<lambda>");
    return ta16_call(ctx, "merge_batched_event", b);
}

/* PoP: ta16_message_mentions_bot @ plugins/platforms/telegram/adapter.py:_message_mentions_bot */
static json_t *ta16_message_mentions_bot(ta16_ctx_t *ctx, const json_t *args) {
    const json_t *bot = ta16_state_get(ctx, "_bot");
    if (ta16_is_null(bot)) return json_bool(false);
    json_t *ur = ta16_call(ctx, "current_bot_username", json_object());
    if (ta16_is_raise(ur)) return ur;
    const char *uname = ta16_str(ur);
    char *expected = NULL;
    if (uname && uname[0]) expected = ta16_fmt("@%s", uname);
    char *expected_lower = expected ? ta16_lower(expected) : NULL;
    const json_t *bot_id = (bot && bot->type == JSON_OBJECT) ? json_obj_get((json_t *)bot, "id") : NULL;
    json_t *a = json_object();
    json_set(a, "message", ta16_copyv(ta16_arg(args, "message")));
    json_t *sources = ta16_call(ctx, "entity_sources", a);
    if (ta16_is_raise(sources)) { free(expected); free(expected_lower); return sources; }
    bool found = false;
    if (sources && sources->type == JSON_ARRAY) {
        for (size_t i = 0; i < sources->c.count && !found; i++) {
            json_t *pair = sources->c.items[i];
            if (!pair || pair->type != JSON_ARRAY || pair->c.count < 2) continue;
            const char *text = ta16_str(pair->c.items[0]);
            json_t *ents = pair->c.items[1];
            if (!text || !ents || ents->type != JSON_ARRAY) continue;
            for (size_t j = 0; j < ents->c.count && !found; j++) {
                json_t *e = ents->c.items[j];
                json_t *ea = json_object();
                json_set(ea, "entity", ta16_copyv(e));
                json_t *tr = ta16_call(ctx, "entity_type", ea);
                if (ta16_is_raise(tr)) {
                    json_free(sources); free(expected); free(expected_lower);
                    return tr;
                }
                const char *et = ta16_str(tr);
                if (et && strcmp(et, "mention") == 0 && expected) {
                    json_t *sa = json_object();
                    json_set(sa, "text", json_string(text));
                    json_set(sa, "entity", ta16_copyv(e));
                    json_t *sp = ta16_call(ctx, "entity_span", sa);
                    if (ta16_is_raise(sp)) { json_free(tr); json_free(sources); free(expected); free(expected_lower); return sp; }
                    if (!ta16_is_null(sp)) {
                        const char *sv = ta16_str(sp);
                        if (sv && ta16_lower_eq_trim(sv, expected_lower)) found = true;
                    }
                    json_free(sp);
                } else if (et && strcmp(et, "text_mention") == 0) {
                    json_t *u = (e && e->type == JSON_OBJECT) ? json_obj_get(e, "user") : NULL;
                    if (u && u->type == JSON_OBJECT) {
                        const json_t *uid = json_obj_get(u, "id");
                        if (uid && bot_id && ta16_json_eq(uid, bot_id)) found = true;
                    }
                } else if (et && strcmp(et, "bot_command") == 0 && expected) {
                    json_t *sa = json_object();
                    json_set(sa, "text", json_string(text));
                    json_set(sa, "entity", ta16_copyv(e));
                    json_t *sp = ta16_call(ctx, "entity_span", sa);
                    if (ta16_is_raise(sp)) { json_free(tr); json_free(sources); free(expected); free(expected_lower); return sp; }
                    if (!ta16_is_null(sp)) {
                        const char *sv = ta16_str(sp);
                        const char *at = sv ? strchr(sv, '@') : NULL;
                        if (at) {
                            char *tail = ta16_lower(at);
                            /* strip whitespace from the tail */
                            char *tt = ta16_trim(tail);
                            if (tt && strcmp(tt, expected_lower) == 0) found = true;
                            free(tt); free(tail);
                        }
                    }
                    json_free(sp);
                }
                json_free(tr);
            }
        }
    }
    json_free(sources);
    if (!found && uname && uname[0]) {
        json_t *ma = json_object();
        json_set(ma, "message", ta16_copyv(ta16_arg(args, "message")));
        json_set_hdr(ma, "bot_username", uname);
        json_t *mr = ta16_call(ctx, "extract_mentions", ma);
        if (ta16_is_raise(mr)) { free(expected); free(expected_lower); return mr; }
        if (mr && (mr->type == JSON_ARRAY || mr->type == JSON_OBJECT))
            for (size_t k = 0; k < mr->c.count; k++) {
                const char *nm = ta16_str(mr->c.items[k]);
                if (nm && strcmp(nm, uname) == 0) found = true;
            }
        json_free(mr);
    }
    free(expected); free(expected_lower);
    json_free(ur);
    return json_bool(found);
}

/* PoP: ta16_notify_clarify_expired @ plugins/platforms/telegram/adapter.py:_notify_clarify_expired */
static json_t *ta16_notify_clarify_expired(ta16_ctx_t *ctx, const json_t *args) {
    json_t *ta = json_object();
    json_t *ta_args = json_array();
    json_append(ta_args, json_string("platform.telegram.prompt.expired_toast"));
    json_set(ta, "args", ta_args);
    json_set(ta, "kwargs", json_object());
    json_t *tr = ta16_call(ctx, "toast", ta);
    const char *toast = "";
    bool toasted = false;
    if (ta16_is_raise(tr)) {
        if (!ta16_is_exception_kind(tr)) { return tr; }  /* BaseException passes */
        /* contextlib.suppress(Exception) swallows it; answer_callback never runs */
        json_free(tr);
    } else {
        toast = ta16_str(tr) ? ta16_str(tr) : "";
        toasted = true;
    }
    if (toasted) {
        json_t *aa = json_object();
        json_set_hdr(aa, "query", "_Query");
        json_set(aa, "text", json_string(toast));
        json_t *ar = ta16_call(ctx, "answer_callback", aa);
        json_free(tr);
        if (ta16_is_raise(ar) && !ta16_is_exception_kind(ar)) return ar;
        json_free(ar);
    }
    /* query.message.text or '' */
    const json_t *q = ta16_arg(args, "query");
    const json_t *msg = (q && q->type == JSON_OBJECT) ? json_obj_get((json_t *)q, "message") : NULL;
    const json_t *txt = (msg && msg->type == JSON_OBJECT) ? json_obj_get((json_t *)msg, "text") : NULL;
    const char *text = (txt && txt->type == JSON_STRING && txt->str_val && txt->str_val[0]) ? txt->str_val : "";
    json_t *ha = json_object(); json_set_hdr(ha, "text", text);
    json_t *e1r = ta16_call(ctx, "html_escape", ha);
    if (ta16_is_raise(e1r)) { return e1r; }
    const char *e1 = ta16_str(e1r) ? ta16_str(e1r) : "";
    json_t *trarg = json_object();
    json_t *trarr = json_array(); json_append(trarr, json_string("platform.telegram.prompt.expired_body"));
    json_set(trarg, "args", trarr); json_set(trarg, "kwargs", json_object());
    json_t *br = ta16_call(ctx, "translate", trarg);
    if (ta16_is_raise(br)) { json_free(e1r); return br; }
    const char *body = ta16_str(br) ? ta16_str(br) : "";
    json_t *hb = json_object(); json_set_hdr(hb, "text", body);
    json_t *e2r = ta16_call(ctx, "html_escape", hb);
    if (ta16_is_raise(e2r)) { json_free(e1r); json_free(br); return e2r; }
    const char *e2 = ta16_str(e2r) ? ta16_str(e2r) : "";
    char *final = ta16_fmt("❓ %s\n\n<i>%s</i>", e1, e2);
    json_t *ea = json_object();
    json_set_hdr(ea, "query", "_Query");
    json_set_hdr(ea, "text", final);
    free(final);
    json_t *er = ta16_call(ctx, "edit_html_quiet", ea);
    if (ta16_is_raise(er)) { json_free(e2r); json_free(br); json_free(e1r); return er; }
    json_free(er); json_free(e2r); json_free(br); json_free(e1r);
    return json_null();
}

/* PoP: ta16_observe_unmentioned_group_message @ plugins/platforms/telegram/adapter.py:_observe_unmentioned_group_message */
static json_t *ta16_observe_unmentioned_group_message(ta16_ctx_t *ctx, const json_t *args) {
    if (!ta16_state_truthy(ctx, "_session_store")) return json_null();
    const char *aname = ta16_str(ta16_state_get(ctx, "name"));
    if (!aname) aname = "telegram";
    /* try */
    json_t *ev = NULL;
    const json_t *given = ta16_arg(args, "event");
    if (given && !ta16_is_null(given)) ev = json_copy((json_t *)given);
    else {
        json_t *a = json_object();
        json_set(a, "message", ta16_copyv(ta16_arg(args, "message")));
        json_set(a, "message_type", ta16_copyv(ta16_arg(args, "msg_type")));
        json_set(a, "update_id", ta16_copyv(ta16_arg(args, "update_id")));
        json_t *r = ta16_call(ctx, "build_message_event", a);
        if (ta16_is_raise(r)) { /* handled by except path below */ }
        else ev = r;
        if (!ev || (ev->type != JSON_OBJECT)) {
            if (ev) json_free(ev);
            json_t *la = json_array();
            json_append(la, json_string("[%s] Failed to observe Telegram group message: %s"));
            json_append(la, json_string(aname));
            json_append(la, ta16_err("AttributeError", "'NoneType' object has no attribute 'source'"));
            json_t *lg = ta16_logger_args(la, false);
            json_t *fp = ta16_call(ctx, "fail_update_preparation", json_object()); json_free(fp);
            json_t *lr = ta16_call(ctx, "logger_warning", lg); json_free(lr);
            return json_null();
        }
    }
    const json_t *source = json_obj_get(ev, "source");
    json_t *sa = json_object(); json_set(sa, "source", ta16_copyv(source));
    json_t *sr = ta16_call(ctx, "observe_shared_source", sa);
    if (ta16_is_raise(sr)) { json_free(ev); goto obs_except_sr; }
    json_t *ga = json_object(); json_set(ga, "source", ta16_copyv(sr));
    json_t *se = ta16_call(ctx, "get_or_create_session", ga);
    if (ta16_is_raise(se)) { json_free(sr); json_free(ev);
        json_t *la = json_array();
        json_append(la, json_string("[%s] Failed to observe Telegram group message: %s"));
        json_append(la, json_string(aname)); json_append(la, json_copy(se));
        json_t *lg = ta16_logger_args(la, false);
        json_t *fp = ta16_call(ctx, "fail_update_preparation", json_object()); json_free(fp);
        json_t *lr = ta16_call(ctx, "logger_warning", lg); json_free(lr);
        json_free(se); return json_null();
    }
    json_free(sr);
    const json_t *sid = (se && se->type == JSON_OBJECT) ? json_obj_get(se, "session_id") : NULL;
    if (!sid) { json_free(se); json_free(ev);
        json_t *la = json_array();
        json_append(la, json_string("[%s] Failed to observe Telegram group message: %s"));
        json_append(la, json_string(aname));
        json_append(la, ta16_err("AttributeError", "'dict' object has no attribute 'session_id'"));
        json_t *lg = ta16_logger_args(la, false);
        json_t *fp = ta16_call(ctx, "fail_update_preparation", json_object()); json_free(fp);
        json_t *lr = ta16_call(ctx, "logger_warning", lg); json_free(lr);
        return json_null();
    }
    const json_t *sid_copy = json_copy((json_t *)sid); json_free(se);
    json_t *oa = json_object(); json_set(oa, "event", ta16_copyv(ev));
    json_t *txt = ta16_call(ctx, "observe_attributed_text", oa);
    if (ta16_is_raise(txt)) { json_free((json_t *)sid_copy); json_free(ev);
        json_t *la = json_array();
        json_append(la, json_string("[%s] Failed to observe Telegram group message: %s"));
        json_append(la, json_string(aname)); json_append(la, json_copy(txt));
        json_t *lg = ta16_logger_args(la, false);
        json_t *fp = ta16_call(ctx, "fail_update_preparation", json_object()); json_free(fp);
        json_t *lr = ta16_call(ctx, "logger_warning", lg); json_free(lr);
        json_free(txt); return json_null();
    }
    json_t *tm = ta16_call(ctx, "time_time", json_object());
    if (ta16_is_raise(tm)) { json_free((json_t *)sid_copy); json_free(ev); json_free(txt);
        json_t *la = json_array();
        json_append(la, json_string("[%s] Failed to observe Telegram group message: %s"));
        json_append(la, json_string(aname)); json_append(la, json_copy(tm));
        json_t *lg = ta16_logger_args(la, false);
        json_t *fp = ta16_call(ctx, "fail_update_preparation", json_object()); json_free(fp);
        json_t *lr = ta16_call(ctx, "logger_warning", lg); json_free(lr);
        json_free(tm); return json_null();
    }
    json_t *entry = json_object();
    json_set_hdr(entry, "role", "user");
    json_set(entry, "content", json_copy(txt));
    json_set(entry, "timestamp", json_copy(tm));
    json_set(entry, "observed", json_bool(true));
    const json_t *mid = json_obj_get(ev, "message_id");
    if (ta16_truthy(mid)) { char *s = ta16_py_str(mid); json_set_hdr(entry, "message_id", s); free(s); }
    json_t *ar = ta16_call(ctx, "accept_update", json_object()); json_free(ar);
    json_t *apa = json_object();
    json_set(apa, "session_id", json_copy((json_t *)sid_copy));
    json_set(apa, "entry", json_copy(entry));
    json_t *apr = ta16_call(ctx, "append_transcript", apa);
    if (ta16_is_raise(apr)) { json_free(apr); }
    json_free(entry); json_free(tm); json_free(txt); json_free((json_t *)sid_copy);
    /* logger.info */
    const json_t *msg = ta16_arg(args, "message");
    const json_t *chat = (msg && msg->type == JSON_OBJECT) ? json_obj_get((json_t *)msg, "chat") : NULL;
    const json_t *cid = (chat && chat->type == JSON_OBJECT) ? json_obj_get((json_t *)chat, "id") : NULL;
    const json_t *uid = (source && source->type == JSON_OBJECT) ? json_obj_get((json_t *)source, "user_id") : NULL;
    json_t *la = json_array();
    json_append(la, json_string("[%s] Telegram group message observed (no bot trigger): chat=%s from=%s"));
    json_append(la, json_string(aname));
    json_append(la, cid ? ta16_copyv(cid) : json_string("unknown"));
    json_append(la, (uid && ta16_truthy(uid)) ? ta16_copyv(uid) : json_string("unknown"));
    json_t *lg = ta16_logger_args(la, false);
    json_t *lr = ta16_call(ctx, "logger_info", lg); json_free(lr);
    json_free(ev);
    return json_null();

obs_except_sr:
    {
        json_t *la = json_array();
        json_append(la, json_string("[%s] Failed to observe Telegram group message: %s"));
        json_append(la, json_string(aname)); json_append(la, json_copy(sr));
        json_t *lg = ta16_logger_args(la, false);
        json_t *fp = ta16_call(ctx, "fail_update_preparation", json_object()); json_free(fp);
        json_t *lr = ta16_call(ctx, "logger_warning", lg); json_free(lr);
        json_free(sr);
        return json_null();
    }
}

/* PoP: ta16_on_platform_update @ plugins/platforms/telegram/adapter.py:_on_platform_update */
static json_t *ta16_on_platform_update(ta16_ctx_t *ctx, const json_t *args) {
    bool have_admission = ta16_state_truthy(ctx, "_update_admission");
    bool count_it = true;
    if (have_admission) {
        json_t *gr = ta16_call(ctx, "admission_get", json_object());
        if (ta16_is_raise(gr)) return gr;
        if (!ta16_is_null(gr)) count_it = false;
        json_free(gr);
    }
    if (count_it) {
        const json_t *cur = ta16_state_get(ctx, "_updates_dispatched_total");
        double v = ta16_is_null(cur) ? 0.0 : ta16_num(cur);
        ta16_state_set(ctx, "_updates_dispatched_total", json_number(v + 1.0));
    }
    if (!ta16_state_truthy(ctx, "_platform_event_handler")) return json_null();
    json_t *hr = ta16_call(ctx, "has_hook", ta16_args_s("name", "gateway_platform_event"));
    if (ta16_is_raise(hr)) return hr;
    if (!ta16_truthy(hr)) { json_free(hr); return json_null(); }
    json_free(hr);
    json_t *na = json_object(); json_set(na, "update", ta16_copyv(ta16_arg(args, "update")));
    json_t *ev = ta16_call(ctx, "normalize_platform_event", na);
    if (ta16_is_raise(ev)) {
        json_t *lr = ta16_call(ctx, "fail_update_preparation", json_object()); json_free(lr);
        json_t *la = json_array();
        json_append(la, json_string("[%s] gateway_platform_event normalize error"));
        json_append(la, json_string(ta16_str(ta16_state_get(ctx, "name")) ? ta16_str(ta16_state_get(ctx, "name")) : "telegram"));
        json_t *lg = ta16_logger_args(la, true);
        json_t *r = ta16_call(ctx, "logger_debug", lg); json_free(r);
        json_free(ev);
        return json_null();
    }
    if (ta16_is_null(ev)) { json_free(ev); return json_null(); }
    json_t *sa = json_object(); json_set(sa, "update", ta16_copyv(ta16_arg(args, "update")));
    json_t *src = ta16_call(ctx, "source_for_platform_event_auth", sa);
    if (ta16_is_raise(src)) { json_free(ev); goto onp_dispatch_except; }
    json_t *ar = ta16_call(ctx, "accept_update", json_object());
    if (ta16_is_raise(ar)) { json_free(src); json_free(ev); goto onp_dispatch_except; }
    json_free(ar);
    json_t *ha = json_object();
    json_set(ha, "event", json_copy(ev)); json_set(ha, "source", json_copy(src));
    json_t *hr2 = ta16_call(ctx, "handler_call", ha);
    if (ta16_is_raise(hr2)) { json_free(hr2); json_free(src); json_free(ev); goto onp_dispatch_except; }
    json_free(hr2); json_free(src); json_free(ev);
    return json_null();

onp_dispatch_except:
    {
        json_t *lr = ta16_call(ctx, "fail_update_preparation", json_object()); json_free(lr);
        json_t *la = json_array();
        json_append(la, json_string("[%s] gateway_platform_event dispatch error"));
        json_append(la, json_string(ta16_str(ta16_state_get(ctx, "name")) ? ta16_str(ta16_state_get(ctx, "name")) : "telegram"));
        json_t *lg = ta16_logger_args(la, true);
        json_t *r = ta16_call(ctx, "logger_debug", lg); json_free(r);
        return json_null();
    }
}

/* PoP: ta16_orphan_and_rebuild_polling_client @ plugins/platforms/telegram/adapter.py:_orphan_and_rebuild_polling_client */
static json_t *ta16_orphan_and_rebuild_polling_client(ta16_ctx_t *ctx, const json_t *args) {
    const json_t *req = ta16_arg(args, "polling_req");
    const json_t *old = (req && req->type == JSON_OBJECT) ? json_obj_get((json_t *)req, "_client") : NULL;
    const json_t *build = (req && req->type == JSON_OBJECT) ? json_obj_get((json_t *)req, "_build_client") : NULL;
    if (ta16_is_null(old) || !ta16_truthy(build)) return json_null();
    const json_t *closed = (old && old->type == JSON_OBJECT) ? json_obj_get((json_t *)old, "is_closed") : NULL;
    bool is_closed = ta16_is_null(closed) ? true : ta16_truthy(closed);
    if (is_closed) return json_null();
    json_t *ba = json_object();
    json_set(ba, "args", json_array()); json_set(ba, "kwargs", json_object());
    json_t *br = ta16_call(ctx, "build_client", ba);
    if (ta16_is_raise(br)) {
        json_t *la = json_array();
        json_append(la, json_string("[%s] Failed to rebuild polling HTTP client after hung drain"));
        json_append(la, json_string(ta16_str(ta16_state_get(ctx, "name")) ? ta16_str(ta16_state_get(ctx, "name")) : "telegram"));
        json_t *lg = ta16_logger_args(la, true);
        json_t *r = ta16_call(ctx, "logger_debug", lg); json_free(r); json_free(br);
        return json_null();
    }
    json_free(br);
    json_t *la = json_array();
    json_append(la, json_string("[%s] Replaced wedged getUpdates HTTP client after drain timeout (likely CLOSE-WAIT socket)"));
    json_append(la, json_string(ta16_str(ta16_state_get(ctx, "name")) ? ta16_str(ta16_state_get(ctx, "name")) : "telegram"));
    json_t *lg = ta16_logger_args(la, false);
    json_t *wr = ta16_call(ctx, "logger_warning", lg); json_free(wr);
    json_t *tr = ta16_call(ctx, "ensure_future", json_object());
    if (ta16_is_raise(tr)) return tr;
    json_t *ada = json_object(); json_set(ada, "task", json_copy(tr));
    json_t *ar = ta16_call(ctx, "background_add", ada);
    if (ta16_is_raise(ar)) { json_free(ar); json_free(tr); return json_null(); }
    json_free(ar);
    /* Live: task.add_done_callback(self._background_tasks.discard) and
     * task.add_done_callback(_consume_abandoned_task), inside try/except pass.
     * A scripted task that is a task-double carries add_done_callback; a plain
     * scripted value (null/string) raises AttributeError and is swallowed. */
    if (tr && tr->type == JSON_OBJECT && ta16_truthy(json_obj_get(tr, "add_done_callback"))) {
        json_t *ca = json_object();
        json_set(ca, "task", json_copy(tr)); json_set_hdr(ca, "cb", "<lambda>");
        json_t *cr = ta16_call(ctx, "task_done_callback", ca);
        json_free(cr);
        json_t *ca2 = json_object();
        json_set(ca2, "task", json_copy(tr)); json_set_hdr(ca2, "cb", "<lambda>");
        json_t *cr2 = ta16_call(ctx, "task_done_callback", ca2);
        json_free(cr2);
    }
    json_free(tr);
    return json_null();
}

/* PoP: ta16_paged_keyboard @ plugins/platforms/telegram/adapter.py:_paged_keyboard */
static json_t *ta16_paged_keyboard(ta16_ctx_t *ctx, const json_t *args) {
    double cols = json_get_num(args, "columns", 2.0);
    json_t *rows;
    if (cols == 2.0) {
        json_t *a = json_object(); json_set(a, "buttons", ta16_copyv(ta16_arg(args, "buttons")));
        rows = ta16_call(ctx, "rows_of_two", a);
        if (ta16_is_raise(rows)) return rows;
    } else {
        rows = json_array();
        const json_t *btns = ta16_arg(args, "buttons");
        if (btns && btns->type == JSON_ARRAY)
            for (size_t i = 0; i < btns->c.count; i++) {
                json_t *row = json_array(); json_append(row, json_copy(btns->c.items[i]));
                json_append(rows, row);
            }
    }
    const json_t *pm = ta16_arg(args, "page_meta");
    const json_t *total_v = (pm && pm->type == JSON_OBJECT) ? json_obj_get((json_t *)pm, "total_pages") : NULL;
    double total = ta16_num(total_v);
    if (total > 1.0) {
        json_t *a = json_object();
        json_set(a, "page", ta16_copyv((pm && pm->type == JSON_OBJECT) ? json_obj_get((json_t *)pm, "page") : NULL));
        json_set(a, "total_pages", ta16_copyv(total_v));
        json_set(a, "nav_prefix", ta16_copyv(ta16_arg(args, "nav_prefix")));
        json_t *nav = ta16_call(ctx, "picker_nav_row", a);
        if (ta16_is_raise(nav)) { json_free(rows); return nav; }
        /* rows.append(nav_row): the whole returned row is ONE row. */
        json_append(rows, ta16_copyv(nav));
        json_free(nav);
    }
    /* rows.append(tail_row): the whole tail_row list is ONE row. */
    json_t *tail = (json_t *)ta16_arg(args, "tail_row");
    json_append(rows, ta16_copyv(tail));
    json_t *kb = json_object(); json_set(kb, "rows", json_copy(rows));
    json_free(rows);
    json_t *out = json_array();
    json_append(out, kb);
    json_append(out, ta16_copyv((pm && pm->type == JSON_OBJECT) ? json_obj_get((json_t *)pm, "page_info") : NULL));
    return out;
}

/* PoP: ta16_persist_dm_topic_thread_id @ plugins/platforms/telegram/adapter.py:_persist_dm_topic_thread_id */
static json_t *ta16_persist_dm_topic_thread_id(ta16_ctx_t *ctx, const json_t *args) {
    const char *aname = ta16_str(ta16_state_get(ctx, "name"));
    if (!aname) aname = "telegram";
    json_t *exc = NULL;
    json_t *hr = ta16_call(ctx, "get_hermes_home", json_object());
    if (ta16_is_raise(hr)) { exc = hr; goto pdt_except; }
    const char *home = ta16_str(hr) ? ta16_str(hr) : "";
    char *path = ta16_fmt("%s%sconfig.yaml", home, (home[0] && home[strlen(home)-1]=='/') ? "" : "/");
    json_t *ce = ta16_call(ctx, "config_exists", ta16_args_s("path", path));
    if (ta16_is_raise(ce)) { exc = ce; goto pdt_except; }
    if (!ta16_truthy(ce)) {
        json_t *la = json_array();
        json_append(la, json_string("[%s] Config file not found at %s, cannot persist thread_id"));
        json_append(la, json_string(aname));
        json_append(la, json_string(path));
        json_t *lg = ta16_logger_args(la, false);
        json_t *lr = ta16_call(ctx, "logger_warning", lg); json_free(lr);
        json_free(ce); free(path); json_free(hr);
        return json_null();
    }
    json_free(ce);
    json_t *cr = ta16_call(ctx, "read_config", ta16_args_s("path", path));
    if (ta16_is_raise(cr)) { exc = cr; goto pdt_except; }
    json_t *config = cr;
    if (!config || config->type != JSON_OBJECT) { json_free(config); free(path); json_free(hr); return json_null(); }
    /* setdefault chain */
    json_t *platforms = json_obj_get(config, "platforms");
    if (!platforms || platforms->type != JSON_OBJECT) { platforms = json_object(); json_set(config, "platforms", platforms); }
    json_t *telegram = json_obj_get(platforms, "telegram");
    if (!telegram || telegram->type != JSON_OBJECT) { telegram = json_object(); json_set(platforms, "telegram", telegram); }
    json_t *extra = json_obj_get(telegram, "extra");
    if (!extra || extra->type != JSON_OBJECT) { extra = json_object(); json_set(telegram, "extra", extra); }
    json_t *topics_root = json_obj_get(extra, "dm_topics");
    if (!topics_root || topics_root->type != JSON_ARRAY) { topics_root = json_array(); json_set(extra, "dm_topics", topics_root); }
    const json_t *cid_arg = ta16_arg(args, "chat_id");
    const char *tname = json_get_str(args, "topic_name", "");
    const json_t *tid_arg = ta16_arg(args, "thread_id");
    bool replace = json_get_bool(args, "replace_existing", false);
    double want_cid = ta16_num(cid_arg);
    double want_tid = ta16_num(tid_arg);
    bool changed = false, matched = false;
    for (size_t i = 0; i < topics_root->c.count; i++) {
        json_t *ce2 = topics_root->c.items[i];
        if (!ce2 || ce2->type != JSON_OBJECT) continue;
        const json_t *ecid = json_obj_get(ce2, "chat_id");
        double ec = ta16_num(ecid);
        if (ec != want_cid) continue;
        matched = true;
        json_t *topics = json_obj_get(ce2, "topics");
        if (!topics || topics->type != JSON_ARRAY) { topics = json_array(); json_set(ce2, "topics", topics); }
        json_t *tfound = NULL;
        for (size_t j = 0; j < topics->c.count; j++) {
            const char *nm = json_get_str(topics->c.items[j], "name", "");
            if (nm && strcmp(nm, tname) == 0) { tfound = topics->c.items[j]; break; }
        }
        if (!tfound) {
            json_t *nt = json_object();
            json_set_hdr(nt, "name", tname); json_set(nt, "thread_id", json_copy((json_t *)tid_arg));
            json_append(topics, nt); changed = true;
        } else {
            const json_t *etid = json_obj_get(tfound, "thread_id");
            double cur = ta16_num(etid);
            if ((replace || !ta16_truthy(etid)) && cur != want_tid) {
                json_set(tfound, "thread_id", json_copy((json_t *)tid_arg)); changed = true;
            }
        }
        break;
    }
    if (!matched) {
        json_t *ne = json_object();
        json_set(ne, "chat_id", json_copy((json_t *)cid_arg));
        json_t *tp = json_array();
        json_t *nt = json_object();
        json_set_hdr(nt, "name", tname); json_set(nt, "thread_id", json_copy((json_t *)tid_arg));
        json_append(tp, nt); json_set(ne, "topics", tp);
        json_append(topics_root, ne); changed = true;
    }
    if (changed) {
        json_t *wa = json_object();
        json_set_hdr(wa, "path", path); json_set(wa, "config", json_copy(config));
        json_t *wr = ta16_call(ctx, "write_config", wa);
        if (ta16_is_raise(wr)) { json_free(wr); }
        json_t *la = json_array();
        json_append(la, json_string("[%s] Persisted thread_id=%s for topic '%s' in config.yaml"));
        json_append(la, json_string(aname));
        json_append(la, json_copy((json_t *)tid_arg));
        json_append(la, json_string(tname));
        json_t *lg = ta16_logger_args(la, false);
        json_t *lr = ta16_call(ctx, "logger_info", lg); json_free(lr);
    }
    json_free(config); free(path); json_free(hr);
    return json_null();

pdt_except:
    {
        json_t *la = json_array();
        json_append(la, json_string("[%s] Failed to persist thread_id to config: %s"));
        json_append(la, json_string(aname));
        json_append(la, exc ? json_copy(exc) : ta16_err("RuntimeError", "persist"));
        json_t *lg = ta16_logger_args(la, true);
        json_t *r = ta16_call(ctx, "logger_warning", lg); json_free(r);
        json_free(exc);
        return json_null();
    }
}

/* PoP: ta16_photo_batch_key @ plugins/platforms/telegram/adapter.py:_photo_batch_key */
static json_t *ta16_photo_batch_key(ta16_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object(); json_set(a, "event", ta16_copyv(ta16_arg(args, "event")));
    json_t *skr = ta16_call(ctx, "event_session_key", a);
    if (ta16_is_raise(skr)) return skr;
    json_t *b = json_object(); json_set(b, "event", ta16_copyv(ta16_arg(args, "event")));
    json_t *sur = ta16_call(ctx, "sender_suffix", b);
    if (ta16_is_raise(sur)) { json_free(skr); return sur; }
    const char *sk = ta16_str(skr) ? ta16_str(skr) : "";
    const char *suf = ta16_str(sur) ? ta16_str(sur) : "";
    const json_t *msg = ta16_arg(args, "msg");
    const json_t *mgid = (msg && msg->type == JSON_OBJECT) ? json_obj_get((json_t *)msg, "media_group_id") : NULL;
    char *out;
    if (ta16_truthy(mgid)) {
        char *m = ta16_py_str(mgid);
        out = ta16_fmt("%s%s:album:%s", sk, suf, m ? m : "");
        free(m);
    } else out = ta16_fmt("%s%s:photo-burst", sk, suf);
    json_t *r = json_string(out); free(out);
    json_free(skr); json_free(sur);
    return r;
}

/* PoP: ta16_picker_edit @ plugins/platforms/telegram/adapter.py:_picker_edit */
static json_t *ta16_picker_edit(ta16_ctx_t *ctx, const json_t *args) {
    json_t *aa = json_object();
    json_set_hdr(aa, "query", "_Query");
    json_set(aa, "text", ta16_copyv(ta16_arg(args, "toast")));
    json_t *ar = ta16_call(ctx, "answer_callback", aa);
    if (ta16_is_raise(ar)) return ar;
    json_free(ar);
    json_t *fa = json_object();
    json_set(fa, "text", ta16_copyv(ta16_arg(args, "text_md")));
    json_t *fr = ta16_call(ctx, "format_message", fa);
    if (ta16_is_raise(fr)) return fr;
    const char *fmt = ta16_str(fr) ? ta16_str(fr) : "";
    json_t *ea = json_object();
    json_set_hdr(ea, "text", fmt);
    json_set_hdr(ea, "parse_mode", "MARKDOWN_V2");
    json_set(ea, "reply_markup", ta16_copyv(ta16_arg(args, "keyboard")));
    json_t *er = ta16_call(ctx, "edit_message_text", ea);
    if (ta16_is_raise(er)) {
        bool tg_err = (strcmp(ta16_raise_kind(er), "TelegramError") == 0);
        if (tg_err) {
            json_t *ra = json_object(); json_set(ra, "error", json_copy(er));
            json_t *rr = ta16_call(ctx, "redact_error", ra);
            const char *red = ta16_str(rr) ? ta16_str(rr) : "";
            json_t *la = json_array();
            json_append(la, json_string("[%s] model picker re-render failed: %s"));
            json_append(la, json_string(ta16_str(ta16_state_get(ctx, "name")) ? ta16_str(ta16_state_get(ctx, "name")) : "telegram"));
            json_append(la, json_string(red));
            json_t *lg = ta16_logger_args(la, false);
            json_t *lr = ta16_call(ctx, "logger_warning", lg); json_free(lr);
            json_free(rr);
            json_free(er); json_free(fr);
            return json_null();
        }
        json_free(fr);
        return er; /* non-TelegramError propagates */
    }
    json_free(er); json_free(fr);
    return json_null();
}

/* PoP: ta16_should_attempt_rich_draft @ plugins/platforms/telegram/adapter.py:_should_attempt_rich_draft */
static json_t *ta16_should_attempt_rich_draft(ta16_ctx_t *ctx, const json_t *args) {
    if (!ta16_truthy(ta16_state_get(ctx, "_rich_messages_enabled"))) return json_bool(false);
    if (!ta16_truthy(ta16_state_get(ctx, "_rich_drafts_enabled"))) return json_bool(false);
    if (ta16_truthy(ta16_state_get(ctx, "_rich_send_disabled"))) return json_bool(false);
    if (ta16_truthy(ta16_state_get(ctx, "_rich_draft_disabled"))) return json_bool(false);
    json_t *a = json_object(); json_set(a, "content", ta16_copyv(ta16_arg(args, "content")));
    json_t *r = ta16_call(ctx, "rich_content_ok", a);
    if (ta16_is_raise(r)) return r;
    bool ok = ta16_truthy(r); json_free(r);
    return json_bool(ok);
}

/* PoP: ta16_restart_task_attr @ plugins/platforms/telegram/adapter.py:_restart_task_attr */
static json_t *ta16_restart_task_attr(ta16_ctx_t *ctx, const json_t *args) {
    const char *attr = json_get_str(args, "attr", "");
    const json_t *prior = ta16_state_get(ctx, attr);
    if (ta16_truthy(prior)) {
        json_t *dr = ta16_call(ctx, "task_done", json_object());
        if (ta16_is_raise(dr)) return dr;
        bool done = ta16_truthy(dr); json_free(dr);
        if (!done) { json_t *cr = ta16_call(ctx, "task_cancel", json_object());
            if (ta16_is_raise(cr)) return cr; json_free(cr); }
    }
    json_t *er = ta16_call(ctx, "ensure_future", json_object());
    if (ta16_is_raise(er)) return er;
    ta16_state_set(ctx, attr, er);
    json_free(er);
    return json_null();
}
