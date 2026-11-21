/*
 * telegram_adapter_flow10.c - Telegram adapter FLOW part 10 of
 * plugins/platforms/telegram/adapter.py (hermes-agent): a coherent batch of
 * UNCLAIMED defs left after telegram_adapter_flow{,2,3,6,7,8,9}.c.  Every
 * target below was confirmed UNCLAIMED (not one of the PoPs in flows 1-9);
 * no flow 1-9 target is re-ported.
 *
 * Coherent theme: small admission/status/format helpers, deadline wrappers,
 * disconnect/refresh loops, and picker/event builders.  All live I/O and
 * injected callables enter through ONE universal seam: ta10_call(ctx,name,args).
 * Pure leaves are transcribed inline (no seam, no PoP re-claim).
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * Every symbol is static and ta10_-prefixed.
 *
 * SEAM NAME SET (must equal oracle SEAM_NAMES exactly):
 *   see tests/oracle_telegram_adapter_flow10.py SEAM_NAMES.
 *
 * Representation contract (shared with tests/oracle_telegram_adapter_flow10.py):
 * - NULL json_t* == Python None / absent; JSON_NULL is a real null value.
 * - Seam scripts: a plain value repeats; a LIST is consumed per call (last
 *   repeats); {"__script__":true,"ret":v,"raise":{type,message}} forces a
 *   result or raise. Missing/exhausted script => fail-closed RuntimeError
 *   recorded in the trace before propagation (BaseException passthrough).
 * - Except sites swallow ONLY Exception-family kinds (TA10_EXC_HIER);
 *   BaseException-only kinds pass through every except-Exception site.
 * - Python `not` truthiness is ta10_truthy; attribute reads with a default
 *   yield NULL when absent.  ints render as %.0f.
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

/* ── universal seam context ── */
typedef struct ta10_ctx {
    json_t *state;        /* owned; carries the attribute-double fields */
    const json_t *io;     /* borrowed scripted seam results */
    json_t *cursors;      /* owned; per-name array cursors */
    json_t *trace;        /* owned; ordered seam events */
    const json_t *errors; /* borrowed exception-graph registry */
} ta10_ctx_t;

/* ── basic helpers ── */
static bool ta10_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *ta10_raise_kind(const json_t *v) {
    return ta10_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static bool ta10_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool ta10_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *ta10_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *ta10_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static const json_t *ta10_arg(const json_t *args, const char *key) {
    return args ? json_obj_get(args, key) : NULL;
}
static const json_t *ta10_getattr(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
static const json_t *ta10_state_get(const ta10_ctx_t *ctx, const char *key) {
    return (ctx && ctx->state && ctx->state->type == JSON_OBJECT)
        ? json_obj_get(ctx->state, key) : NULL;
}
static double ta10_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}

/* ── string helpers ── */
static char *ta10_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static char *ta10_lower(const char *s) {
    char *o = ta10_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static char *ta10_fmt(const char *fmt, ...) {
    char tmp[512]; va_list ap;
    va_start(ap, fmt); int n = vsnprintf(tmp, sizeof tmp, fmt, ap); va_end(ap);
    if (n < 0) return ta10_strdup("");
    char *o = malloc((size_t)n + 1);
    if (!o) return ta10_strdup("");
    va_start(ap, fmt); vsnprintf(o, (size_t)n + 1, fmt, ap); va_end(ap);
    return o;
}
/* Python str() of a JSON scalar; integral doubles render as ints. */
static char *ta10_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return ta10_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return ta10_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e16) return ta10_fmt("%.0f", d);
            return ta10_fmt("%.17g", d);
        }
        case JSON_STRING: return ta10_strdup(v->str_val ? v->str_val : "");
        default: { char *s = json_serialize((json_t *)v); return s ? s : ta10_strdup(""); }
    }
}
/* strip leading/trailing ASCII whitespace (Python str.strip for ASCII cases) */
static char *ta10_strip(const char *s) {
    if (!s) return ta10_strdup("");
    while (*s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\v' || *s == '\f')) s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == '\v' || s[n-1] == '\f')) n--;
    char *o = malloc(n + 1);
    if (o) { memcpy(o, s, n); o[n] = '\0'; }
    return o;
}

/* ── FULL builtin exception hierarchy (CPython 3.14) ── */
typedef struct { const char *name; const char *parent; } ta10_exc_row_t;
static const ta10_exc_row_t TA10_EXC_HIER[] = {
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
static const char *ta10_builtin_parent(const char *name) {
    for (size_t i = 0; i < sizeof(TA10_EXC_HIER) / sizeof(TA10_EXC_HIER[0]); i++)
        if (strcmp(name, TA10_EXC_HIER[i].name) == 0) return TA10_EXC_HIER[i].parent;
    return NULL;
}
static bool ta10_chain_has(const char *start, const char *target) {
    const char *cur = start;
    for (int depth = 0; cur && depth < 24; depth++) {
        if (strcmp(cur, target) == 0) return true;
        cur = ta10_builtin_parent(cur);
    }
    return false;
}
static bool ta10_is_exception(const char *kind) {
    if (!kind) return false;
    if (ta10_builtin_parent(kind) == NULL) return true;
    return ta10_chain_has(kind, "Exception");
}

/* ── the one universal seam ──
 * args is ADOPTED into the trace event; the return is a fresh owned node. */
static json_t *ta10_call(ta10_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ta10_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ta10_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = ta10_err(json_get_str(raise, "type", "RuntimeError"),
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

/* ── seam-arg helpers ── */
static json_t *ta10_sarg(const char *k, const char *v) {
    json_t *a = json_object(); json_set_hdr(a, k, v); return a;
}
static json_t *ta10_ikb(ta10_ctx_t *ctx, const char *text, const char *cb) {
    json_t *a = json_object();
    json_set_hdr(a, "text", text);
    json_set_hdr(a, "callback_data", cb);
    return ta10_call(ctx, "InlineKeyboardButton", a);
}
/* t(key) -- records {"key":k,"kwargs":{}} */
static json_t *ta10_t(ta10_ctx_t *ctx, const char *key) {
    json_t *a = json_object();
    json_set_hdr(a, "key", key);
    json_set(a, "kwargs", json_object());
    return ta10_call(ctx, "t", a);
}

/* ═══════════════════════ ported flows ═══════════════════════ */

/* PoP: ta10_accept_update @ plugins/platforms/telegram/adapter.py:_accept_update */
static json_t *ta10_accept_update(ta10_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *adm = ta10_state_get(ctx, "_update_admission");
    if (ta10_is_null(adm)) return json_null();
    json_t *claim = ta10_call(ctx, "admission_get", json_object());
    if (ta10_is_raise(claim)) return claim;
    json_t *out = claim ? json_copy(claim) : json_null();
    if (claim && claim->type == JSON_OBJECT) json_set(out, "accepted", json_bool(true));
    json_set(ctx->state, "_claim", out);
    return json_null();
}

/* PoP: ta10_bot_commands @ plugins/platforms/telegram/adapter.py:_bot_commands */
static json_t *ta10_bot_commands(ta10_ctx_t *ctx, const json_t *args) {
    const json_t *menu = ta10_arg(args, "menu_commands");
    json_t *rows = json_array();
    if (!menu || menu->type != JSON_ARRAY) return rows;
    for (size_t i = 0; i < json_len(menu); i++) {
        const json_t *pair = json_get(menu, i);
        const json_t *nm = (pair && pair->type == JSON_ARRAY) ? json_get(pair, 0) : NULL;
        const json_t *ds = (pair && pair->type == JSON_ARRAY) ? json_get(pair, 1) : NULL;
        char *name = ta10_py_str(nm);
        char *desc = ta10_truthy(ds) ? ta10_py_str(ds) : ta10_strdup("");
        if (strlen(desc) > 256) desc[256] = '\0';
        json_t *a = json_object();
        json_set_hdr(a, "name", name ? name : "");
        json_set_hdr(a, "description", desc ? desc : "");
        free(name); free(desc);
        json_t *r = ta10_call(ctx, "BotCommand", a);
        if (ta10_is_raise(r)) return r;
        json_append(rows, r);
    }
    return rows;
}

/* PoP: ta10_await_with_thread_deadline @ plugins/platforms/telegram/adapter.py:_await_with_thread_deadline */
static json_t *ta10_await_with_thread_deadline(ta10_ctx_t *ctx, const json_t *args) {
    const json_t *aw = ta10_arg(args, "awaitable");
    const json_t *to = ta10_arg(args, "timeout");
    const json_t *label = ta10_arg(args, "label");
    const json_t *on_ab = ta10_arg(args, "on_abandon");
    const json_t *dump = ta10_arg(args, "dump_on_blocked_loop");
    json_t *a = json_object();
    json_set(a, "awaitable", aw ? json_copy((json_t *)aw) : json_null());
    json_set(a, "timeout", to ? json_copy((json_t *)to) : json_null());
    json_set(a, "label", label ? json_copy((json_t *)label) : json_null());
    json_set(a, "on_abandon", on_ab ? json_copy((json_t *)on_ab) : json_null());
    json_set(a, "dump_on_blocked_loop", dump ? json_copy((json_t *)dump) : json_null());
    json_t *r = ta10_call(ctx, "run_bounded_async", a);
    if (ta10_is_raise(r)) return r;
    if (ta10_truthy(ta10_getattr(r, "timed_out"))) {
        const char *l = ta10_str(label) ? ta10_str(label) : "telegram";
        char *msg = ta10_fmt("timed out after %.0fs (%s)", ta10_num(to), l);
        json_t *e = ta10_err("TimeoutError", msg ? msg : "");
        free(msg);
        return e;
    }
    const json_t *val = ta10_getattr(r, "value");
    return val ? json_copy((json_t *)val) : json_null();
}

/* PoP: ta10_bounded_request_step @ plugins/platforms/telegram/adapter.py:_bounded_request_step */
static json_t *ta10_bounded_request_step(ta10_ctx_t *ctx, const json_t *args) {
    const json_t *aw = ta10_arg(args, "awaitable");
    const json_t *fm = ta10_arg(args, "failure_msg");
    json_t *inner = json_object();
    json_set(inner, "awaitable", aw ? json_copy((json_t *)aw) : json_null());
    json_set(inner, "timeout", json_number(15.0));
    json_set_hdr(inner, "label", "telegram");
    json_set(inner, "on_abandon", json_null());
    json_set(inner, "dump_on_blocked_loop", json_bool(true));
    json_t *r = ta10_await_with_thread_deadline(ctx, inner);
    if (ta10_is_raise(r)) {
        const char *kind = ta10_raise_kind(r);
        if (!ta10_is_exception(kind)) return r;  /* BaseException passthrough */
        char *fmt = ta10_fmt("[%s] %s", "%s", ta10_str(fm) ? ta10_str(fm) : "");
        json_t *a = json_object();
        json_set_hdr(a, "fmt", fmt ? fmt : "");
        json_t *nm = ta10_state_get(ctx, "name");
        json_set(a, "name", nm ? json_copy((json_t *)nm) : json_null());
        json_set(a, "exc_info", json_bool(true));
        free(fmt);
        json_t *d = ta10_call(ctx, "logger_debug", a);
        if (ta10_is_raise(d)) { json_free(r); return d; }
        json_free(r);
        return json_bool(false);
    }
    json_free(r);
    return json_bool(true);
}

/* PoP: ta10_bot_identity_refresh_loop @ plugins/platforms/telegram/adapter.py:_bot_identity_refresh_loop */
static json_t *ta10_bot_identity_refresh_loop(ta10_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *ttl = ta10_state_get(ctx, "_BOT_IDENTITY_TTL_SECONDS");
    for (int guard = 0; guard < 32; guard++) {
        json_t *a = json_object();
        json_set(a, "seconds", ttl ? json_copy((json_t *)ttl) : json_number(0));
        json_t *s = ta10_call(ctx, "sleep", a);
        if (ta10_is_raise(s)) {
            const char *kind = ta10_raise_kind(s);
            if (strcmp(kind, "CancelledError") == 0) return json_null();
            if (!ta10_is_exception(kind)) return s;
            json_t *lg = json_object();
            json_set_hdr(lg, "fmt", "[%s] Telegram identity refresh loop iteration failed");
            json_t *nmv = ta10_state_get(ctx, "name");
            json_set(lg, "name", nmv ? json_copy((json_t *)nmv) : json_null());
            json_set(lg, "exc_info", json_bool(true));
            json_free(s); s = ta10_call(ctx, "logger_debug", lg);
            if (ta10_is_raise(s)) return s;
            json_free(s);
            continue;
        }
        json_free(s);
        if (ta10_truthy(ta10_state_get(ctx, "_teardown_started")) ||
            ta10_truthy(ta10_state_get(ctx, "has_fatal_error"))) return json_null();
        json_t *r = json_object();
        json_set(r, "force", json_bool(true));
        json_t *rf = ta10_call(ctx, "refresh_bot_identity", r);
        if (ta10_is_raise(rf)) {
            const char *kind = ta10_raise_kind(rf);
            if (strcmp(kind, "CancelledError") == 0) return json_null();
            if (!ta10_is_exception(kind)) return rf;
            json_t *lg = json_object();
            json_set_hdr(lg, "fmt", "[%s] Telegram identity refresh loop iteration failed");
            json_t *nmv2 = ta10_state_get(ctx, "name");
            json_set(lg, "name", nmv2 ? json_copy((json_t *)nmv2) : json_null());
            json_set(lg, "exc_info", json_bool(true));
            json_free(rf); rf = ta10_call(ctx, "logger_debug", lg);
            if (ta10_is_raise(rf)) return rf;
            json_free(rf);
            continue;
        }
        json_free(rf);
    }
    return json_null();
}

/* PoP: ta10_after_legacy_edit @ plugins/platforms/telegram/adapter.py:_after_legacy_edit */
static json_t *ta10_after_legacy_edit(ta10_ctx_t *ctx, const json_t *args) {
    const json_t *chat = ta10_arg(args, "chat_id");
    const json_t *mid = ta10_arg(args, "message_id");
    const json_t *content = ta10_arg(args, "content");
    const json_t *finalize = ta10_arg(args, "finalize");
    if (ta10_truthy(ta10_arg(args, "saturated_preview"))) {
        /* Tuple-key dict; fixtures keep this false. Mirrored as a string key. */
        char *k = ta10_fmt("%s|%s", ta10_str(chat) ? ta10_str(chat) : "",
                           ta10_str(mid) ? ta10_str(mid) : "");
        const json_t *prev = ta10_state_get(ctx, "_last_overflow_preview");
        json_t *d = (prev && prev->type == JSON_OBJECT) ? json_copy((json_t *)prev) : json_object();
        json_set(d, k, content ? json_copy((json_t *)content) : json_null());
        json_set(ctx->state, "_last_overflow_preview", d);
        free(k);
    }
    if (ta10_truthy(finalize)) {
        json_t *a = json_object();
        json_set(a, "content", content ? json_copy((json_t *)content) : json_null());
        json_t *q = ta10_call(ctx, "quotable_text", a);
        if (ta10_is_raise(q)) return q;
        json_t *b = json_object();
        json_set(b, "chat_id", chat ? json_copy((json_t *)chat) : json_null());
        json_set(b, "message_id", mid ? json_copy((json_t *)mid) : json_null());
        json_set(b, "text", q);
        json_t *r = ta10_call(ctx, "record_sent_text", b);
        if (ta10_is_raise(r)) return r;
        json_free(r);
    }
    return json_null();
}

/* PoP: ta10_attach_cached @ plugins/platforms/telegram/adapter.py:_attach_cached */
static json_t *ta10_attach_cached(ta10_ctx_t *ctx, const json_t *args) {
    const json_t *event_in = json_obj_get((json_t *)args, "event");
    const json_t *cached = ta10_arg(args, "cached");
    const json_t *note = ta10_arg(args, "note");
    const json_t *lf = ta10_arg(args, "log_fmt");
    if (!event_in || event_in->type != JSON_OBJECT) return json_null();
    /* Own a working copy, adopt it into state so every mutation (and the
       pre-exception value) echoes like drive()'s inst.event. Never adopt the
       borrowed child of args (that would double-free). */
    json_t *event = json_copy((json_t *)event_in);
    json_set(ctx->state, "event", event);
    /* pad_inline_flags(event) */
    json_t *a = json_object();
    json_set(a, "event", json_copy(event));
    json_t *pf = ta10_call(ctx, "pad_inline_flags", a);
    if (ta10_is_raise(pf)) return pf;
    json_t *inl = (pf && pf->type == JSON_ARRAY) ? json_copy(pf) : json_array();
    json_append(inl, json_bool(false));
    json_set(event, "media_text_inlined", inl);
    /* media_urls.append(cached.path); media_types.append(cached.media_type) */
    json_t *urls = json_obj_get(event, "media_urls");
    if (!urls || urls->type != JSON_ARRAY) { urls = json_array(); json_set(event, "media_urls", urls); }
    const json_t *path = ta10_getattr(cached, "path");
    json_append(urls, path ? json_copy((json_t *)path) : json_null());
    json_t *types = json_obj_get(event, "media_types");
    if (!types || types->type != JSON_ARRAY) { types = json_array(); json_set(event, "media_types", types); }
    const json_t *mt = ta10_getattr(cached, "media_type");
    json_append(types, mt ? json_copy((json_t *)mt) : json_null());
    /* if retype and len(urls)==1 and cached.kind in mapping: event.message_type = mapping[kind] */
    const json_t *kind = ta10_getattr(cached, "kind");
    const json_t *mapping = ta10_state_get(ctx, "_CACHED_KIND_TO_MESSAGE_TYPE");
    if (ta10_truthy(ta10_arg(args, "retype")) && json_len(urls) == 1 &&
        kind && ta10_getattr(mapping, ta10_str(kind) ? ta10_str(kind) : "")) {
        json_set(event, "message_type",
                 json_copy((json_t *)ta10_getattr(mapping, ta10_str(kind))));
    }
    /* event.text = self._append_observed_note(event.text, note) */
    json_t *b = json_object();
    const json_t *et = json_obj_get(event, "text");
    json_set(b, "existing", et ? json_copy((json_t *)et) : json_null());
    json_set(b, "note", note ? json_copy((json_t *)note) : json_null());
    json_t *an = ta10_call(ctx, "append_observed_note", b);
    if (ta10_is_raise(an)) return an;
    json_set(event, "text", an);
    /* logger.info(log_fmt, cached.kind, cached.path) */
    json_t *c = json_object();
    json_set(c, "fmt", lf ? json_copy((json_t *)lf) : json_null());
    json_set(c, "kind", kind ? json_copy((json_t *)kind) : json_null());
    json_set(c, "path", path ? json_copy((json_t *)path) : json_null());
    json_t *li = ta10_call(ctx, "logger_info", c);
    if (ta10_is_raise(li)) return li;
    json_free(li);
    return json_null();
}

/* PoP: ta10_build_triggered_event @ plugins/platforms/telegram/adapter.py:_build_triggered_event */
static json_t *ta10_build_triggered_event(ta10_ctx_t *ctx, const json_t *args) {
    const json_t *msg = ta10_arg(args, "msg");
    const json_t *update = ta10_arg(args, "update");
    const json_t *mt = ta10_arg(args, "msg_type");
    const json_t *uid = ta10_getattr(update, "update_id");
    json_t *a = json_object();
    json_set(a, "msg", msg ? json_copy((json_t *)msg) : json_null());
    json_set(a, "msg_type", mt ? json_copy((json_t *)mt) : json_null());
    json_set(a, "update_id", uid ? json_copy((json_t *)uid) : json_null());
    json_t *event = ta10_call(ctx, "_build_message_event", a);
    if (ta10_is_raise(event)) return event;
    if (!event || event->type != JSON_OBJECT) {
        json_set(ctx->state, "_event", event ? json_copy(event) : json_null());
        return event;
    }
    /* Adopt event into state so mutations / mid-exception values echo like drive(). */
    json_set(ctx->state, "_event", event);
    const json_t *et = json_obj_get(event, "text");
    json_t *b = json_object();
    json_set(b, "msg", msg ? json_copy((json_t *)msg) : json_null());
    json_set(b, "text", et ? json_copy((json_t *)et) : json_null());
    json_t *gt = ta10_call(ctx, "group_trigger_text", b);
    if (ta10_is_raise(gt)) return gt;
    json_set(event, "text", gt);
    json_t *c = json_object();
    json_set(c, "msg", msg ? json_copy((json_t *)msg) : json_null());
    json_set(c, "event", json_copy(event));
    json_t *cm = ta10_call(ctx, "_cache_replied_media", c);
    if (ta10_is_raise(cm)) return cm;
    json_free(cm);
    json_t *d = json_object();
    json_set(d, "event", json_copy(event));
    return ta10_call(ctx, "_apply_telegram_group_observe_attribution", d);
}

/* PoP: ta10_note_instead_of_media @ plugins/platforms/telegram/adapter.py:_note_instead_of_media */
static json_t *ta10_note_instead_of_media(ta10_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    const json_t *ev = ta10_arg(args, "event");
    const json_t *tx = ta10_arg(args, "text");
    json_set(a, "event", ev ? json_copy((json_t *)ev) : json_null());
    json_set(a, "text", tx ? json_copy((json_t *)tx) : json_null());
    json_t *r = ta10_call(ctx, "add_item_note", a);
    if (ta10_is_raise(r)) return r;
    json_free(r);
    return json_bool(false);
}

/* PoP: ta10_is_connected @ plugins/platforms/telegram/adapter.py:_is_connected */
static json_t *ta10_is_connected(ta10_ctx_t *ctx, const json_t *args) {
    const json_t *cfg = ta10_arg(args, "config");
    const json_t *token = ta10_getattr(cfg, "token");
    if (!ta10_truthy(token)) {
        json_t *r = ta10_call(ctx, "get_env_value",
                              ta10_sarg("key", "TELEGRAM_BOT_TOKEN"));
        if (ta10_is_raise(r)) return r;
        token = r;
    }
    const json_t *use = ta10_truthy(token) ? token : NULL;
    char *s = use ? ta10_py_str(use) : ta10_strdup("");
    char *st = ta10_strip(s ? s : "");
    free(s);
    bool ok = st && st[0] != '\0';
    free(st);
    return json_bool(ok);
}

/* PoP: ta10_truncate_stream_overflow_preview @ plugins/platforms/telegram/adapter.py:_truncate_stream_overflow_preview */
static json_t *ta10_truncate_stream_overflow_preview(ta10_ctx_t *ctx, const json_t *args) {
    const json_t *content = ta10_arg(args, "content");
    const json_t *mx = ta10_state_get(ctx, "MAX_MESSAGE_LENGTH");
    json_t *a = json_object();
    json_set(a, "content", content ? json_copy((json_t *)content) : json_null());
    json_set(a, "max_length", mx ? json_copy((json_t *)mx) : json_null());
    json_set_hdr(a, "len_fn", "utf16_len");
    json_t *r = ta10_call(ctx, "truncate_message", a);
    if (ta10_is_raise(r)) return r;
    if (r && r->type == JSON_ARRAY && json_len(r) > 0) {
        json_t *first = json_copy(json_get(r, 0));
        json_free(r);
        return first;
    }
    if (r && r->type == JSON_ARRAY) {
        json_free(r);
        return ta10_err("IndexError", "list index out of range");
    }
    json_free(r);
    return json_null();
}

/* PoP: ta10_parse_page @ plugins/platforms/telegram/adapter.py:_parse_page */
static json_t *ta10_parse_page(ta10_ctx_t *ctx, const json_t *args) {
    const json_t *raw = ta10_arg(args, "raw");
    const char *s = ta10_str(raw);
    if (s) {
        char *end = NULL;
        long v = strtol(s, &end, 10);
        while (*end == ' ') end++;
        if (end != s && *end == '\0') return json_number((double)v);
        /* ValueError path */
        json_t *t = ta10_call(ctx, "_toast",
                              ta10_sarg("key", "platform.telegram.picker.invalid_page"));
        if (ta10_is_raise(t)) return t;
        json_t *a = json_object();
        const json_t *q = ta10_arg(args, "query");
        json_set(a, "query", q ? json_copy((json_t *)q) : json_null());
        json_set(a, "text", t);
        json_t *r = ta10_call(ctx, "answer_callback", a);
        if (ta10_is_raise(r)) return r;
        json_free(r);
        return json_null();
    }
    /* int(None)/int(list) -> TypeError (uncaught) */
    return ta10_err("TypeError", "int() argument must be a string, a bytes-like object or a real number");
}

/* PoP: ta10_edit_html_quiet @ plugins/platforms/telegram/adapter.py:_edit_html_quiet */
static json_t *ta10_edit_html_quiet(ta10_ctx_t *ctx, const json_t *args) {
    const json_t *q = ta10_arg(args, "query");
    const json_t *tx = ta10_arg(args, "text");
    json_t *a = json_object();
    json_set(a, "query", q ? json_copy((json_t *)q) : json_null());
    json_set(a, "text", tx ? json_copy((json_t *)tx) : json_null());
    json_set_hdr(a, "parse_mode", "HTML");
    json_set(a, "reply_markup", json_null());
    json_t *r = ta10_call(ctx, "edit_message_text", a);
    if (ta10_is_raise(r)) {
        if (!ta10_is_exception(ta10_raise_kind(r))) return r;  /* BaseException passthrough */
    }
    json_free(r);
    return json_null();
}

/* PoP: ta10_build_model_keyboard @ plugins/platforms/telegram/adapter.py:_build_model_keyboard */
static json_t *ta10_build_model_keyboard(ta10_ctx_t *ctx, const json_t *args) {
    const json_t *models = ta10_arg(args, "models");
    const json_t *page = ta10_arg(args, "page");
    const json_t *prov = ta10_arg(args, "provider");
    const json_t *ps = ta10_state_get(ctx, "_MODEL_PAGE_SIZE");
    json_t *a = json_object();
    json_set(a, "items", models ? json_copy((json_t *)models) : json_null());
    json_set(a, "page", page ? json_copy((json_t *)page) : json_null());
    json_set(a, "page_size", ps ? json_copy((json_t *)ps) : json_null());
    json_t *fp = ta10_call(ctx, "_format_choice_page", a);
    if (ta10_is_raise(fp)) return fp;
    if (!fp || fp->type != JSON_ARRAY || json_len(fp) < 2) { json_free(fp); return json_null(); }
    const json_t *page_models = json_get(fp, 0);
    const json_t *page_meta = json_get(fp, 1);
    double start = ta10_num(ta10_getattr(page_meta, "start"));
    json_t *labels_in = json_array();
    if (models && models->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(models); i++) {
            char *s = ta10_py_str(json_get(models, i));
            json_append(labels_in, json_string(s ? s : ""));
            free(s);
        }
    }
    json_t *b = json_object();
    json_set(b, "labels", labels_in);
    json_t *lb = ta10_call(ctx, "model_button_labels", b);
    if (ta10_is_raise(lb)) { json_free(fp); return lb; }
    json_t *buttons = json_array();
    size_t n = (page_models && page_models->type == JSON_ARRAY) ? json_len(page_models) : 0;
    for (size_t i = 0; i < n; i++) {
        double idx = start + (double)i;
        if (!lb || lb->type != JSON_ARRAY || (size_t)idx >= json_len(lb)) {
            if (lb) json_free(lb);
            json_free(fp);
            return ta10_err("IndexError", "list index out of range");
        }
        const json_t *labv = json_get(lb, (size_t)idx);
        char *lab = ta10_py_str(labv);
        char *cb = ta10_fmt("mm:%.0f", idx);
        json_t *bt = json_object();
        json_set_hdr(bt, "text", lab ? lab : "");
        json_set_hdr(bt, "callback_data", cb ? cb : "");
        free(lab); free(cb);
        json_t *x = ta10_call(ctx, "InlineKeyboardButton", bt);
        if (ta10_is_raise(x)) { json_free(lb); json_free(fp); return x; }
        json_append(buttons, x);
    }
    json_free(lb);
    json_t *tail = ta10_call(ctx, "_picker_back_cancel_row", json_object());
    if (ta10_is_raise(tail)) { json_free(fp); return tail; }
    json_t *c = json_object();
    json_set(c, "buttons", buttons);
    json_set(c, "page_meta", page_meta ? json_copy((json_t *)page_meta) : json_null());
    json_set_hdr(c, "nav_prefix", "mg");
    json_set(c, "tail_row", tail);
    json_set(c, "columns", json_number(1));
    json_t *out = ta10_call(ctx, "_paged_keyboard", c);
    json_free(fp);
    return out;
}

/* PoP: ta10_await_disconnect_step @ plugins/platforms/telegram/adapter.py:_await_disconnect_step */
static json_t *ta10_await_disconnect_step(ta10_ctx_t *ctx, const json_t *args) {
    const json_t *aw = ta10_arg(args, "awaitable");
    const json_t *to = ta10_arg(args, "timeout");
    const json_t *step = ta10_arg(args, "step");
    json_t *a = json_object();
    json_set(a, "awaitable", aw ? json_copy((json_t *)aw) : json_null());
    json_t *task = ta10_call(ctx, "ensure_future", a);
    if (ta10_is_raise(task)) return task;  /* outside try */
    json_t *b = json_object();
    json_t *tasks = json_array();
    json_append(tasks, json_copy(task));
    json_set(b, "tasks", tasks);
    json_set(b, "timeout", (ta10_num(to) > 0 && to) ? json_copy((json_t *)to) : json_null());
    json_t *w = ta10_call(ctx, "wait", b);
    if (ta10_is_raise(w)) {
        const char *kind = ta10_raise_kind(w);
        if (strcmp(kind, "CancelledError") == 0) {
            json_t *c = json_object(); json_set(c, "task", json_copy(task));
            json_t *x = ta10_call(ctx, "task_cancel", c);
            if (ta10_is_raise(x)) { json_free(task); return x; }
            json_free(x);
            json_t *d = json_object();
            json_set(d, "task", json_copy(task));
            json_set_hdr(d, "callback", "_consume_abandoned_task");
            x = ta10_call(ctx, "add_done_callback", d);
            if (ta10_is_raise(x)) { json_free(task); return x; }
            json_free(x); json_free(task);
            return w;  /* re-raise */
        }
        json_free(task);
        return w;
    }
    bool done = json_get_bool(w, "done", false);
    json_free(w);
    if (done) {
        json_t *e = json_object(); json_set(e, "task", json_copy(task));
        json_t *awt = ta10_call(ctx, "task_await", e);
        if (ta10_is_raise(awt)) {
            const char *kind = ta10_raise_kind(awt);
            if (strcmp(kind, "CancelledError") != 0) { json_free(task); return awt; }
            /* suppressed */
        }
        json_free(awt); json_free(task);
        return json_bool(true);
    }
    json_t *c = json_object(); json_set(c, "task", json_copy(task));
    json_t *x = ta10_call(ctx, "task_cancel", c);
    if (ta10_is_raise(x)) { json_free(task); return x; }
    json_free(x);
    json_t *d = json_object();
    json_set(d, "task", json_copy(task));
    json_set_hdr(d, "callback", "_consume_abandoned_task");
    x = ta10_call(ctx, "add_done_callback", d);
    if (ta10_is_raise(x)) { json_free(task); return x; }
    json_free(x); json_free(task);
    json_t *g = json_object();
    json_set_hdr(g, "fmt", "[%s] %s timed out after %.1fs during disconnect; continuing teardown");
    json_t *nm = ta10_state_get(ctx, "name");
    json_set(g, "name", nm ? json_copy((json_t *)nm) : json_null());
    json_set(g, "step", step ? json_copy((json_t *)step) : json_null());
    json_set(g, "timeout", to ? json_copy((json_t *)to) : json_null());
    json_t *lw = ta10_call(ctx, "logger_warning", g);
    if (ta10_is_raise(lw)) return lw;
    json_free(lw);
    return json_bool(false);
}

/* PoP: ta10_build_provider_keyboard @ plugins/platforms/telegram/adapter.py:_build_provider_keyboard */
static json_t *ta10_build_provider_keyboard(ta10_ctx_t *ctx, const json_t *args) {
    const json_t *providers = ta10_arg(args, "providers");
    const json_t *page = ta10_arg(args, "page");
    bool present = ta10_truthy(ta10_state_get(ctx, "group_providers_present"));
    json_t *by_slug = json_object();
    if (providers && providers->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(providers); i++) {
            const json_t *p = json_get(providers, i);
            const json_t *s = ta10_getattr(p, "slug");
            char *key = ta10_lower(ta10_truthy(s) ? ta10_py_str(s) : "");
            json_set(by_slug, key ? key : "", p ? json_copy((json_t *)p) : json_null());
            free(key);
        }
    }
    json_t *buttons = json_array();
    if (present) {
        /* slugs: list of p.get("slug") (verbatim) */
        json_t *slugs = json_array();
        if (providers && providers->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(providers); i++) {
                const json_t *s = ta10_getattr(json_get(providers, i), "slug");
                json_append(slugs, s ? json_copy((json_t *)s) : json_null());
            }
        }
        json_t *a = json_object();
        json_set(a, "slugs", slugs);
        json_t *rows = ta10_call(ctx, "group_providers", a);
        if (ta10_is_raise(rows)) { json_free(by_slug); return rows; }
        if (rows && rows->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(rows); i++) {
                const json_t *row = json_get(rows, i);
                const char *kind = ta10_str(ta10_getattr(row, "kind"));
                if (kind && strcmp(kind, "group") == 0) {
                    const json_t *members = ta10_getattr(row, "members");
                    double count = 0;
                    bool any_current = false;
                    if (members && members->type == JSON_ARRAY) {
                        for (size_t j = 0; j < json_len(members); j++) {
                            const char *ms = ta10_str(json_get(members, j));
                            const json_t *m = ms ? ta10_getattr(by_slug, ms) : NULL;
                            if (!m) continue;
                            const json_t *tm = ta10_getattr(m, "total_models");
                            if (tm && tm->type == JSON_NUMBER) count += tm->num_val;
                            else {
                                const json_t *ml = ta10_getattr(m, "models");
                                count += (ml && ml->type == JSON_ARRAY) ? (double)json_len(ml) : 0.0;
                            }
                            if (ta10_truthy(ta10_getattr(m, "is_current"))) any_current = true;
                        }
                    }
                    const char *lbl = ta10_str(ta10_getattr(row, "label"));
                    char *lab = ta10_fmt("%s ▸ (%.0f)", lbl ? lbl : "", count);
                    if (any_current) { char *x = ta10_fmt("✓ %s", lab ? lab : ""); free(lab); lab = x; }
                    const char *gid = ta10_str(ta10_getattr(row, "group_id"));
                    char *cb = ta10_fmt("mpg:%s", gid ? gid : "");
                    json_t *bt = json_object();
                    json_set_hdr(bt, "text", lab ? lab : "");
                    json_set_hdr(bt, "callback_data", cb ? cb : "");
                    free(lab); free(cb);
                    json_t *x = ta10_call(ctx, "InlineKeyboardButton", bt);
                    if (ta10_is_raise(x)) { json_free(rows); json_free(by_slug); return x; }
                    json_append(buttons, x);
                } else {
                    const json_t *s = ta10_getattr(row, "slug");
                    const json_t *p = s ? ta10_getattr(by_slug, ta10_str(s) ? ta10_str(s) : "") : NULL;
                    if (p) {
                        json_t *bb = json_object(); json_set(bb, "p", json_copy((json_t *)p));
                        json_t *x = ta10_call(ctx, "_provider_button", bb);
                        if (ta10_is_raise(x)) { json_free(rows); json_free(by_slug); return x; }
                        json_append(buttons, x);
                    }
                }
            }
        }
        json_free(rows);
    } else {
        if (providers && providers->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(providers); i++) {
                json_t *bb = json_object(); json_set(bb, "p", json_copy(json_get(providers, i)));
                json_t *x = ta10_call(ctx, "_provider_button", bb);
                if (ta10_is_raise(x)) { json_free(by_slug); return x; }
                json_append(buttons, x);
            }
        }
    }
    json_free(by_slug);
    const json_t *ps = ta10_state_get(ctx, "_PROVIDER_PAGE_SIZE");
    json_t *c = json_object();
    json_set(c, "items", buttons);
    json_set(c, "page", page ? json_copy((json_t *)page) : json_null());
    json_set(c, "page_size", ps ? json_copy((json_t *)ps) : json_null());
    json_t *fp = ta10_call(ctx, "_format_choice_page", c);
    if (ta10_is_raise(fp)) return fp;
    if (!fp || fp->type != JSON_ARRAY || json_len(fp) < 2) { json_free(fp); return json_null(); }
    const json_t *pb = json_get(fp, 0);
    const json_t *pm = json_get(fp, 1);
    json_t *t = ta10_t(ctx, "platform.telegram.picker.cancel");
    if (ta10_is_raise(t)) { json_free(fp); return t; }
    char *ts = ta10_py_str(t);
    json_t *cx = ta10_ikb(ctx, ts ? ts : "", "mx");
    free(ts);
    if (ta10_is_raise(cx)) { json_free(fp); return cx; }
    json_t *tail = json_array();
    json_append(tail, cx);
    json_t *d = json_object();
    json_set(d, "buttons", pb ? json_copy((json_t *)pb) : json_null());
    json_set(d, "page_meta", pm ? json_copy((json_t *)pm) : json_null());
    json_set_hdr(d, "nav_prefix", "mpv");
    json_set(d, "tail_row", tail);
    json_set(d, "columns", json_number(2));
    json_t *out = ta10_call(ctx, "_paged_keyboard", d);
    json_free(fp);
    return out;
}
