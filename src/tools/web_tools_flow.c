/* web_tools_flow.c — tools/web_tools.py FLOW + leaves (web tools surface).
 *
 * Decision-sequence port of the generic web_search / web_extract selection,
 * availability, managed-routing, keyless-rescue, memoized-search and debug
 * finish core of tools/web_tools.py, plus its UNCLAIMED sync leaves in
 * tools/web_tools_rescue.py, tools/web_tools_truncate.py and
 * tools/web_tools_extract.py.
 *
 * Ported targets (27):
 *   web_tools.py:      _configured_backend, _registry_call, _probe,
 *                      _autodetect_backend, _keyless_backend,
 *                      _managed_web_search, _xai_available, _finish_debug,
 *                      _memoized_search (nested _paid_search /
 *                      _served_after_failure transcribed inline).
 *   web_tools_rescue:  _ring_vendor_keyless, _managed_search_fallback,
 *                      _rescue_search, _rescue_extract.
 *   web_tools_truncate:_clamp_char_limit, _clamp_or_default,
 *                      _effective_char_limit, _trim_results.
 *   web_tools_extract: _disabled_plugin_error, _no_provider_error,
 *                      _strict_selection_error, _result_entry,
 *                      _extract_error_json, _refuse_all, _merge_in_order,
 *                      _resolve_extract_provider, _extract_timeout_seconds,
 *                      _backend_failed_whole_batch.
 *
 * ALL external collaborators ride the injected universal seam (wt_call); the
 * harness scripts results and records the ordered trace the oracle diffs
 * against LIVE quarry code under the same scripts. Ported siblings called by a
 * target stay REAL (internal wt_* calls). CLAIMED siblings are SEAMED:
 *   _load_web_config, _has_env, _ddgs_package_importable,
 *   _list_registered_web_providers, _registered_web_provider,
 *   _ensure_web_plugins_loaded, _get_extract_char_limit, _rescue_eligible,
 *   _policy_blocked_result, read_selection, selection_exists, selection_error.
 * Foreign collaborators seamed: _is_tool_gateway_ready, check_firecrawl_api_key,
 *   get_provider, list_providers, get_active_extract_provider,
 *   _keyless_tier_enabled, _keyless_preference, _disabled_web_plugin_for,
 *   has_xai_credentials, peek_nous_access_token / resolve_free_search_gateway,
 *   get_provider_env, use_keyless, _use_keyless_ring, search_with_failover,
 *   extract_with_failover, is_origin_http_error, bucket_limit / search_memo /
 *   slice_search_response, tool_error, _debug.log_call / _debug.save,
 *   provider.<method> (is_available / is_keyless_available / search /
 *   supports_extract / supports_search).
 *
 * Exception model: targets that catch `except Exception` (_probe,
 * _registry_call, _keyless_backend, _xai_available, _managed_search_fallback,
 * nested _paid_search) swallow Exception subclasses but let the BaseException
 * trio (SystemExit / KeyboardInterrupt / GeneratorExit) pass through; the FULL
 * builtin hierarchy table below drives that decision. _clamp_or_default and
 * _extract_timeout_seconds catch only (TypeError, ValueError). No other target
 * catches: every other seam raise propagates (BaseException passthrough).
 *
 * Representation contract (shared with tests/oracle_web_tools_flow.py):
 *   - scripts: MISSING key -> fail-closed RuntimeError("no script for <name>");
 *     null -> None; {"$seq":[...]} per-call queue (exhaustion raises
 *     RuntimeError("script exhausted: <name>")); {"__raise__":cls,...} raises
 *     (trace ret is the WHOLE marker); any other value reused (a bare ARRAY is
 *     a LIST RESULT, not a queue). None-valued seam arg keys dropped BOTH sides.
 *   - provider doubles are attribute dicts (C reads JSON fields; the oracle
 *     materializes ProviderDouble with method recorders). search_memo.flight_lock
 *     records its script as ret but returns an inert context manager.
 *   - tuple returns (_refuse_all, _resolve_extract_provider) project as JSON
 *     arrays; integral floats collapse to ints (libjson %.0f); uncaught
 *     exception -> {"__raise__":"<ClassName>"}; bool<->int cross-equality holds.
 *   - _extract_error_json / _refuse_all return Python json.dumps COMPACT text
 *     (separators ", " / ": "); wt_dumps_compact reproduces it byte-for-byte.
 *
 * Fixture domain (ASCII scope):
 *   - strings ASCII (str.strip/lower/slice/split transcribed byte-wise).
 *   - int()/float() numeric strings are plain base-10 (optional sign/space, no
 *     underscores/hex/inf/nan); non-numeric -> ValueError, None/containers ->
 *     TypeError. str(exc) consumed only for scripted seam raises whose class
 *     has str==msg (RuntimeError/ValueError/OSError/Exception/...); KeyError and
 *     None.search receiver shapes are excluded from str(exc) consumers.
 *   - _registry_call func_name in {get_provider, list_providers}.
 *   - _probe method in {is_available, is_keyless_available}; provider doubles
 *     carry every probe method (getattr never fails on an object provider).
 *   - _merge_in_order fixed keys are stringified ints; total covers positions.
 *   - rescued / resp seam results are dicts; provider seam results are dicts or
 *     raises (never None where .search/.supports_extract is then read).
 *
 * libjson-only (default oracle deps suffice). Self-contained: the harness
 * #includes "tools/web_tools_flow.c".
 */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

/* ── context + seam plumbing ─────────────────────────────────────────────── */

typedef struct wt_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} wt_ctx;

static bool wt_str_eq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool wt_is_none(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool wt_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool wt_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool wt_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool wt_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool wt_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }

static bool wt_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *wt_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

/* Deep Python-style equality; NULL and JSON_NULL both read None. Object key
 * order irrelevant; array order matters; bool<->number cross-equality. */
static bool wt_json_eq(const json_t *a, const json_t *b) {
    if (wt_is_none(a) && wt_is_none(b)) return true;
    if (wt_is_none(a) || wt_is_none(b)) return false;
    if (a->type == JSON_BOOL && b->type == JSON_NUMBER)
        return (a->bool_val ? 1.0 : 0.0) == b->num_val;
    if (a->type == JSON_NUMBER && b->type == JSON_BOOL)
        return a->num_val == (b->bool_val ? 1.0 : 0.0);
    if (a->type != b->type) return false;
    switch (a->type) {
        case JSON_BOOL: return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return wt_str_eq(a->str_val, b->str_val);
        case JSON_ARRAY:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++)
                if (!wt_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                json_t *bv = json_obj_get(b, a->c.keys[i]);
                if (!bv || !wt_json_eq(a->c.items[i], bv)) return false;
            }
            return true;
        default: return true;
    }
}

/* ── exception plumbing + FULL builtin hierarchy ─────────────────────────── */

static bool wt_exc_pending(const wt_ctx *ctx) { return ctx && ctx->exc != NULL; }

static void wt_raise(wt_ctx *ctx, const char *cls) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
}
static void wt_raise_msg(wt_ctx *ctx, const char *cls, const char *msg) {
    if (!ctx) return;
    if (ctx->exc) json_free(ctx->exc);
    ctx->exc = json_object();
    json_set(ctx->exc, "__raise__", json_string(cls));
    json_set(ctx->exc, "msg", json_string(msg));
}
static const char *wt_exc_cls(const wt_ctx *ctx) {
    if (!ctx || !ctx->exc) return "";
    return json_get_str(ctx->exc, "__raise__", "");
}
static const char *wt_exc_msg(const wt_ctx *ctx) {
    if (!ctx || !ctx->exc) return "";
    return json_get_str(ctx->exc, "msg", "");
}

/* Parent link for every builtin exception class (NULL == BaseException root).
 * SystemExit / KeyboardInterrupt / GeneratorExit parent ONLY to BaseException,
 * so they pass through `except Exception`. */
static const char *wt_exc_parent(const char *c) {
    /* BaseException direct children */
    if (wt_str_eq(c, "Exception")) return "BaseException";
    if (wt_str_eq(c, "SystemExit")) return "BaseException";
    if (wt_str_eq(c, "KeyboardInterrupt")) return "BaseException";
    if (wt_str_eq(c, "GeneratorExit")) return "BaseException";
    if (wt_str_eq(c, "BaseExceptionGroup")) return "BaseException";
    if (wt_str_eq(c, "ExceptionGroup")) return "Exception";
    /* Exception direct children */
    if (wt_str_eq(c, "StopIteration")) return "Exception";
    if (wt_str_eq(c, "StopAsyncIteration")) return "Exception";
    if (wt_str_eq(c, "ArithmeticError")) return "Exception";
    if (wt_str_eq(c, "AssertionError")) return "Exception";
    if (wt_str_eq(c, "AttributeError")) return "Exception";
    if (wt_str_eq(c, "BufferError")) return "Exception";
    if (wt_str_eq(c, "EOFError")) return "Exception";
    if (wt_str_eq(c, "ImportError")) return "Exception";
    if (wt_str_eq(c, "LookupError")) return "Exception";
    if (wt_str_eq(c, "MemoryError")) return "Exception";
    if (wt_str_eq(c, "NameError")) return "Exception";
    if (wt_str_eq(c, "OSError")) return "Exception";
    if (wt_str_eq(c, "ReferenceError")) return "Exception";
    if (wt_str_eq(c, "RuntimeError")) return "Exception";
    if (wt_str_eq(c, "SyntaxError")) return "Exception";
    if (wt_str_eq(c, "SystemError")) return "Exception";
    if (wt_str_eq(c, "TypeError")) return "Exception";
    if (wt_str_eq(c, "ValueError")) return "Exception";
    if (wt_str_eq(c, "Warning")) return "Exception";
    /* ArithmeticError */
    if (wt_str_eq(c, "FloatingPointError")) return "ArithmeticError";
    if (wt_str_eq(c, "OverflowError")) return "ArithmeticError";
    if (wt_str_eq(c, "ZeroDivisionError")) return "ArithmeticError";
    /* ImportError */
    if (wt_str_eq(c, "ModuleNotFoundError")) return "ImportError";
    /* LookupError */
    if (wt_str_eq(c, "IndexError")) return "LookupError";
    if (wt_str_eq(c, "KeyError")) return "LookupError";
    /* NameError */
    if (wt_str_eq(c, "UnboundLocalError")) return "NameError";
    /* OSError aliases + children */
    if (wt_str_eq(c, "EnvironmentError")) return "OSError";
    if (wt_str_eq(c, "IOError")) return "OSError";
    if (wt_str_eq(c, "WindowsError")) return "OSError";
    if (wt_str_eq(c, "BlockingIOError")) return "OSError";
    if (wt_str_eq(c, "ChildProcessError")) return "OSError";
    if (wt_str_eq(c, "ConnectionError")) return "OSError";
    if (wt_str_eq(c, "FileExistsError")) return "OSError";
    if (wt_str_eq(c, "FileNotFoundError")) return "OSError";
    if (wt_str_eq(c, "InterruptedError")) return "OSError";
    if (wt_str_eq(c, "IsADirectoryError")) return "OSError";
    if (wt_str_eq(c, "NotADirectoryError")) return "OSError";
    if (wt_str_eq(c, "PermissionError")) return "OSError";
    if (wt_str_eq(c, "ProcessLookupError")) return "OSError";
    if (wt_str_eq(c, "TimeoutError")) return "OSError";
    /* ConnectionError */
    if (wt_str_eq(c, "BrokenPipeError")) return "ConnectionError";
    if (wt_str_eq(c, "ConnectionAbortedError")) return "ConnectionError";
    if (wt_str_eq(c, "ConnectionRefusedError")) return "ConnectionError";
    if (wt_str_eq(c, "ConnectionResetError")) return "ConnectionError";
    /* RuntimeError */
    if (wt_str_eq(c, "NotImplementedError")) return "RuntimeError";
    if (wt_str_eq(c, "RecursionError")) return "RuntimeError";
    /* SyntaxError */
    if (wt_str_eq(c, "IndentationError")) return "SyntaxError";
    if (wt_str_eq(c, "TabError")) return "IndentationError";
    /* ValueError */
    if (wt_str_eq(c, "UnicodeError")) return "ValueError";
    if (wt_str_eq(c, "UnicodeDecodeError")) return "UnicodeError";
    if (wt_str_eq(c, "UnicodeEncodeError")) return "UnicodeError";
    if (wt_str_eq(c, "UnicodeTranslateError")) return "UnicodeError";
    /* Warning */
    if (wt_str_eq(c, "DeprecationWarning")) return "Warning";
    if (wt_str_eq(c, "PendingDeprecationWarning")) return "Warning";
    if (wt_str_eq(c, "RuntimeWarning")) return "Warning";
    if (wt_str_eq(c, "SyntaxWarning")) return "Warning";
    if (wt_str_eq(c, "UserWarning")) return "Warning";
    if (wt_str_eq(c, "FutureWarning")) return "Warning";
    if (wt_str_eq(c, "ImportWarning")) return "Warning";
    if (wt_str_eq(c, "UnicodeWarning")) return "Warning";
    if (wt_str_eq(c, "BytesWarning")) return "Warning";
    if (wt_str_eq(c, "ResourceWarning")) return "Warning";
    if (wt_str_eq(c, "EncodingWarning")) return "Warning";
    return NULL; /* unknown / BaseException root */
}

/* True when cls is Exception or a subclass (walks to "Exception"); the
 * BaseException trio walks straight to BaseException and returns false. */
static bool wt_exc_is_exception(const char *cls) {
    const char *c = cls;
    for (int depth = 0; c && depth < 32; depth++) {
        if (wt_str_eq(c, "Exception")) return true;
        if (wt_str_eq(c, "BaseException")) return false;
        c = wt_exc_parent(c);
    }
    return false;
}
static bool wt_exc_is_typeerror_valueerror(const char *cls) {
    const char *c = cls;
    for (int depth = 0; c && depth < 32; depth++) {
        if (wt_str_eq(c, "TypeError") || wt_str_eq(c, "ValueError")) return true;
        if (wt_str_eq(c, "BaseException")) return false;
        c = wt_exc_parent(c);
    }
    return false;
}
/* Catch helpers: clear the pending exc when caught (return true); leave it
 * pending when it must propagate (return false). */
static bool wt_catch_exception(wt_ctx *ctx) {
    if (!wt_exc_pending(ctx)) return false;
    if (wt_exc_is_exception(wt_exc_cls(ctx))) {
        json_free(ctx->exc); ctx->exc = NULL; return true;
    }
    return false;
}
static bool wt_catch_typeerror_valueerror(wt_ctx *ctx) {
    if (!wt_exc_pending(ctx)) return false;
    if (wt_exc_is_typeerror_valueerror(wt_exc_cls(ctx))) {
        json_free(ctx->exc); ctx->exc = NULL; return true;
    }
    return false;
}

/* ── universal seam (fail-closed) ────────────────────────────────────────── */

/* args ownership TRANSFERRED (NULL -> empty object). Result is NEW. */
static json_t *wt_call(wt_ctx *ctx, const char *name, json_t *args) {
    if (!args) args = json_object();
    json_t *script = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    json_t *chosen = NULL;
    json_t *fc_marker = NULL;
    if (!script) {
        fc_marker = json_object();
        json_set(fc_marker, "__raise__", json_string("RuntimeError"));
        char msg[256];
        snprintf(msg, sizeof(msg), "no script for %s", name);
        json_set(fc_marker, "msg", json_string(msg));
        chosen = fc_marker;
    } else if (script->type == JSON_OBJECT) {
        json_t *seq = json_obj_get(script, "$seq");
        if (seq && seq->type == JSON_ARRAY) {
            double cur = json_get_num(ctx->counters, name, 0);
            size_t idx = (size_t)cur;
            if (idx >= seq->c.count) {
                fc_marker = json_object();
                json_set(fc_marker, "__raise__", json_string("RuntimeError"));
                char msg[256];
                snprintf(msg, sizeof(msg), "script exhausted: %s", name);
                json_set(fc_marker, "msg", json_string(msg));
                chosen = fc_marker;
            } else {
                json_set(ctx->counters, name, json_number(cur + 1));
                chosen = seq->c.items[idx];
            }
        } else {
            chosen = script;
        }
    } else {
        chosen = script;
    }
    json_t *ret = NULL;
    bool raises = false;
    if (fc_marker) {
        if (ctx->exc) json_free(ctx->exc);
        ctx->exc = json_copy(fc_marker);
        raises = true;
    } else if (chosen && chosen->type == JSON_OBJECT) {
        json_t *r = json_obj_get(chosen, "__raise__");
        if (r && r->type == JSON_STRING) {
            if (ctx->exc) json_free(ctx->exc);
            ctx->exc = json_copy(chosen);
            raises = true;
        } else {
            ret = json_copy(chosen);
        }
    } else if (chosen && chosen->type != JSON_NULL) {
        ret = json_copy(chosen);
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args);
    if (raises) json_set(ev, "ret", json_copy(ctx->exc));
    else json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(ctx->trace, ev);
    if (fc_marker) json_free(fc_marker);
    return ret;
}

/* Arg builders: None-valued keys dropped on BOTH sides ("" kept). */
static void wt_arg_str(json_t *args, const char *key, const char *val) {
    if (!val) return;
    json_set(args, key, json_string(val));
}
static void wt_arg_node(json_t *args, const char *key, const json_t *val) {
    if (!wt_is_none(val)) json_set(args, key, json_copy(val));
}

/* ── attribute-double accessors ──────────────────────────────────────────── */

static json_t *wt_getattr_def(json_t *obj, const char *key) {
    if (!wt_is_obj(obj)) return NULL;
    return json_obj_get(obj, key);
}
/* Direct obj.key read — non-object / absent raises AttributeError. Borrowed. */
static json_t *wt_dattr(wt_ctx *ctx, json_t *obj, const char *key) {
    if (!wt_is_obj(obj) || !json_has(obj, key)) {
        wt_raise(ctx, "AttributeError");
        return NULL;
    }
    return json_obj_get(obj, key);
}

/* ── ASCII string helpers ────────────────────────────────────────────────── */

static char *wt_dup_span(const char *s, size_t len) {
    char *r = (char *)malloc(len + 1);
    if (!r) return NULL;
    memcpy(r, s, len);
    r[len] = '\0';
    return r;
}
static bool wt_is_ws(char c) {
    unsigned char u = (unsigned char)c;
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r'
        || (u >= 0x1c && u <= 0x1f);
}
static char *wt_py_strip(const char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && wt_is_ws(s[a])) a++;
    while (b > a && wt_is_ws(s[b - 1])) b--;
    return wt_dup_span(s + a, b - a);
}
static char *wt_py_lower(const char *s) {
    char *r = wt_dup_span(s, strlen(s));
    if (!r) return NULL;
    for (char *p = r; *p; p++)
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    return r;
}

/* ── Python int()/float() (fixture domain: plain base-10 numeric strings) ── */

/* int(value): bool->0/1, number->trunc, str->base-10 parse, else TypeError.
 * Sets ctx->exc and returns false on raise; *out holds the value on success. */
static bool wt_py_int(wt_ctx *ctx, const json_t *v, long long *out) {
    if (wt_is_bool(v)) { *out = v->bool_val ? 1 : 0; return true; }
    if (wt_is_num(v)) { *out = (long long)trunc(v->num_val); return true; }
    if (wt_is_str(v)) {
        char *st = wt_py_strip(wt_jstr(v));
        const char *p = st;
        bool neg = false;
        if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
        if (!*p) { free(st); wt_raise(ctx, "ValueError"); return false; }
        long long acc = 0;
        for (; *p; p++) {
            if (*p < '0' || *p > '9') { free(st); wt_raise(ctx, "ValueError"); return false; }
            acc = acc * 10 + (*p - '0');
        }
        free(st);
        *out = neg ? -acc : acc;
        return true;
    }
    wt_raise(ctx, "TypeError");
    return false;
}
/* float(value): bool->0/1, number->val, str->strtod (finite, full consume). */
static bool wt_py_float(wt_ctx *ctx, const json_t *v, double *out) {
    if (wt_is_bool(v)) { *out = v->bool_val ? 1.0 : 0.0; return true; }
    if (wt_is_num(v)) { *out = v->num_val; return true; }
    if (wt_is_str(v)) {
        char *st = wt_py_strip(wt_jstr(v));
        if (!*st) { free(st); wt_raise(ctx, "ValueError"); return false; }
        char *end = NULL;
        double d = strtod(st, &end);
        if (end == st || *end != '\0' || !isfinite(d)) {
            free(st); wt_raise(ctx, "ValueError"); return false;
        }
        free(st);
        *out = d;
        return true;
    }
    wt_raise(ctx, "TypeError");
    return false;
}

/* ── Python json.dumps COMPACT (separators ", " / ": ", ASCII escaping) ──── */

typedef struct { char *buf; size_t len, cap; bool oom; } wt_ser;
static void wt_ser_putc(wt_ser *s, char c) {
    if (s->oom) return;
    if (s->len + 1 >= s->cap) {
        s->cap = s->cap ? s->cap * 2 : 128;
        char *nb = (char *)realloc(s->buf, s->cap);
        if (!nb) { s->oom = true; return; }
        s->buf = nb;
    }
    s->buf[s->len++] = c;
}
static void wt_ser_puts(wt_ser *s, const char *str) {
    if (s->oom || !str) return;
    while (*str) wt_ser_putc(s, *str++);
}
static void wt_ser_escaped(wt_ser *s, const char *str) {
    wt_ser_putc(s, '"');
    if (str) for (const char *p = str; *p; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"': wt_ser_puts(s, "\\\""); break;
            case '\\': wt_ser_puts(s, "\\\\"); break;
            case '\b': wt_ser_puts(s, "\\b"); break;
            case '\f': wt_ser_puts(s, "\\f"); break;
            case '\n': wt_ser_puts(s, "\\n"); break;
            case '\r': wt_ser_puts(s, "\\r"); break;
            case '\t': wt_ser_puts(s, "\\t"); break;
            default:
                if (c < 32) { char e[8]; snprintf(e, sizeof(e), "\\u%04x", c); wt_ser_puts(s, e); }
                else wt_ser_putc(s, (char)c);
                break;
        }
    }
    wt_ser_putc(s, '"');
}
static void wt_ser_node(wt_ser *s, const json_t *n) {
    if (!n) { wt_ser_puts(s, "null"); return; }
    switch (n->type) {
        case JSON_NULL: wt_ser_puts(s, "null"); break;
        case JSON_BOOL: wt_ser_puts(s, n->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: {
            char tmp[64];
            if (n->num_val == (double)(long long)n->num_val)
                snprintf(tmp, sizeof(tmp), "%.0f", n->num_val);
            else snprintf(tmp, sizeof(tmp), "%.17g", n->num_val);
            wt_ser_puts(s, tmp);
            break;
        }
        case JSON_STRING: wt_ser_escaped(s, n->str_val); break;
        case JSON_ARRAY:
            wt_ser_putc(s, '[');
            for (size_t i = 0; i < n->c.count; i++) {
                if (i > 0) wt_ser_puts(s, ", ");
                wt_ser_node(s, n->c.items[i]);
            }
            wt_ser_putc(s, ']');
            break;
        case JSON_OBJECT:
            wt_ser_putc(s, '{');
            for (size_t i = 0; i < n->c.count; i++) {
                if (i > 0) wt_ser_puts(s, ", ");
                wt_ser_escaped(s, n->c.keys[i]);
                wt_ser_puts(s, ": ");
                wt_ser_node(s, n->c.items[i]);
            }
            wt_ser_putc(s, '}');
            break;
    }
}
/* Python json.dumps(node) compact, ensure_ascii irrelevant in ASCII domain. */
static char *wt_dumps_compact(const json_t *node) {
    wt_ser s = {NULL, 0, 0, false};
    wt_ser_node(&s, node);
    wt_ser_putc(&s, '\0');
    if (s.oom) { free(s.buf); return NULL; }
    return s.buf ? s.buf : wt_dup_span("null", 4);
}

/* ── live constant tables ────────────────────────────────────────────────── */

#define WT_NOUS_MANAGED_PROVIDER "nous"
#define WT_DEFAULT_EXTRACT_CHAR_LIMIT 15000
#define WT_CHAR_LIMIT_FLOOR 2000
#define WT_CHAR_LIMIT_CEILING 500000
#define WT_DEFAULT_EXTRACT_TIMEOUT_S 120.0
#define WT_NO_RESULT_ERROR "Extract backend returned no result for this URL"
#define WT_EXTRACT_BACKENDS_HINT "firecrawl, tavily, keenable, exa, or parallel."

static bool wt_in_legacy_web_backends(const char *s) {
    static const char *T[] = {
        "exa", "parallel", "keenable", "firecrawl", "tavily",
        "perplexity", "searxng", "brave-free", "ddgs", "xai",
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (wt_str_eq(T[i], s)) return true;
    return false;
}
static const char *wt_ring_key_var(const char *name) {
    if (wt_str_eq(name, "exa")) return "EXA_API_KEY";
    if (wt_str_eq(name, "parallel")) return "PARALLEL_API_KEY";
    if (wt_str_eq(name, "firecrawl")) return "FIRECRAWL_API_KEY";
    if (wt_str_eq(name, "keenable")) return "KEENABLE_API_KEY";
    return "";
}

/* provider.<method> seam call (no args). Returns NEW result or NULL on raise. */
static json_t *wt_provider_method(wt_ctx *ctx, const char *method) {
    char name[128];
    snprintf(name, sizeof(name), "provider.%s", method);
    return wt_call(ctx, name, json_object());
}

/* ── web_tools.py: _configured_backend ───────────────────────────────────── */

/* PoP: wt_configured_backend @ tools/web_tools.py:_configured_backend */
static json_t *wt_configured_backend(wt_ctx *ctx, const char *key) {
    json_t *cfg = wt_call(ctx, "_load_web_config", json_object());
    if (wt_exc_pending(ctx)) { if (cfg) json_free(cfg); return NULL; }
    if (!wt_is_obj(cfg)) { wt_raise(ctx, "AttributeError"); json_free(cfg); return NULL; }
    /* (_load_web_config().get(key) or "").lower().strip() — a truthy non-str
     * value reaches .lower() and raises AttributeError. */
    json_t *v = json_obj_get(cfg, key);
    const char *s = "";
    if (wt_truthy(v)) {
        if (!wt_is_str(v)) { wt_raise(ctx, "AttributeError"); json_free(cfg); return NULL; }
        s = wt_jstr(v);
    }
    char *lo = wt_py_lower(s);
    char *st = wt_py_strip(lo ? lo : "");
    free(lo);
    json_t *ret = json_string(st ? st : "");
    free(st);
    json_free(cfg);
    return ret;
}

/* ── web_tools.py: _registry_call ────────────────────────────────────────── */

/* PoP: wt_registry_call @ tools/web_tools.py:_registry_call
 * func_name in {get_provider, list_providers}; args is the positional *args
 * array. except Exception -> default (BaseException trio propagates). */
static json_t *wt_registry_call(wt_ctx *ctx, const char *func_name,
                                json_t *def, json_t *args_arr) {
    json_t *a = json_object();
    if (wt_str_eq(func_name, "get_provider")) {
        json_t *backend = wt_is_arr(args_arr) && args_arr->c.count > 0 ? args_arr->c.items[0] : NULL;
        wt_arg_node(a, "backend", backend);
    } else if (wt_str_eq(func_name, "list_providers")) {
        /* no args */
    } else {
        wt_arg_node(a, "args", args_arr);
    }
    json_t *ret = wt_call(ctx, func_name, a);
    if (wt_exc_pending(ctx)) {
        if (wt_catch_exception(ctx)) { if (ret) json_free(ret); return def ? json_copy(def) : json_null(); }
        if (ret) json_free(ret);
        return NULL;
    }
    return ret ? ret : json_null();
}

/* ── web_tools.py: _probe ────────────────────────────────────────────────── */

/* PoP: wt_probe @ tools/web_tools.py:_probe
 * bool(getattr(provider, method)()); except Exception -> None. A non-object
 * provider raises AttributeError on getattr (no seam fires) -> caught -> None. */
static json_t *wt_probe(wt_ctx *ctx, json_t *provider, const char *method) {
    if (!wt_is_obj(provider)) {
        /* getattr(provider, method) raises AttributeError; caught below. */
        wt_raise(ctx, "AttributeError");
        if (wt_catch_exception(ctx)) return json_null();
        return NULL;
    }
    json_t *r = wt_provider_method(ctx, method);
    if (wt_exc_pending(ctx)) {
        if (wt_catch_exception(ctx)) { if (r) json_free(r); return json_null(); }
        if (r) json_free(r);
        return NULL;
    }
    bool b = wt_truthy(r);
    if (r) json_free(r);
    return json_bool(b);
}

/* ── web_tools.py: _autodetect_backend ───────────────────────────────────── */

/* _has_env(name) seam -> bool. */
static bool wt_has_env(wt_ctx *ctx, const char *name, bool *ok) {
    json_t *a = json_object();
    wt_arg_str(a, "name", name);
    json_t *r = wt_call(ctx, "_has_env", a);
    if (wt_exc_pending(ctx)) { if (r) json_free(r); *ok = false; return false; }
    bool b = wt_truthy(r);
    if (r) json_free(r);
    *ok = true;
    return b;
}

/* PoP: wt_autodetect_backend @ tools/web_tools.py:_autodetect_backend
 * The candidate tuple is built EAGERLY (every probe fires in source order,
 * with the FIRECRAWL_API_KEY or FIRECRAWL_API_URL short-circuit), then the
 * first available rung wins; else the plugin-provider walk. */
static json_t *wt_autodetect_backend(wt_ctx *ctx) {
    bool ok;
    /* ("tavily", _has_env("TAVILY_API_KEY")) */
    bool tavily = wt_has_env(ctx, "TAVILY_API_KEY", &ok); if (!ok) return NULL;
    bool perplexity = wt_has_env(ctx, "PERPLEXITY_API_KEY", &ok); if (!ok) return NULL;
    bool exa = wt_has_env(ctx, "EXA_API_KEY", &ok); if (!ok) return NULL;
    bool parallel = wt_has_env(ctx, "PARALLEL_API_KEY", &ok); if (!ok) return NULL;
    bool keenable = wt_has_env(ctx, "KEENABLE_API_KEY", &ok); if (!ok) return NULL;
    /* ("firecrawl", _has_env(KEY) or _has_env(URL)) */
    bool fc_key = wt_has_env(ctx, "FIRECRAWL_API_KEY", &ok); if (!ok) return NULL;
    bool firecrawl_env = fc_key;
    if (!fc_key) { firecrawl_env = wt_has_env(ctx, "FIRECRAWL_API_URL", &ok); if (!ok) return NULL; }
    /* ("firecrawl", _is_tool_gateway_ready()) */
    json_t *gw = wt_call(ctx, "_is_tool_gateway_ready", json_object());
    if (wt_exc_pending(ctx)) { if (gw) json_free(gw); return NULL; }
    bool firecrawl_gw = wt_truthy(gw); if (gw) json_free(gw);
    bool searxng = wt_has_env(ctx, "SEARXNG_URL", &ok); if (!ok) return NULL;
    bool brave = wt_has_env(ctx, "BRAVE_SEARCH_API_KEY", &ok); if (!ok) return NULL;
    /* ("ddgs", _ddgs_package_importable()) */
    json_t *dd = wt_call(ctx, "_ddgs_package_importable", json_object());
    if (wt_exc_pending(ctx)) { if (dd) json_free(dd); return NULL; }
    bool ddgs = wt_truthy(dd); if (dd) json_free(dd);

    const char *names[] = {"tavily","perplexity","exa","parallel","keenable",
                           "firecrawl","firecrawl","searxng","brave-free","ddgs"};
    bool avail[] = {tavily,perplexity,exa,parallel,keenable,
                    firecrawl_env,firecrawl_gw,searxng,brave,ddgs};
    for (int i = 0; i < 10; i++)
        if (avail[i]) return json_string(names[i]);

    /* Plugin-contributed providers. */
    json_t *providers = wt_call(ctx, "_list_registered_web_providers", json_object());
    if (wt_exc_pending(ctx)) { if (providers) json_free(providers); return NULL; }
    if (wt_is_arr(providers)) {
        for (size_t i = 0; i < providers->c.count; i++) {
            json_t *p = providers->c.items[i];
            json_t *nm = wt_dattr(ctx, p, "name");
            if (wt_exc_pending(ctx)) { json_free(providers); return NULL; }
            if (!wt_in_legacy_web_backends(wt_jstr(nm))) {
                json_t *pr = wt_probe(ctx, p, "is_available");
                if (wt_exc_pending(ctx)) { if (pr) json_free(pr); json_free(providers); return NULL; }
                bool ready = wt_truthy(pr); if (pr) json_free(pr);
                if (ready) {
                    json_t *ret = json_string(wt_jstr(nm));
                    json_free(providers);
                    return ret;
                }
            }
        }
    }
    json_free(providers);
    return json_null();
}

/* ── web_tools.py: _keyless_backend ──────────────────────────────────────── */

/* PoP: wt_keyless_backend @ tools/web_tools.py:_keyless_backend
 * Whole body in try/except Exception -> None. */
static json_t *wt_keyless_backend(wt_ctx *ctx) {
    json_t *r = wt_call(ctx, "_ensure_web_plugins_loaded", json_object());
    if (r) json_free(r);
    if (wt_exc_pending(ctx)) { if (wt_catch_exception(ctx)) return json_null(); return NULL; }

    json_t *tier = wt_call(ctx, "_keyless_tier_enabled", json_object());
    if (wt_exc_pending(ctx)) { if (tier) json_free(tier); if (wt_catch_exception(ctx)) return json_null(); return NULL; }
    bool enabled = wt_truthy(tier); if (tier) json_free(tier);
    if (enabled) {
        json_t *pref = wt_call(ctx, "_keyless_preference", json_object());
        if (wt_exc_pending(ctx)) { if (pref) json_free(pref); if (wt_catch_exception(ctx)) return json_null(); return NULL; }
        if (wt_is_arr(pref)) {
            for (size_t i = 0; i < pref->c.count; i++) {
                const char *name = wt_jstr(pref->c.items[i]);
                json_t *a = json_object();
                wt_arg_str(a, "name", name);
                json_t *provider = wt_call(ctx, "_registered_web_provider", a);
                if (wt_exc_pending(ctx)) { if (provider) json_free(provider); json_free(pref); if (wt_catch_exception(ctx)) return json_null(); return NULL; }
                if (!wt_is_none(provider)) {
                    json_t *pr = wt_probe(ctx, provider, "is_keyless_available");
                    if (wt_exc_pending(ctx)) { if (pr) json_free(pr); json_free(provider); json_free(pref); if (wt_catch_exception(ctx)) return json_null(); return NULL; }
                    bool ready = wt_truthy(pr); if (pr) json_free(pr);
                    if (ready) {
                        json_t *ret = json_string(name);
                        json_free(provider); json_free(pref);
                        return ret;
                    }
                }
                if (provider) json_free(provider);
            }
        }
        json_free(pref);
    }
    return json_null();
}

/* ── web_tools.py: _managed_web_search ───────────────────────────────────── */

/* PoP: wt_managed_web_search @ tools/web_tools.py:_managed_web_search */
static json_t *wt_managed_web_search(wt_ctx *ctx) {
    json_t *pin = wt_configured_backend(ctx, "search_backend");
    if (wt_exc_pending(ctx)) { if (pin) json_free(pin); return NULL; }
    if (wt_truthy(pin)) {
        bool b = wt_str_eq(wt_jstr(pin), WT_NOUS_MANAGED_PROVIDER);
        json_free(pin);
        return json_bool(b);
    }
    json_free(pin);

    json_t *sa = json_object();
    wt_arg_str(sa, "section", "web");
    json_t *selected = wt_call(ctx, "read_selection", sa);
    if (wt_exc_pending(ctx)) { if (selected) json_free(selected); return NULL; }
    if (!wt_is_none(selected)) {
        bool b = wt_str_eq(wt_jstr(selected), WT_NOUS_MANAGED_PROVIDER);
        json_free(selected);
        return json_bool(b);
    }
    json_free(selected);

    json_t *backend = wt_autodetect_backend(ctx);
    if (wt_exc_pending(ctx)) { if (backend) json_free(backend); return NULL; }
    if (wt_is_str(backend) && wt_str_eq(wt_jstr(backend), "firecrawl")) {
        /* not (_has_env(KEY) or _has_env(URL)) and _is_tool_gateway_ready() */
        bool ok;
        bool k = wt_has_env(ctx, "FIRECRAWL_API_KEY", &ok);
        if (!ok) { json_free(backend); return NULL; }
        bool env = k;
        if (!k) { env = wt_has_env(ctx, "FIRECRAWL_API_URL", &ok); if (!ok) { json_free(backend); return NULL; } }
        if (env) { json_free(backend); return json_bool(false); }  /* not True -> False, short-circuit */
        json_t *gw = wt_call(ctx, "_is_tool_gateway_ready", json_object());
        if (wt_exc_pending(ctx)) { if (gw) json_free(gw); json_free(backend); return NULL; }
        json_free(backend);
        return gw ? gw : json_null();   /* True and gw -> gw RAW */
    }
    /* backend is None and resolve_free_search_gateway(token_reader=peek) is not None */
    bool is_none = wt_is_none(backend);
    json_free(backend);
    if (!is_none) return json_bool(false);  /* short-circuit: resolve not called */
    json_t *ra = json_object();
    wt_arg_str(ra, "token_reader", "peek_nous_access_token");
    json_t *res = wt_call(ctx, "resolve_free_search_gateway", ra);
    if (wt_exc_pending(ctx)) { if (res) json_free(res); return NULL; }
    bool b = !wt_is_none(res);
    if (res) json_free(res);
    return json_bool(b);
}

/* ── web_tools.py: _xai_available ────────────────────────────────────────── */

/* PoP: wt_xai_available @ tools/web_tools.py:_xai_available */
static json_t *wt_xai_available(wt_ctx *ctx) {
    json_t *r = wt_call(ctx, "has_xai_credentials", json_object());
    if (wt_exc_pending(ctx)) {
        if (wt_catch_exception(ctx)) { if (r) json_free(r); return json_bool(false); }
        if (r) json_free(r);
        return NULL;
    }
    return r ? r : json_null();   /* return has_xai_credentials() RAW (no bool wrap) */
}

/* ── web_tools.py: _finish_debug ─────────────────────────────────────────── */

/* PoP: wt_finish_debug @ tools/web_tools.py:_finish_debug
 * Mutates debug_call_data["error"] when error_msg is not None; echoes the
 * debug_call_data window. Seams: debug_log_call, debug_save, tool_error. */
static json_t *wt_finish_debug(wt_ctx *ctx, const char *call_name,
                               json_t *debug_call_data, json_t *error_msg) {
    if (!wt_is_none(error_msg)) {
        /* logger.debug (no-op); debug_call_data["error"] = error_msg */
        if (wt_is_obj(debug_call_data))
            json_set(debug_call_data, "error", json_copy(error_msg));
    }
    json_t *a = json_object();
    wt_arg_str(a, "call_name", call_name);
    wt_arg_node(a, "debug_call_data", debug_call_data);
    json_t *r1 = wt_call(ctx, "debug_log_call", a);
    if (r1) json_free(r1);
    if (wt_exc_pending(ctx)) return NULL;

    json_t *r2 = wt_call(ctx, "debug_save", json_object());
    if (r2) json_free(r2);
    if (wt_exc_pending(ctx)) return NULL;

    if (wt_is_none(error_msg)) return json_null();
    json_t *ta = json_object();
    wt_arg_str(ta, "message", wt_jstr(error_msg));
    json_t *r3 = wt_call(ctx, "tool_error", ta);
    if (wt_exc_pending(ctx)) { if (r3) json_free(r3); return NULL; }
    return r3 ? r3 : json_null();
}

/* ── web_tools_rescue.py: _ring_vendor_keyless ───────────────────────────── */

/* PoP: wt_ring_vendor_keyless @ tools/web_tools_rescue.py:_ring_vendor_keyless */
static json_t *wt_ring_vendor_keyless(wt_ctx *ctx, const char *name) {
    if (wt_str_eq(name, "firecrawl")) {
        json_t *r = wt_call(ctx, "_use_keyless_ring", json_object());
        if (wt_exc_pending(ctx)) { if (r) json_free(r); return NULL; }
        return r ? r : json_null();   /* RAW return */
    }
    const char *key_var = wt_ring_key_var(name);
    /* use_keyless(name, get_provider_env(key_var) if key_var else "") */
    char *key = wt_dup_span("", 0);
    if (key_var[0]) {
        json_t *ga = json_object();
        wt_arg_str(ga, "var", key_var);
        json_t *gv = wt_call(ctx, "get_provider_env", ga);
        if (wt_exc_pending(ctx)) { free(key); if (gv) json_free(gv); return NULL; }
        free(key);
        key = wt_dup_span(wt_jstr(gv), strlen(wt_jstr(gv)));
        if (gv) json_free(gv);
    }
    json_t *ua = json_object();
    wt_arg_str(ua, "name", name);
    wt_arg_str(ua, "key", key ? key : "");
    free(key);
    json_t *r = wt_call(ctx, "use_keyless", ua);
    if (wt_exc_pending(ctx)) { if (r) json_free(r); return NULL; }
    return r ? r : json_null();   /* RAW return */
}

/* ── web_tools_rescue.py: _managed_search_fallback ───────────────────────── */

/* forward decl (defined below; called by nested _served_after_failure) */
static json_t *wt_managed_search_fallback(wt_ctx *ctx, json_t *provider,
                                          const char *original_error,
                                          const char *query, double limit);

/* PoP: wt_managed_search_fallback @ tools/web_tools_rescue.py:_managed_search_fallback
 * Guard or-chain short-circuits; get_provider("firecrawl").search in
 * try/except Exception; resp.setdefault("data",{}).update(...) on success. */
static json_t *wt_managed_search_fallback(wt_ctx *ctx, json_t *provider,
                                          const char *original_error,
                                          const char *query, double limit) {
    /* getattr(provider, "name", "") != "perplexity" */
    json_t *nm = wt_getattr_def(provider, "name");
    const char *name = wt_is_str(nm) ? wt_jstr(nm) : "";
    if (!wt_str_eq(name, "perplexity")) return json_null();

    json_t *ga = json_object();
    wt_arg_str(ga, "var", "PERPLEXITY_API_KEY");
    json_t *env = wt_call(ctx, "get_provider_env", ga);
    if (wt_exc_pending(ctx)) { if (env) json_free(env); return NULL; }
    bool has_key = wt_truthy(env); if (env) json_free(env);
    if (has_key) return json_null();

    json_t *managed = wt_managed_web_search(ctx);   /* _wt._managed_web_search() */
    if (wt_exc_pending(ctx)) { if (managed) json_free(managed); return NULL; }
    bool is_managed = wt_truthy(managed); if (managed) json_free(managed);
    if (!is_managed) return json_null();

    json_t *gw = wt_call(ctx, "_is_tool_gateway_ready", json_object());  /* _wt._is_tool_gateway_ready() */
    if (wt_exc_pending(ctx)) { if (gw) json_free(gw); return NULL; }
    bool gw_ready = wt_truthy(gw); if (gw) json_free(gw);
    if (!gw_ready) return json_null();

    /* try: resp = get_provider("firecrawl").search(query, limit) */
    json_t *pa = json_object();
    wt_arg_str(pa, "backend", "firecrawl");
    json_t *fc = wt_call(ctx, "get_provider", pa);
    json_t *resp = NULL;
    if (wt_exc_pending(ctx)) {
        if (wt_catch_exception(ctx)) {
            resp = json_object();
            json_set(resp, "success", json_bool(false));
            json_set(resp, "error", json_string(wt_exc_msg(ctx)));
            /* exc cleared by catch; fall through to resp handling */
        } else { if (fc) json_free(fc); return NULL; }
    } else if (wt_is_none(fc)) {
        /* None.search -> AttributeError (caught). Out of str(exc) domain; the
         * receiver shape is excluded from fixtures, modeled for completeness. */
        if (fc) json_free(fc);
        wt_raise(ctx, "AttributeError");
        if (wt_catch_exception(ctx)) {
            resp = json_object();
            json_set(resp, "success", json_bool(false));
            json_set(resp, "error", json_string(wt_exc_msg(ctx)));
        } else return NULL;
    } else {
        json_t *sargs = json_object();
        wt_arg_str(sargs, "query", query);
        json_set(sargs, "limit", json_number(limit));
        resp = wt_call(ctx, "provider.search", sargs);
        if (fc) json_free(fc);
        if (wt_exc_pending(ctx)) {
            if (wt_catch_exception(ctx)) {
                if (resp) json_free(resp);
                resp = json_object();
                json_set(resp, "success", json_bool(false));
                json_set(resp, "error", json_string(wt_exc_msg(ctx)));
            } else { if (resp) json_free(resp); return NULL; }
        }
    }

    /* if not resp.get("success"): return None */
    json_t *succ = wt_is_obj(resp) ? json_obj_get(resp, "success") : NULL;
    if (!wt_truthy(succ)) { if (resp) json_free(resp); return json_null(); }

    /* resp.setdefault("data", {}).update(fallback_from=..., backend_error=...) */
    if (wt_is_obj(resp)) {
        json_t *data = json_obj_get(resp, "data");
        if (!wt_is_obj(data)) {
            data = json_object();
            json_set(resp, "data", data);
        }
        json_set(data, "fallback_from", json_string("managed_primary"));
        const char *oe = (original_error && original_error[0]) ? original_error : "unknown error";
        char oe300[301];
        snprintf(oe300, sizeof(oe300), "%.300s", oe);
        char be[512];
        snprintf(be, sizeof(be),
                 "Primary managed search failed this call (%s); result served by the "
                 "managed fallback. The next call will use the primary again.", oe300);
        json_set(data, "backend_error", json_string(be));
    }
    return resp ? resp : json_null();
}

/* ── web_tools_rescue.py: _rescue_search ─────────────────────────────────── */

/* PoP: wt_rescue_search @ tools/web_tools_rescue.py:_rescue_search */
static json_t *wt_rescue_search(wt_ctx *ctx, const char *provider_name,
                                const char *original_error, const char *query, double limit) {
    json_t *a = json_object();
    wt_arg_str(a, "provider_name", provider_name);
    wt_arg_str(a, "query", query);
    json_set(a, "limit", json_number(limit));
    json_t *rescued = wt_call(ctx, "search_with_failover", a);
    if (wt_exc_pending(ctx)) { if (rescued) json_free(rescued); return NULL; }

    json_t *succ = wt_is_obj(rescued) ? json_obj_get(rescued, "success") : NULL;
    if (wt_truthy(succ)) {
        if (wt_is_obj(rescued)) {
            json_t *data = json_obj_get(rescued, "data");
            if (!wt_is_obj(data)) { data = json_object(); json_set(rescued, "data", data); }
            json_set(data, "rescued_from", json_string(provider_name));
            const char *oe = (original_error && original_error[0]) ? original_error : "unknown error";
            char oe300[301]; snprintf(oe300, sizeof(oe300), "%.300s", oe);
            char be[600];
            snprintf(be, sizeof(be),
                     "Configured backend '%s' failed this call (%s); result served by the "
                     "keyless free tier. The next call will use '%s' again.",
                     provider_name, oe300, provider_name);
            json_set(data, "backend_error", json_string(be));
        }
        return rescued ? rescued : json_null();
    }
    /* ring also failed: lead with the ORIGINAL error. Read rescued.get("error",
     * "unknown") BEFORE freeing rescued. */
    const char *oe = (original_error && original_error[0]) ? original_error : "search failed";
    const char *rerr_s = "unknown";
    if (wt_is_obj(rescued) && json_has(rescued, "error")) {
        json_t *e = json_obj_get(rescued, "error");
        if (wt_is_str(e)) rerr_s = wt_jstr(e);
        else if (wt_is_none(e)) rerr_s = "None";
    }
    if (rescued) json_free(rescued);
    json_t *out = json_object();
    json_set(out, "success", json_bool(false));
    char err[700];
    snprintf(err, sizeof(err), "%s (keyless rescue also failed: %s)", oe, rerr_s);
    json_set(out, "error", json_string(err));
    return out;
}

/* ── web_tools_rescue.py: _rescue_extract ────────────────────────────────── */

/* _policy_blocked_result(result) claimed seam -> bool. */
static bool wt_policy_blocked(wt_ctx *ctx, json_t *result, bool *ok) {
    json_t *a = json_object();
    wt_arg_node(a, "result", result);
    json_t *r = wt_call(ctx, "_policy_blocked_result", a);
    if (wt_exc_pending(ctx)) { if (r) json_free(r); *ok = false; return false; }
    bool b = wt_truthy(r); if (r) json_free(r);
    *ok = true;
    return b;
}

/* PoP: wt_rescue_extract @ tools/web_tools_rescue.py:_rescue_extract */
static json_t *wt_rescue_extract(wt_ctx *ctx, const char *provider_name,
                                 json_t *urls, json_t *results) {
    size_t nurls = wt_is_arr(urls) ? urls->c.count : 0;
    size_t nres = wt_is_arr(results) ? results->c.count : 0;
    bool parity = (nres == nurls);

    /* rescue_idx = [i for i,r in enumerate(results) if not parity or not _policy_blocked_result(r)] */
    size_t *rescue_idx = (size_t *)malloc((nres ? nres : 1) * sizeof(size_t));
    size_t nidx = 0;
    for (size_t i = 0; i < nres; i++) {
        bool include;
        if (!parity) include = true;
        else {
            bool ok;
            bool blocked = wt_policy_blocked(ctx, results->c.items[i], &ok);
            if (!ok) { free(rescue_idx); return NULL; }
            include = !blocked;
        }
        if (include) rescue_idx[nidx++] = i;
    }
    if (nidx == 0) { free(rescue_idx); return json_copy(results); }

    /* rescue_urls = [urls[i] for i in rescue_idx] if parity else list(urls) */
    json_t *rescue_urls = json_array();
    if (parity) {
        for (size_t k = 0; k < nidx; k++)
            json_append(rescue_urls, json_copy(urls->c.items[rescue_idx[k]]));
    } else {
        for (size_t i = 0; i < nurls; i++)
            json_append(rescue_urls, json_copy(urls->c.items[i]));
    }

    /* original_error = next((results[i].get("error") for i in rescue_idx if results[i].get("error")), "extract failed") */
    const char *original_error = "extract failed";
    for (size_t k = 0; k < nidx; k++) {
        json_t *e = wt_is_obj(results->c.items[rescue_idx[k]])
                        ? json_obj_get(results->c.items[rescue_idx[k]], "error") : NULL;
        if (wt_truthy(e)) { original_error = wt_jstr(e); break; }
    }

    json_t *ea = json_object();
    wt_arg_str(ea, "provider_name", provider_name);
    wt_arg_node(ea, "urls", rescue_urls);
    json_free(rescue_urls);
    json_t *rescued = wt_call(ctx, "extract_with_failover", ea);
    if (wt_exc_pending(ctx)) { free(rescue_idx); if (rescued) json_free(rescued); return NULL; }

    /* if rescued and all(r.get("error","") for r in rescued): return results */
    if (wt_truthy(rescued) && wt_is_arr(rescued)) {
        bool all_err = true;
        for (size_t i = 0; i < rescued->c.count; i++) {
            json_t *r = rescued->c.items[i];
            json_t *e = wt_is_obj(r) ? json_obj_get(r, "error") : NULL;
            /* r.get("error", ""): absent -> "" (falsy); present -> truthiness */
            if (!wt_truthy(e)) { all_err = false; break; }
        }
        if (all_err) { free(rescue_idx); json_free(rescued); return json_copy(results); }
    }

    /* for r in rescued: meta = None if r.get("error") else r.setdefault("metadata",{}); ... */
    if (wt_is_arr(rescued)) {
        for (size_t i = 0; i < rescued->c.count; i++) {
            json_t *r = rescued->c.items[i];
            json_t *e = wt_is_obj(r) ? json_obj_get(r, "error") : NULL;
            if (wt_truthy(e)) continue;  /* meta = None */
            if (!wt_is_obj(r)) continue;
            json_t *meta = json_obj_get(r, "metadata");
            if (!wt_is_obj(meta)) { meta = json_object(); json_set(r, "metadata", meta); }
            json_set(meta, "rescued_from", json_string(provider_name));
            char oe300[301]; snprintf(oe300, sizeof(oe300), "%.300s", original_error);
            json_set(meta, "backend_error", json_string(oe300));
        }
    }

    /* if parity and len(rescued) == len(rescue_idx): replacements merge */
    size_t nresc = wt_is_arr(rescued) ? rescued->c.count : 0;
    if (parity && nresc == nidx) {
        json_t *out = json_array();
        for (size_t i = 0; i < nres; i++) {
            json_t *rep = NULL;
            for (size_t k = 0; k < nidx; k++)
                if (rescue_idx[k] == i) { rep = rescued->c.items[k]; break; }
            json_append(out, rep ? json_copy(rep) : json_copy(results->c.items[i]));
        }
        free(rescue_idx); json_free(rescued);
        return out;
    }
    free(rescue_idx);
    return rescued ? rescued : json_null();
}

/* ── web_tools_truncate.py: _clamp_char_limit / _clamp_or_default ────────── */

/* PoP: wt_clamp_char_limit @ tools/web_tools_truncate.py:_clamp_char_limit */
static json_t *wt_clamp_char_limit(wt_ctx *ctx, json_t *value) {
    long long iv;
    if (!wt_py_int(ctx, value, &iv)) return NULL;
    if (iv < WT_CHAR_LIMIT_FLOOR) iv = WT_CHAR_LIMIT_FLOOR;
    if (iv > WT_CHAR_LIMIT_CEILING) iv = WT_CHAR_LIMIT_CEILING;
    return json_number((double)iv);
}

/* PoP: wt_clamp_or_default @ tools/web_tools_truncate.py:_clamp_or_default */
static json_t *wt_clamp_or_default(wt_ctx *ctx, json_t *value) {
    if (wt_is_none(value)) return json_number(WT_DEFAULT_EXTRACT_CHAR_LIMIT);
    json_t *r = wt_clamp_char_limit(ctx, value);
    if (wt_exc_pending(ctx)) {
        if (wt_catch_typeerror_valueerror(ctx)) { if (r) json_free(r); return json_number(WT_DEFAULT_EXTRACT_CHAR_LIMIT); }
        if (r) json_free(r);
        return NULL;
    }
    return r;
}

/* PoP: wt_effective_char_limit @ tools/web_tools_truncate.py:_effective_char_limit */
static json_t *wt_effective_char_limit(wt_ctx *ctx, json_t *char_limit) {
    if (!wt_is_none(char_limit)) return wt_clamp_or_default(ctx, char_limit);
    json_t *r = wt_call(ctx, "_get_extract_char_limit", json_object());
    if (wt_exc_pending(ctx)) { if (r) json_free(r); return NULL; }
    return r ? r : json_null();
}

/* PoP: wt_trim_results @ tools/web_tools_truncate.py:_trim_results */
static json_t *wt_trim_results(json_t *results) {
    json_t *out = json_array();
    if (!wt_is_arr(results)) return out;
    for (size_t i = 0; i < results->c.count; i++) {
        json_t *r = results->c.items[i];
        json_t *e = json_object();
        json_set(e, "url", json_string(wt_jstr(wt_getattr_def(r, "url"))));
        json_set(e, "title", json_string(wt_jstr(wt_getattr_def(r, "title"))));
        json_set(e, "content", json_string(wt_jstr(wt_getattr_def(r, "content"))));
        json_t *err = wt_getattr_def(r, "error");
        json_set(e, "error", err ? json_copy(err) : json_null());
        if (wt_is_obj(r) && json_has(r, "blocked_by_policy"))
            json_set(e, "blocked_by_policy", json_copy(json_obj_get(r, "blocked_by_policy")));
        json_append(out, e);
    }
    return out;
}

/* ── web_tools_extract.py: pure error / entry leaves ─────────────────────── */

/* PoP: wt_disabled_plugin_error @ tools/web_tools_extract.py:_disabled_plugin_error */
static json_t *wt_disabled_plugin_error(const char *capability, const char *disabled_key) {
    /* vendor = disabled_key.split("/", 1)[-1] */
    const char *slash = strchr(disabled_key, '/');
    const char *vendor = slash ? slash + 1 : disabled_key;
    char buf[1024];
    snprintf(buf, sizeof(buf),
             "web.%s_backend is set to '%s', but its plugin ('%s') is disabled in config. "
             "Re-enable it with `hermes plugins enable %s` (or remove it from plugins.disabled).",
             capability, vendor, disabled_key, disabled_key);
    return json_string(buf);
}

/* PoP: wt_result_entry @ tools/web_tools_extract.py:_result_entry */
static json_t *wt_result_entry(const char *url, json_t *error) {
    json_t *e = json_object();
    json_set(e, "url", json_string(url ? url : ""));
    json_set(e, "title", json_string(""));
    json_set(e, "content", json_string(""));
    json_set(e, "error", error ? json_copy(error) : json_null());
    return e;
}

/* {"success": False, "error": error} compact dumps node. */
static json_t *wt_success_error_node(const char *error) {
    json_t *o = json_object();
    json_set(o, "success", json_bool(false));
    json_set(o, "error", json_string(error ? error : ""));
    return o;
}

/* PoP: wt_extract_error_json @ tools/web_tools_extract.py:_extract_error_json */
static json_t *wt_extract_error_json(const char *error) {
    json_t *o = wt_success_error_node(error);
    char *s = wt_dumps_compact(o);
    json_free(o);
    json_t *ret = json_string(s ? s : "");
    free(s);
    return ret;
}

/* PoP: wt_refuse_all @ tools/web_tools_extract.py:_refuse_all
 * (None, None, None, json.dumps({"success": False, "error": error})) */
static json_t *wt_refuse_all(const char *error) {
    json_t *o = wt_success_error_node(error);
    char *s = wt_dumps_compact(o);
    json_free(o);
    json_t *arr = json_array();
    json_append(arr, json_null());
    json_append(arr, json_null());
    json_append(arr, json_null());
    json_append(arr, json_string(s ? s : ""));
    free(s);
    return arr;
}

/* PoP: wt_no_provider_error @ tools/web_tools_extract.py:_no_provider_error */
static json_t *wt_no_provider_error(wt_ctx *ctx, const char *capability, const char *fallback) {
    json_t *a = json_object();
    wt_arg_str(a, "capability", capability);
    json_t *dk = wt_call(ctx, "_disabled_web_plugin_for", a);
    if (wt_exc_pending(ctx)) { if (dk) json_free(dk); return NULL; }
    if (wt_truthy(dk)) {
        json_t *ret = wt_disabled_plugin_error(capability, wt_jstr(dk));
        json_free(dk);
        return ret;
    }
    json_free(dk);
    return json_string(fallback ? fallback : "");
}

/* PoP: wt_strict_selection_error @ tools/web_tools_extract.py:_strict_selection_error */
static json_t *wt_strict_selection_error(wt_ctx *ctx, const char *capability, const char *backend) {
    char failure[256];
    snprintf(failure, sizeof(failure), "no registered web %s provider has that name", capability);
    char selname[256];
    snprintf(selname, sizeof(selname), "'%s'", backend);
    json_t *a = json_object();
    wt_arg_str(a, "section", "web");
    wt_arg_str(a, "selection_name", selname);
    wt_arg_str(a, "failure", failure);
    json_t *se = wt_call(ctx, "selection_error", a);   /* argument evaluated first */
    if (wt_exc_pending(ctx)) { if (se) json_free(se); return NULL; }
    json_t *ret = wt_no_provider_error(ctx, capability, wt_jstr(se));
    json_free(se);
    return ret;
}

/* PoP: wt_merge_in_order @ tools/web_tools_extract.py:_merge_in_order
 * fixed keys are stringified ints; returns total-long list. */
static json_t *wt_merge_in_order(json_t *fixed, double total_d, json_t *fetch_positions,
                                 json_t *fetch_urls, json_t *results) {
    long long total = (long long)total_d;
    /* merged = dict(fixed) */
    json_t *merged = wt_is_obj(fixed) ? json_copy(fixed) : json_object();
    size_t npos = wt_is_arr(fetch_positions) ? fetch_positions->c.count : 0;
    size_t nres = wt_is_arr(results) ? results->c.count : 0;
    for (size_t pos = 0; pos < npos; pos++) {
        json_t *pn = fetch_positions->c.items[pos];
        long long position = wt_is_num(pn) ? (long long)pn->num_val : 0;
        const char *furl = wt_is_arr(fetch_urls) && pos < fetch_urls->c.count
                               ? wt_jstr(fetch_urls->c.items[pos]) : "";
        json_t *missing = wt_result_entry(furl, json_string(WT_NO_RESULT_ERROR));
        char key[32]; snprintf(key, sizeof(key), "%lld", position);
        if (pos < nres) {
            json_set(merged, key, json_copy(results->c.items[pos]));
            json_free(missing);
        } else {
            json_set(merged, key, missing);
        }
    }
    json_t *out = json_array();
    for (long long i = 0; i < total; i++) {
        char key[32]; snprintf(key, sizeof(key), "%lld", i);
        json_t *v = json_obj_get(merged, key);
        json_append(out, v ? json_copy(v) : json_null());
    }
    json_free(merged);
    return out;
}

/* PoP: wt_resolve_extract_provider @ tools/web_tools_extract.py:_resolve_extract_provider
 * Returns (provider, error_json) as a 2-element array. */
static json_t *wt_resolve_extract_provider(wt_ctx *ctx, const char *backend) {
    json_t *provider = NULL;
    if (backend && backend[0]) {
        json_t *a = json_object();
        wt_arg_str(a, "backend", backend);
        provider = wt_call(ctx, "get_provider", a);
        if (wt_exc_pending(ctx)) { if (provider) json_free(provider); return NULL; }
    }
    if (!wt_is_none(provider)) {
        json_t *se = wt_provider_method(ctx, "supports_extract");
        if (wt_exc_pending(ctx)) { if (se) json_free(se); json_free(provider); return NULL; }
        bool supports = wt_truthy(se); if (se) json_free(se);
        if (supports) {
            json_t *arr = json_array();
            json_append(arr, json_copy(provider));
            json_append(arr, json_null());
            json_free(provider);
            return arr;
        }
        /* search-only backend error */
        json_t *dn = wt_getattr_def(provider, "display_name");
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "%s is a search-only backend and cannot extract URL content. "
                 "Set web.extract_backend to %s", wt_jstr(dn), WT_EXTRACT_BACKENDS_HINT);
        json_t *ej = wt_extract_error_json(msg);
        json_t *arr = json_array();
        json_append(arr, json_null());
        json_append(arr, ej);
        json_free(provider);
        return arr;
    }
    if (provider) json_free(provider);

    if (backend && backend[0]) {
        json_t *sa = json_object();
        wt_arg_str(sa, "section", "web");
        json_t *ex = wt_call(ctx, "selection_exists", sa);
        if (wt_exc_pending(ctx)) { if (ex) json_free(ex); return NULL; }
        bool exists = wt_truthy(ex); if (ex) json_free(ex);
        if (exists) {
            json_t *sse = wt_strict_selection_error(ctx, "extract", backend);
            if (wt_exc_pending(ctx)) { if (sse) json_free(sse); return NULL; }
            json_t *ej = wt_extract_error_json(wt_jstr(sse));
            json_free(sse);
            json_t *arr = json_array();
            json_append(arr, json_null());
            json_append(arr, ej);
            return arr;
        }
    }

    json_t *active = wt_call(ctx, "get_active_extract_provider", json_object());
    if (wt_exc_pending(ctx)) { if (active) json_free(active); return NULL; }
    if (wt_is_none(active)) {
        char fallback[256];
        snprintf(fallback, sizeof(fallback),
                 "No web extract provider configured. Set web.extract_backend to %s",
                 WT_EXTRACT_BACKENDS_HINT);
        json_t *npe = wt_no_provider_error(ctx, "extract", fallback);
        if (wt_exc_pending(ctx)) { if (npe) json_free(npe); if (active) json_free(active); return NULL; }
        json_t *ej = wt_extract_error_json(wt_jstr(npe));
        json_free(npe);
        json_t *arr = json_array();
        json_append(arr, json_null());
        json_append(arr, ej);
        if (active) json_free(active);
        return arr;
    }
    json_t *arr = json_array();
    json_append(arr, active);   /* transfer ownership */
    json_append(arr, json_null());
    return arr;
}

/* PoP: wt_extract_timeout_seconds @ tools/web_tools_extract.py:_extract_timeout_seconds */
static json_t *wt_extract_timeout_seconds(wt_ctx *ctx) {
    json_t *cfg = wt_call(ctx, "_load_web_config", json_object());
    if (wt_exc_pending(ctx)) { if (cfg) json_free(cfg); return NULL; }
    json_t *v = wt_is_obj(cfg) ? json_obj_get(cfg, "extract_timeout") : NULL;
    /* .get("extract_timeout", 120.0): absent -> default; present null -> float(None) TypeError */
    json_t *val = v ? v : json_number(WT_DEFAULT_EXTRACT_TIMEOUT_S);
    bool owned = (v == NULL);
    double d;
    bool ok = wt_py_float(ctx, val, &d);
    if (owned) json_free(val);
    json_free(cfg);
    if (!ok) {
        if (wt_catch_typeerror_valueerror(ctx)) return json_number(WT_DEFAULT_EXTRACT_TIMEOUT_S);
        return NULL;
    }
    return json_number(d);
}

/* PoP: wt_backend_failed_whole_batch @ tools/web_tools_extract.py:_backend_failed_whole_batch */
static json_t *wt_backend_failed_whole_batch(wt_ctx *ctx, json_t *results) {
    /* errors = [r.get("error") for r in results] */
    size_t n = wt_is_arr(results) ? results->c.count : 0;
    if (n == 0) return json_bool(false);   /* bool(errors) False */
    bool all_truthy = true;
    for (size_t i = 0; i < n; i++) {
        json_t *r = results->c.items[i];
        json_t *e = wt_is_obj(r) ? json_obj_get(r, "error") : NULL;
        if (!wt_truthy(e)) { all_truthy = false; break; }
    }
    if (!all_truthy) return json_bool(false);  /* all(errors) False, short-circuit */
    /* not any(is_origin_http_error(e) for e in errors) */
    bool any_origin = false;
    for (size_t i = 0; i < n; i++) {
        json_t *r = results->c.items[i];
        json_t *e = wt_is_obj(r) ? json_obj_get(r, "error") : NULL;
        json_t *a = json_object();
        wt_arg_node(a, "error", e);
        json_t *io = wt_call(ctx, "is_origin_http_error", a);
        if (wt_exc_pending(ctx)) { if (io) json_free(io); return NULL; }
        bool b = wt_truthy(io); if (io) json_free(io);
        if (b) { any_origin = true; break; }
    }
    return json_bool(!any_origin);
}

/* ── web_tools.py: _memoized_search (+ nested _paid_search / _served_after_failure) ── */

/* _served_after_failure(error, fetch_limit): managed fallback else keyless rescue. */
static json_t *wt_served_after_failure(wt_ctx *ctx, json_t *provider,
                                       const char *error, const char *query, double fetch_limit) {
    json_t *fallback = wt_managed_search_fallback(ctx, provider, error, query, fetch_limit);
    if (wt_exc_pending(ctx)) { if (fallback) json_free(fallback); return NULL; }
    if (!wt_is_none(fallback)) return fallback;
    if (fallback) json_free(fallback);
    /* _rescue_search(...) if _rescue_eligible(provider) else None */
    json_t *ea = json_object();
    wt_arg_node(ea, "provider", provider);
    json_t *elig = wt_call(ctx, "_rescue_eligible", ea);
    if (wt_exc_pending(ctx)) { if (elig) json_free(elig); return NULL; }
    bool eligible = wt_truthy(elig); if (elig) json_free(elig);
    if (!eligible) return json_null();
    json_t *nm = wt_getattr_def(provider, "name");
    return wt_rescue_search(ctx, wt_jstr(nm), error, query, fetch_limit);
}

/* _paid_search() -> (resp, was_rescued). Returns a 2-element array. */
static json_t *wt_paid_search(wt_ctx *ctx, json_t *provider, const char *query, double limit) {
    json_t *ba = json_object();
    json_set(ba, "limit", json_number(limit));
    json_t *bl = wt_call(ctx, "bucket_limit", ba);
    if (wt_exc_pending(ctx)) { if (bl) json_free(bl); return NULL; }
    double fetch_limit = wt_is_num(bl) ? bl->num_val : 0.0;
    if (bl) json_free(bl);

    json_t *sargs = json_object();
    wt_arg_str(sargs, "query", query);
    json_set(sargs, "limit", json_number(fetch_limit));
    json_t *resp = wt_call(ctx, "provider.search", sargs);
    if (wt_exc_pending(ctx)) {
        /* except Exception as exc: served = _served_after_failure(str(exc), fetch_limit);
         * if served is None: raise (re-raise the ORIGINAL exc, class preserved). */
        json_t *orig_exc = json_copy(ctx->exc);
        char msg[512]; snprintf(msg, sizeof(msg), "%s", wt_exc_msg(ctx));
        if (wt_catch_exception(ctx)) {
            if (resp) json_free(resp);
            json_t *served = wt_served_after_failure(ctx, provider, msg, query, fetch_limit);
            if (wt_exc_pending(ctx)) { if (served) json_free(served); json_free(orig_exc); return NULL; }
            if (wt_is_none(served)) {
                if (served) json_free(served);
                if (ctx->exc) json_free(ctx->exc);
                ctx->exc = orig_exc;   /* restore original raise (transfer) */
                return NULL;
            }
            json_free(orig_exc);
            json_t *arr = json_array();
            json_append(arr, served);
            json_append(arr, json_bool(true));
            return arr;
        }
        json_free(orig_exc);
        if (resp) json_free(resp);
        return NULL;   /* BaseException trio propagates */
    }
    /* if not resp.get("success"): served = _served_after_failure(str(resp.get("error","")), fetch_limit) */
    json_t *succ = wt_is_obj(resp) ? json_obj_get(resp, "success") : NULL;
    if (!wt_truthy(succ)) {
        json_t *err = wt_is_obj(resp) ? json_obj_get(resp, "error") : NULL;
        const char *es;
        if (!err) es = "";                       /* .get("error","") absent -> "" */
        else if (wt_is_str(err)) es = wt_jstr(err);
        else if (wt_is_none(err)) es = "None";   /* str(None) */
        else es = "";
        char msg[512]; snprintf(msg, sizeof(msg), "%s", es);
        json_t *served = wt_served_after_failure(ctx, provider, msg, query, fetch_limit);
        if (wt_exc_pending(ctx)) { if (served) json_free(served); if (resp) json_free(resp); return NULL; }
        if (!wt_is_none(served)) {
            json_t *arr = json_array();
            json_append(arr, served);
            json_append(arr, json_bool(true));
            if (resp) json_free(resp);
            return arr;
        }
        if (served) json_free(served);
    }
    json_t *arr = json_array();
    json_append(arr, resp ? resp : json_null());
    json_append(arr, json_bool(false));
    return arr;
}

/* PoP: wt_memoized_search @ tools/web_tools.py:_memoized_search
 * search_memo.lookup / flight_lock (inert CM) / store; slice_search_response. */
static json_t *wt_memoized_search(wt_ctx *ctx, json_t *provider, const char *query, double limit) {
    json_t *nm = wt_getattr_def(provider, "name");
    const char *pname = wt_jstr(nm);

    json_t *la = json_object();
    wt_arg_str(la, "name", pname);
    wt_arg_str(la, "query", query);
    json_set(la, "limit", json_number(limit));
    json_t *response_data = wt_call(ctx, "search_memo.lookup", la);
    if (wt_exc_pending(ctx)) { if (response_data) json_free(response_data); return NULL; }

    if (wt_is_none(response_data)) {
        if (response_data) json_free(response_data);
        /* with search_memo.flight_lock(name, query, limit): (inert CM) */
        json_t *fa = json_object();
        wt_arg_str(fa, "name", pname);
        wt_arg_str(fa, "query", query);
        json_set(fa, "limit", json_number(limit));
        json_t *cm = wt_call(ctx, "search_memo.flight_lock", fa);
        if (cm) json_free(cm);
        if (wt_exc_pending(ctx)) return NULL;

        json_t *la2 = json_object();
        wt_arg_str(la2, "name", pname);
        wt_arg_str(la2, "query", query);
        json_set(la2, "limit", json_number(limit));
        response_data = wt_call(ctx, "search_memo.lookup", la2);
        if (wt_exc_pending(ctx)) { if (response_data) json_free(response_data); return NULL; }

        if (wt_is_none(response_data)) {
            if (response_data) json_free(response_data);
            json_t *paid = wt_paid_search(ctx, provider, query, limit);
            if (wt_exc_pending(ctx)) { if (paid) json_free(paid); return NULL; }
            /* paid = (response_data, was_rescued) */
            json_t *rd = wt_is_arr(paid) && paid->c.count > 0 ? paid->c.items[0] : NULL;
            json_t *wr = wt_is_arr(paid) && paid->c.count > 1 ? paid->c.items[1] : NULL;
            bool was_rescued = wt_truthy(wr);
            response_data = rd ? json_copy(rd) : json_null();
            json_free(paid);
            if (!was_rescued) {
                json_t *sa = json_object();
                wt_arg_str(sa, "name", pname);
                wt_arg_str(sa, "query", query);
                json_set(sa, "limit", json_number(limit));
                wt_arg_node(sa, "response", response_data);
                json_t *st = wt_call(ctx, "search_memo.store", sa);
                if (st) json_free(st);
                if (wt_exc_pending(ctx)) { json_free(response_data); return NULL; }
            }
        }
    }

    json_t *sla = json_object();
    wt_arg_node(sla, "response", response_data);
    json_set(sla, "limit", json_number(limit));
    json_free(response_data);
    json_t *sliced = wt_call(ctx, "slice_search_response", sla);
    if (wt_exc_pending(ctx)) { if (sliced) json_free(sliced); return NULL; }
    return sliced ? sliced : json_null();
}