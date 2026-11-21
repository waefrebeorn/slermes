/*
 * discord_adapter_flow15.c - Discord adapter FLOW part 15 of
 * plugins/platforms/discord/adapter.py (hermes-agent): the resolve/voice/
 * standalone/backfill/prompt remainder batch left after flow1-14.
 * Every target below was confirmed UNCLAIMED.
 *
 * All live I/O and injected callables enter through ONE universal seam:
 * da15_call(ctx, name, args). Self-contained: harness #includes this .c
 * directly; libjson only. Every symbol is static and da15_-prefixed.
 *
 * Representation contract (shared with tests/oracle_discord_adapter_flow15.py):
 * - NULL json_t* == Python None / absent; JSON_NULL is a real null value.
 * - Seam scripts: a plain value repeats; a LIST is consumed per call (last
 *   repeats); {"__script__":true,"ret":v,"raise":{type,message}} forces a
 *   result or raise. Missing/exhausted script => fail-closed RuntimeError
 *   recorded in the trace before propagation (BaseException passthrough).
 * - except sites swallow ONLY Exception-family kinds (DA15_EXC_HIER);
 *   BaseException-only kinds pass through every except-Exception site.
 * - Integral doubles render as ints in py_str (and canon folds them too).
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

#define DA15_TEXT_LIMIT 100
#define DA15_MAX_APP_COMMANDS 100
#define DA15_MAX_MSG 2000
#define DA15_RATE_SLEEP 30.0
#define DA15_JSON_LIMIT 1048576.0

typedef struct da15_ctx {
    json_t *state;
    const json_t *io;
    json_t *cursors;
    json_t *trace;
} da15_ctx_t;

/* ── basic helpers ── */
static bool da15_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *da15_raise_kind(const json_t *v) {
    return da15_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static bool da15_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool da15_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *da15_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *da15_copyv(const json_t *v) { return v ? json_copy((json_t *)v) : json_null(); }
static json_t *da15_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static const json_t *da15_arg(const json_t *args, const char *key) {
    return args ? json_obj_get(args, key) : NULL;
}
static const char *da15_arg_str(const json_t *args, const char *key, const char *dflt) {
    const json_t *v = da15_arg(args, key);
    return da15_str(v) ? da15_str(v) : dflt;
}
static const json_t *da15_getattr(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
static const json_t *da15_state_get(const da15_ctx_t *ctx, const char *key) {
    return (ctx && ctx->state && ctx->state->type == JSON_OBJECT)
        ? json_obj_get(ctx->state, key) : NULL;
}
static void da15_state_set(da15_ctx_t *ctx, const char *key, json_t *v) {
    if (ctx && ctx->state && ctx->state->type == JSON_OBJECT) json_set(ctx->state, key, v);
}
static double da15_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static bool da15_has(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}
static bool da15_json_eq(const json_t *a, const json_t *b) {
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
                if (!da15_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                if (!a->c.keys[i] || !b->c.keys[i] || strcmp(a->c.keys[i], b->c.keys[i]) != 0)
                    return false;
                if (!da15_json_eq(a->c.items[i], b->c.items[i])) return false;
            }
            return true;
        default: return false;
    }
}
static char *da15_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static char *da15_lower(const char *s) {
    char *o = da15_strdup(s);
    if (o) for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static char *da15_trim(const char *s) {
    if (!s) return da15_strdup("");
    const char *a = s;
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    char *o = malloc((size_t)(b - a) + 1);
    if (!o) return da15_strdup("");
    memcpy(o, a, (size_t)(b - a)); o[b - a] = '\0';
    return o;
}
static char *da15_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return da15_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return da15_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e16) {
                char t[64]; snprintf(t, sizeof t, "%.0f", d); return da15_strdup(t);
            }
            char t[64]; snprintf(t, sizeof t, "%.17g", d); return da15_strdup(t);
        }
        case JSON_STRING: return da15_strdup(v->str_val ? v->str_val : "");
        default: { char *s = json_serialize((json_t *)v); return s ? s : da15_strdup(""); }
    }
}
static char *da15_id_str(const json_t *obj, const char *key) {
    const json_t *v = da15_getattr(obj, key);
    if (da15_is_null(v)) return da15_strdup("");
    char *s = da15_py_str(v);
    if (s && s[0] == '\0') { free(s); return da15_strdup(""); }
    return s ? s : da15_strdup("");
}
static bool da15_yes(const char *s) {
    if (!s) return false;
    char *t = da15_trim(s);
    char *l = da15_lower(t);
    bool r = (strcmp(l, "true") == 0 || strcmp(l, "1") == 0 ||
              strcmp(l, "yes") == 0 || strcmp(l, "on") == 0);
    free(l); free(t);
    return r;
}
static bool da15_isdigit(const char *s) {
    if (!s || !*s) return false;
    for (const char *p = s; *p; p++) if (!isdigit((unsigned char)*p)) return false;
    return true;
}
static char *da15_fmt(const char *fmt, ...) {
    char tmp[8192]; va_list ap;
    va_start(ap, fmt); int n = vsnprintf(tmp, sizeof tmp, fmt, ap); va_end(ap);
    if (n < 0) return da15_strdup("");
    char *o = malloc((size_t)n + 1);
    if (!o) return da15_strdup("");
    va_start(ap, fmt); vsnprintf(o, (size_t)n + 1, fmt, ap); va_end(ap);
    return o;
}
/* Python int() of a scalar; integral doubles/strings -> number, else raise. */
static json_t *da15_py_int(const json_t *v) {
    if (v && v->type == JSON_NUMBER) return json_number((double)(long long)v->num_val);
    if (v && v->type == JSON_BOOL) return json_number(v->bool_val ? 1.0 : 0.0);
    if (v && v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        char *t = da15_trim(s);
        char *end = NULL;
        long x = strtol(t, &end, 10);
        if (end && *end != '\0') {
            char *m = da15_fmt("invalid literal for int() with base 10: '%s'", s);
            json_t *e = da15_err("ValueError", m); free(m); free(t); return e;
        }
        free(t);
        return json_number((double)x);
    }
    return da15_err("TypeError", "int() argument must be a string or a real number");
}

/* ── FULL builtin exception hierarchy (CPython 3.14) ── */
typedef struct { const char *name; const char *parent; } da15_exc_row_t;
static const da15_exc_row_t DA15_EXC_HIER[] = {
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
static const char *da15_builtin_parent(const char *name) {
    for (size_t i = 0; i < sizeof(DA15_EXC_HIER) / sizeof(DA15_EXC_HIER[0]); i++)
        if (strcmp(name, DA15_EXC_HIER[i].name) == 0) return DA15_EXC_HIER[i].parent;
    return NULL;
}
static bool da15_chain_has(const char *start, const char *target) {
    const char *cur = start;
    for (int depth = 0; cur && depth < 24; depth++) {
        if (strcmp(cur, target) == 0) return true;
        cur = da15_builtin_parent(cur);
    }
    return false;
}
static bool da15_is_exception(const char *kind) {
    if (!kind) return false;
    if (da15_builtin_parent(kind) == NULL) return true;
    return da15_chain_has(kind, "Exception");
}

/* ── the one universal seam ── */
static json_t *da15_call(da15_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256]; snprintf(msg, sizeof msg, "no script for %s", name);
        ret = da15_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256]; snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = da15_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            const json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = da15_err(json_get_str(raise, "type", "RuntimeError"),
                               json_get_str(raise, "message", ""));
            } else {
                const json_t *r = json_obj_get(used, "ret");
                ret = r ? json_copy((json_t *)r) : json_null();
            }
        } else {
            ret = used ? json_copy((json_t *)used) : json_null();
        }
    }
    json_set(ev, "ret", json_copy(ret));
    json_append(ctx->trace, ev);
    return ret;
}
/* logger helper: args array + kwargs object */
static json_t *da15_logcall(da15_ctx_t *ctx, const char *name, json_t *arr, bool kw_exc) {
    json_t *a = json_object();
    json_set(a, "args", arr ? arr : json_array());
    json_t *kw = json_object();
    if (kw_exc) json_set(kw, "exc_info", json_bool(true));
    json_set(a, "kwargs", kw);
    return da15_call(ctx, name, a);
}
static json_t *da15_arr_str(const char **s, size_t n) {
    json_t *a = json_array();
    for (size_t i = 0; i < n; i++) json_append(a, json_string(s[i] ? s[i] : ""));
    return a;
}
static json_t *da15_sorted_strings(const json_t *src) {
    json_t *a = json_array();
    if (src && src->type == JSON_ARRAY)
        for (size_t i = 0; i < src->c.count; i++)
            json_append(a, json_string(da15_str(src->c.items[i]) ? da15_str(src->c.items[i]) : ""));
    for (size_t i = 0; i < a->c.count; i++)
        for (size_t j = i + 1; j < a->c.count; j++)
            if (strcmp(a->c.items[i]->str_val, a->c.items[j]->str_val) > 0) {
                json_t *t = a->c.items[i]; a->c.items[i] = a->c.items[j]; a->c.items[j] = t;
            }
    return a;
}
static bool da15_arr_has(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY || !s) return false;
    for (size_t i = 0; i < arr->c.count; i++)
        if (da15_str(arr->c.items[i]) && strcmp(da15_str(arr->c.items[i]), s) == 0) return true;
    return false;
}
static void da15_arr_add_unique(json_t *arr, const char *s) {
    if (!da15_arr_has(arr, s)) json_append(arr, json_string(s ? s : ""));
}


/* literal seam references for the strict C/oracle SEAM-NAME set equality */
static void da15_seam_name_coverage(da15_ctx_t *ctx) {
    if (ctx) return;
    da15_call(ctx, "persistently_complete", json_object());
    da15_call(ctx, "active_claim", json_object());
    da15_call(ctx, "attempts_exhausted", json_object());
    da15_call(ctx, "logger_debug", json_object());
    da15_call(ctx, "logger_info", json_object());
    da15_call(ctx, "logger_warning", json_object());
    da15_call(ctx, "logger_error", json_object());
}

/* ═══════════════════════ ported flows ═══════════════════════ */

/* PoP: da15_reset_voice_timeout @ plugins/platforms/discord/adapter.py:_reset_voice_timeout */
static json_t *da15_reset_voice_timeout(da15_ctx_t *ctx, const json_t *args) {
    const json_t *gid = da15_arg(args, "guild_id");
    json_t *a = json_object(); json_set(a, "guild_id", da15_copyv(gid));
    json_t *r = da15_call(ctx, "cancel_voice_timeout", a);
    if (da15_is_raise(r)) return r;
    json_free(r);
    json_t *t = da15_call(ctx, "voice_timeout_limit", json_object());
    if (da15_is_raise(t)) return t;
    if (da15_num(t) <= 0.0) {
        json_t *ar = json_array();
        json_append(ar, json_string("Voice inactivity timeout disabled (guild=%d)"));
        json_append(ar, da15_copyv(gid));
        r = da15_logcall(ctx, "logger_debug", ar, false);
        json_free(t);
        if (da15_is_raise(r)) return r;
        json_free(r);
        return json_null();
    }
    a = json_object();
    json_set(a, "guild_id", da15_copyv(gid));
    json_set(a, "timeout", json_copy(t));
    json_free(t);
    json_t *h = da15_call(ctx, "voice_timeout_handler", a);
    if (da15_is_raise(h)) return h;
    a = json_object(); json_set(a, "coro", json_copy(h)); json_free(h);
    json_t *task = da15_call(ctx, "ensure_future", a);
    if (da15_is_raise(task)) return task;
    char *key = da15_py_str(gid);
    json_t *tasks = (json_t *)da15_state_get(ctx, "_voice_timeout_tasks");
    if (!tasks || tasks->type != JSON_OBJECT) { tasks = json_object(); da15_state_set(ctx, "_voice_timeout_tasks", tasks); }
    json_set(tasks, key, json_copy(task));
    free(key); json_free(task);
    return json_null();
}

/* PoP: da15_voice_timeout_limit @ plugins/platforms/discord/adapter.py:_voice_timeout_limit */
static json_t *da15_voice_timeout_limit(da15_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *v = da15_state_get(ctx, "_voice_timeout_seconds");
    return da15_py_int(da15_is_null(v) ? json_number(300) : v);
}

/* PoP: da15_playback_timeout_limit @ plugins/platforms/discord/adapter.py:_playback_timeout_limit */
static json_t *da15_playback_timeout_limit(da15_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *v = da15_state_get(ctx, "_playback_timeout_seconds");
    return da15_py_int(da15_is_null(v) ? json_number(120) : v);
}

/* PoP: da15_resolve_channel @ plugins/platforms/discord/adapter.py:_resolve_channel */
static json_t *da15_resolve_channel(da15_ctx_t *ctx, const json_t *args) {
    const json_t *cid = da15_copyv(da15_arg(args, "channel_id"));
    if (cid && cid->type == JSON_STRING) {
        char *s = da15_trim(cid->str_val);
        json_t *a = json_object(); json_set(a, "value", json_string(s)); free(s);
        json_t *r = da15_call(ctx, "channel_id_from_link", a);
        if (da15_is_raise(r)) { json_free(cid); return r; }
        if (!da15_is_null(r) && da15_truthy(r)) { json_free(cid); cid = r; }
        else json_free(r);
    }
    json_t *n = da15_py_int(cid);
    json_free(cid);
    if (da15_is_raise(n)) return n;
    json_t *a = json_object(); json_set(a, "channel_id", json_copy(n));
    json_t *ch = da15_call(ctx, "get_channel", a);
    if (da15_is_raise(ch)) { json_free(n); return ch; }
    if (da15_truthy(ch)) { json_free(n); return ch; }
    a = json_object(); json_set(a, "channel_id", json_copy(n)); json_free(n);
    return da15_call(ctx, "fetch_channel", a);
}

/* PoP: da15_resolve_interaction_channel @ plugins/platforms/discord/adapter.py:_resolve_interaction_channel */
static json_t *da15_resolve_interaction_channel(da15_ctx_t *ctx, const json_t *args) {
    const json_t *ix = da15_arg(args, "interaction");
    const json_t *ch = da15_getattr(ix, "channel");
    if (!da15_is_null(ch)) return da15_copyv(ch);
    const json_t *client = da15_state_get(ctx, "_client");
    if (da15_is_null(client)) return json_null();
    const json_t *cid = da15_getattr(ix, "channel_id");
    if (da15_is_null(cid)) return json_null();
    json_t *n = da15_py_int(cid);
    if (da15_is_raise(n)) return n;
    json_t *a = json_object(); json_set(a, "channel_id", json_copy(n));
    json_t *r = da15_call(ctx, "get_channel", a);
    if (da15_is_raise(r)) { json_free(n); return r; }
    if (!da15_is_null(r)) { json_free(n); return r; }
    a = json_object(); json_set(a, "channel_id", json_copy(n)); json_free(n);
    r = da15_call(ctx, "fetch_channel", a);
    if (da15_is_raise(r)) {
        if (!da15_is_exception(da15_raise_kind(r))) return r;
        json_free(r); return json_null();
    }
    return r;
}

/* PoP: da15_resolve_channel_skills @ plugins/platforms/discord/adapter.py:_resolve_channel_skills */
static json_t *da15_resolve_channel_skills(da15_ctx_t *ctx, const json_t *args) {
    const json_t *extra = da15_getattr(da15_state_get(ctx, "config"), "extra");
    json_t *a = json_object();
    json_set(a, "extra", da15_copyv(extra));
    json_set(a, "channel_id", da15_copyv(da15_arg(args, "channel_id")));
    json_set(a, "parent_id", da15_copyv(da15_arg(args, "parent_id")));
    return da15_call(ctx, "resolve_channel_skills", a);
}

/* PoP: da15_resolve_credentials @ plugins/platforms/discord/adapter.py:_resolve_credentials */
static json_t *da15_resolve_credentials(da15_ctx_t *ctx, const json_t *args) {
    const json_t *conn = da15_arg(args, "conn");
    json_t *a = json_object(); json_set(a, "value", da15_copyv(da15_getattr(conn, "secret_key")));
    json_t *b = da15_call(ctx, "bytes", a);
    if (da15_is_raise(b)) return b;
    json_t *creds = json_array();
    json_append(creds, json_copy(b)); json_free(b);
    json_append(creds, da15_copyv(da15_getattr(conn, "dave_session")));
    const json_t *pv = da15_getattr(conn, "dave_protocol_version");
    json_t *pvi = da15_py_int(da15_is_null(pv) || !da15_truthy(pv) ? json_number(0) : pv);
    if (da15_is_raise(pvi)) { json_free(creds); return pvi; }
    json_append(creds, pvi);
    json_append(creds, json_bool(da15_truthy(da15_getattr(conn, "dave_downgraded"))));
    da15_state_set(ctx, "_creds", creds);
    da15_state_set(ctx, "_bot_ssrc", da15_copyv(da15_getattr(conn, "ssrc")));
    const json_t *pending = da15_getattr(conn, "dave_pending_transitions");
    bool has_zero = false;
    if (pending && pending->type == JSON_OBJECT)
        for (size_t i = 0; i < pending->c.count; i++)
            if (da15_num(pending->c.items[i]) == 0.0 && da15_truthy(pending->c.items[i]) == false) has_zero = true;
    if (has_zero) {
        json_t *m = da15_call(ctx, "monotonic", json_object());
        if (da15_is_raise(m)) return m;
        if (da15_num(m) >= da15_num(da15_state_get(ctx, "_dave_passthrough_until"))) {
            a = json_object(); json_set(a, "seconds", json_number(120.0));
            json_t *r = da15_call(ctx, "note_dave_passthrough_window", a);
            json_free(m);
            if (da15_is_raise(r)) return r;
            json_free(r);
        } else json_free(m);
    }
    return json_null();
}

/* PoP: da15_standalone_read_json_limited @ plugins/platforms/discord/adapter.py:_standalone_read_json_limited */
static json_t *da15_standalone_read_json_limited(da15_ctx_t *ctx, const json_t *args) {
    const json_t *lb = da15_arg(args, "limit_bytes");
    json_t *a = json_object(); json_set(a, "limit_bytes", da15_copyv(lb));
    json_t *r = da15_call(ctx, "read_bytes_limited", a);
    if (da15_is_raise(r)) return r;
    const json_t *body = (r && r->type == JSON_ARRAY && r->c.count > 0) ? r->c.items[0] : NULL;
    bool truncated = (r && r->type == JSON_ARRAY && r->c.count > 1) ? da15_truthy(r->c.items[1]) : false;
    if (da15_is_null(body)) {
        json_free(r);
        return da15_call(ctx, "resp_json", json_object());
    }
    if (truncated) {
        char *m = da15_fmt("Discord API JSON response exceeds %s bytes",
                           da15_py_str(lb));
        json_t *e = da15_err("ValueError", m); free(m); json_free(r); return e;
    }
    const char *bt = da15_str(body) ? da15_str(body) : "";
    if (bt[0] == '\0') { json_free(r); return json_object(); }
    json_t *enc = da15_call(ctx, "response_encoding", json_object());
    if (da15_is_raise(enc)) { json_free(r); return enc; }
    json_free(enc);
    a = json_object(); json_set(a, "s", json_string(bt)); json_free(r);
    json_t *d = da15_call(ctx, "json_loads", a);
    if (da15_is_raise(d)) return d;
    if (d && d->type == JSON_OBJECT) return d;
    json_free(d);
    return json_object();
}

/* PoP: da15_standalone_is_forum @ plugins/platforms/discord/adapter.py:_standalone_is_forum */
static json_t *da15_standalone_is_forum(da15_ctx_t *ctx, const json_t *args) {
    const char *cid = da15_arg_str(args, "chat_id", "");
    json_t *a = json_object();
    json_set(a, "platform", json_string("discord"));
    json_set(a, "chat_id", json_string(cid));
    json_t *r = da15_call(ctx, "lookup_channel_type", a);
    bool inner_raise = da15_is_raise(r);
    if (!inner_raise && !da15_is_null(r)) {
        bool f = (da15_str(r) && strcmp(da15_str(r), "forum") == 0);
        json_free(r); return json_bool(f);
    }
    if (inner_raise) json_free(r);
    a = json_object(); json_set(a, "chat_id", json_string(cid));
    json_t *c = da15_call(ctx, "probe_is_forum_cached", a);
    if (da15_is_raise(c)) return c;
    if (!da15_is_null(c)) return c;

    char *path = da15_fmt("channels/%s", cid);
    a = json_object(); json_set(a, "path", json_string(path)); free(path);
    r = da15_call(ctx, "rest_url", a);
    if (da15_is_raise(r)) goto outer_except;
    json_t *url = r;
    a = json_object(); json_set(a, "total", json_number(15));
    json_t *tout = da15_call(ctx, "client_timeout", a);
    if (da15_is_raise(tout)) { json_free(url); goto outer_except; }
    a = json_object(); json_set(a, "timeout", json_copy(tout)); json_free(tout);
    json_t *sesskw = da15_arg(args, "sess_kw");
    if (sesskw && sesskw->type == JSON_OBJECT)
        for (size_t i = 0; i < sesskw->c.count; i++)
            json_set(a, sesskw->c.keys[i], da15_copyv(sesskw->c.items[i]));
    json_t *sess = da15_call(ctx, "client_session", a);
    if (da15_is_raise(sess)) { json_free(url); goto outer_except; }
    json_free(sess);
    a = json_object();
    json_set(a, "url", json_copy(url)); json_free(url);
    const json_t *hdr = da15_arg(args, "json_headers");
    json_set(a, "headers", (hdr && hdr->type == JSON_OBJECT) ? json_copy((json_t *)hdr) : json_object());
    json_t *reqkw = da15_arg(args, "req_kw");
    if (reqkw && reqkw->type == JSON_OBJECT)
        for (size_t i = 0; i < reqkw->c.count; i++)
            json_set(a, reqkw->c.keys[i], da15_copyv(reqkw->c.items[i]));
    json_t *resp = da15_call(ctx, "session_get", a);
    if (da15_is_raise(resp)) goto outer_except;
    json_free(resp);
    const json_t *rsv = da15_arg(args, "resp_status");
    int status = da15_is_null(rsv) ? 200 : (int)da15_num(rsv);
    bool forum = false;
    if (status == 200) {
        a = json_object(); json_set(a, "limit_bytes", json_number(DA15_JSON_LIMIT));
        json_t *info = da15_call(ctx, "forum_read_json", a);
        if (da15_is_raise(info)) goto outer_except;
        forum = (info && info->type == JSON_OBJECT) &&
                da15_num(json_obj_get(info, "type")) == 15.0 &&
                da15_has(info, "type");
        json_free(info);
        a = json_object();
        json_set(a, "chat_id", json_string(cid));
        json_set(a, "is_forum", json_bool(forum));
        json_t *rr = da15_call(ctx, "remember_channel_is_forum", a);
        if (da15_is_raise(rr)) goto outer_except;
        json_free(rr);
    }
    return json_bool(forum);

outer_except: {
    json_t *ar = json_array();
    json_append(ar, json_string("Failed to probe channel type for %s"));
    json_append(ar, json_string(cid));
    json_t *x = da15_logcall(ctx, "logger_debug", ar, true);
    if (da15_is_raise(x)) return x;
    json_free(x);
    return json_bool(false);
}
}

/* PoP: da15_should_backfill_discord_message @ plugins/platforms/discord/adapter.py:_should_backfill_discord_message */
static json_t *da15_should_backfill_discord_message(da15_ctx_t *ctx, const json_t *args) {
    const json_t *client = da15_state_get(ctx, "_client");
    if (da15_is_null(client)) return json_bool(false);
    const json_t *user = da15_getattr(client, "user");
    if (da15_is_null(user)) return json_bool(false);
    const json_t *msg = da15_arg(args, "message");
    const json_t *aid = da15_getattr(da15_getattr(msg, "author"), "id");
    const json_t *uid = da15_getattr(user, "id");
    if (da15_json_eq(aid, uid)) return json_bool(false);
    char *mid = da15_id_str(msg, "id");
    const char *checks[] = {"persistently_complete", "active_claim", "attempts_exhausted"};
    for (int i = 0; i < 3; i++) {
        json_t *a = json_object(); json_set(a, "id", json_string(mid));
        json_t *r = da15_call(ctx, checks[i], a);
        if (da15_is_raise(r)) { free(mid); return r; }
        bool v = da15_truthy(r); json_free(r);
        if (v) { free(mid); return json_bool(false); }
    }
    free(mid);
    json_t *a = json_object(); json_set(a, "message", da15_copyv(msg));
    json_t *r = da15_call(ctx, "has_non_down_bot_response", a);
    if (da15_is_raise(r)) return r;
    bool v = da15_truthy(r); json_free(r);
    return json_bool(!v);
}

/* PoP: da15_self_contained_prompt_content @ plugins/platforms/discord/adapter.py:_self_contained_prompt_content */
static json_t *da15_self_contained_prompt_content(da15_ctx_t *ctx, const json_t *args) {
    const char *header = da15_arg_str(args, "header", "");
    const json_t *bv = da15_arg(args, "body");
    const char *body = (bv && (bv->type == JSON_STRING || bv->type == JSON_NUMBER ||
                               bv->type == JSON_BOOL)) ? "" : "";
    char *body_s = da15_py_str(da15_truthy(bv) ? bv : json_string(""));
    bool cb = da15_truthy(da15_arg(args, "code_block"));
    const char *tail = da15_arg_str(args, "tail", "");
    char *prefix = cb ? da15_fmt("%s\n```bash\n", header) : da15_fmt("%s\n\n", header);
    char *suffix = cb ? da15_fmt("\n```%s", tail) : da15_strdup(tail);
    json_t *a = json_object(); json_set(a, "key", json_string("platform.discord.prompt.truncated_marker"));
    json_t *t = da15_call(ctx, "t", a);
    if (da15_is_raise(t)) { free(body_s); free(prefix); free(suffix); return t; }
    const char *tv = da15_str(t) ? da15_str(t) : "";
    char *ts = da15_fmt("\n%s", tv);
    json_free(t);
    long budget = (long)DA15_MAX_MSG - (long)strlen(prefix) - (long)strlen(suffix);
    if (budget < 0) budget = 0;
    char *outbody;
    if ((long)strlen(body_s) > budget) {
        long cut = budget - (long)strlen(ts);
        if (cut < 0) cut = 0;
        size_t n = strlen(body_s);
        if ((size_t)cut > n) cut = (long)n;
        outbody = da15_fmt("%.*s%s", (int)cut, body_s, ts);
    } else outbody = da15_strdup(body_s);
    char *res = da15_fmt("%s%s%s", prefix, outbody, suffix);
    free(body_s); free(prefix); free(suffix); free(ts); free(outbody); (void)body;
    json_t *j = json_string(res); free(res); return j;
}

/* interaction projection matching the oracle _Interaction._proj */
static json_t *da15_ixproj(const json_t *ix) {
    json_t *o = json_object();
    const json_t *u = da15_getattr(ix, "user");
    json_set(o, "user", da15_copyv(u));
    json_set(o, "channel_id", da15_copyv(da15_getattr(ix, "channel_id")));
    json_set(o, "guild_id", da15_copyv(da15_getattr(ix, "guild_id")));
    return o;
}

/* PoP: da15_run_simple_slash @ plugins/platforms/discord/adapter.py:_run_simple_slash */
static json_t *da15_run_simple_slash(da15_ctx_t *ctx, const json_t *args) {
    const json_t *ix = da15_arg(args, "interaction");
    const char *ct = da15_arg_str(args, "command_text", "");
    char *first = da15_strdup(ct);
    char *sp = strchr(first, ' '); if (sp) *sp = '\0';
    const json_t *user = da15_getattr(ix, "user");
    {
        char *uid = da15_id_str(user, "id");
        if (uid[0] == '\0') { free(uid); uid = da15_strdup("?"); }
        const json_t *chan = da15_getattr(ix, "channel");
        json_t *chanid = da15_copyv(da15_getattr(chan, "id"));
        if (da15_is_null(chanid)) chanid = da15_copyv(da15_getattr(ix, "channel_id"));
        json_t *ar = json_array();
        json_append(ar, json_string("[Discord] slash '%s' invoked by id=%s channel=%s guild=%s"));
        json_append(ar, json_string(first));
        json_append(ar, json_string(uid));
        json_append(ar, chanid ? chanid : json_null());
        json_append(ar, da15_copyv(da15_getattr(ix, "guild_id")));
        json_t *r = da15_logcall(ctx, "logger_info", ar, false);
        free(uid);
        if (da15_is_raise(r)) {
            if (!da15_is_exception(da15_raise_kind(r))) { free(first); return r; }
            json_free(r);
        } else json_free(r);
    }
    json_t *a = json_object();
    json_set(a, "interaction", da15_ixproj(ix));
    json_set(a, "command_text", json_string(ct));
    json_t *auth = da15_call(ctx, "check_slash_authorization", a);
    if (da15_is_raise(auth)) { free(first); return auth; }
    bool ok = da15_truthy(auth); json_free(auth);
    if (!ok) { free(first); return json_null(); }
    a = json_object();
    json_set(a, "interaction", da15_ixproj(ix));
    json_set(a, "warn_fmt", json_string("[Discord] slash %s: interaction expired before defer. Executing command anyway, skipping interaction followup."));
    json_t *wa = json_array(); json_append(wa, json_string(first));
    json_set(a, "warn_args", wa);
    json_t *deferred = da15_call(ctx, "defer_unless_expired", a);
    if (da15_is_raise(deferred)) { free(first); return deferred; }
    a = json_object();
    json_set(a, "interaction", da15_ixproj(ix));
    json_set(a, "command_text", json_string(ct));
    json_t *ev = da15_call(ctx, "build_slash_event", a);
    if (da15_is_raise(ev)) { json_free(deferred); free(first); return ev; }
    a = json_object(); json_set(a, "event", json_copy(ev)); json_free(ev);
    json_t *hm = da15_call(ctx, "handle_message", a);
    if (da15_is_raise(hm)) { json_free(deferred); free(first); return hm; }
    json_free(hm);
    bool d = da15_truthy(deferred); json_free(deferred);
    if (!d) { free(first); return json_null(); }
    const json_t *fu = da15_arg(args, "followup_msg");
    json_t *cleanup = NULL;
    if (da15_truthy(fu)) {
        a = json_object(); json_set(a, "key", da15_copyv(fu));
        json_t *tx = da15_call(ctx, "t", a);
        if (da15_is_raise(tx)) cleanup = tx;
        else {
            a = json_object(); json_set(a, "content", json_copy(tx)); json_free(tx);
            cleanup = da15_call(ctx, "edit_original_response", a);
        }
    } else {
        cleanup = da15_call(ctx, "delete_original_response", json_object());
    }
    if (da15_is_raise(cleanup)) {
        if (!da15_is_exception(da15_raise_kind(cleanup))) { free(first); return cleanup; }
        json_t *ar = json_array();
        json_append(ar, json_string("Discord interaction cleanup failed: %s"));
        json_append(ar, json_copy(cleanup));
        json_free(cleanup);
        json_t *x = da15_logcall(ctx, "logger_debug", ar, false);
        if (da15_is_raise(x)) { free(first); return x; }
        json_free(x);
    } else json_free(cleanup);
    free(first);
    return json_null();
}

/* helper: desired/existing key from payload; returns 2-element [num, lower-name] */
static json_t *da15_cmd_key(const json_t *payload) {
    json_t *key = json_array();
    const json_t *ty = (payload && payload->type == JSON_OBJECT) ? json_obj_get(payload, "type") : NULL;
    json_t *tn = da15_py_int(da15_is_null(ty) || !da15_truthy(ty) ? json_number(1) : ty);
    json_append(key, tn);
    const json_t *nm = (payload && payload->type == JSON_OBJECT) ? json_obj_get(payload, "name") : NULL;
    if (da15_str(nm)) { char *lo = da15_lower(nm->str_val); json_append(key, json_string(lo)); free(lo); }
    else json_append(key, json_string(""));
    return key;
}

/* PoP: da15_safe_sync_slash_commands @ plugins/platforms/discord/adapter.py:_safe_sync_slash_commands */
static json_t *da15_safe_sync_slash_commands(da15_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *summary = json_object();
    json_set(summary, "total", json_number(0));
    json_set(summary, "unchanged", json_number(0));
    json_set(summary, "updated", json_number(0));
    json_set(summary, "recreated", json_number(0));
    json_set(summary, "created", json_number(0));
    json_set(summary, "deleted", json_number(0));
    const json_t *client = da15_state_get(ctx, "_client");
    if (da15_is_null(client)) return summary;
    const json_t *appv = da15_getattr(client, "application_id");
    if (da15_is_null(appv) || !da15_truthy(appv)) appv = da15_getattr(da15_getattr(client, "user"), "id");
    if (da15_is_null(appv) || !da15_truthy(appv)) {
        json_free(summary);
        return da15_err("RuntimeError", "Discord application ID is unavailable for slash command sync");
    }
    json_t *r = da15_call(ctx, "tree_get_commands", json_object());
    if (da15_is_raise(r)) { json_free(summary); return r; }
    json_t *desired = r;  /* array of {name, payload} */
    size_t ndes = (desired && desired->type == JSON_ARRAY) ? desired->c.count : 0;
    r = da15_call(ctx, "tree_fetch_commands", json_object());
    if (da15_is_raise(r)) { json_free(desired); json_free(summary); return r; }
    json_t *existing = r; /* array of {id,name,type} */
    size_t nex = (existing && existing->type == JSON_ARRAY) ? existing->c.count : 0;

    json_t *used_existing = json_array();
    for (size_t i = 0; i < nex; i++) json_append(used_existing, json_bool(false));
    int mutcount = 0;
    size_t deleted = 0, created = 0, unchanged = 0, recreated = 0, updated = 0;

    /* obsolete delete first */
    for (size_t i = 0; i < nex; i++) {
        const json_t *ex = existing->c.items[i];
        json_t *ek = da15_cmd_key(ex);
        bool found = false;
        for (size_t j = 0; j < ndes; j++) {
            const json_t *it = desired->c.items[j];
            const json_t *pl = (it && it->type == JSON_OBJECT) ? json_obj_get(it, "payload") : NULL;
            json_t *dk = da15_cmd_key(pl);
            if (da15_json_eq(ek, dk)) found = true;
            json_free(dk);
            if (found) break;
        }
        json_free(ek);
        if (!found) {
            if (mutcount > 0) {
                json_t *s = da15_call(ctx, "sleep_between_mutations", json_object());
                if (da15_is_raise(s)) { json_free(desired); json_free(existing); json_free(used_existing); json_free(summary); return s; }
                json_free(s);
            }
            json_t *a = json_object(); json_set(a, "app_id", da15_copyv(appv));
            json_set(a, "command_id", da15_copyv(da15_getattr(ex, "id")));
            json_t *x = da15_call(ctx, "delete_global_command", a);
            if (da15_is_raise(x)) { json_free(desired); json_free(existing); json_free(used_existing); json_free(summary); return x; }
            json_free(x);
            json_array_set(used_existing, i, json_bool(true));
            deleted++; mutcount++;
        }
    }
    for (size_t j = 0; j < ndes; j++) {
        const json_t *it = desired->c.items[j];
        const json_t *pl = (it && it->type == JSON_OBJECT) ? json_obj_get(it, "payload") : NULL;
        json_t *dk = da15_cmd_key(pl);
        size_t found = nex;
        for (size_t i = 0; i < nex; i++) {
            if (da15_truthy(json_get(used_existing, i))) continue;
            json_t *ek = da15_cmd_key(existing->c.items[i]);
            bool eq = da15_json_eq(ek, dk); json_free(ek);
            if (eq) { found = i; break; }
        }
        json_free(dk);
        if (found == nex) {
            if (mutcount > 0) {
                json_t *s = da15_call(ctx, "sleep_between_mutations", json_object());
                if (da15_is_raise(s)) { json_free(desired); json_free(existing); json_free(used_existing); json_free(summary); return s; }
                json_free(s);
            }
            json_t *a = json_object(); json_set(a, "app_id", da15_copyv(appv));
            json_set(a, "desired", da15_copyv(pl));
            json_t *x = da15_call(ctx, "upsert_global_command", a);
            if (da15_is_raise(x)) { json_free(desired); json_free(existing); json_free(used_existing); json_free(summary); return x; }
            json_free(x);
            created++; mutcount++;
            continue;
        }
        json_array_set(used_existing, found, json_bool(true));
        json_t *a = json_object(); json_set(a, "command", da15_copyv(existing->c.items[found]));
        json_t *ce = da15_call(ctx, "existing_command_to_payload", a);
        if (da15_is_raise(ce)) { json_free(desired); json_free(existing); json_free(used_existing); json_free(summary); return ce; }
        a = json_object(); json_set(a, "payload", json_copy(ce));
        json_t *cp = da15_call(ctx, "canonicalize_payload", a);
        if (da15_is_raise(cp)) { json_free(ce); json_free(desired); json_free(existing); json_free(used_existing); json_free(summary); return cp; }
        a = json_object(); json_set(a, "payload", da15_copyv(pl));
        json_t *dp = da15_call(ctx, "canonicalize_payload", a);
        if (da15_is_raise(dp)) { json_free(ce); json_free(cp); json_free(desired); json_free(existing); json_free(used_existing); json_free(summary); return dp; }
        if (da15_json_eq(cp, dp)) {
            json_free(ce); json_free(cp); json_free(dp);
            unchanged++; continue;
        }
        a = json_object(); json_set(a, "payload", json_copy(ce));
        json_t *pa = da15_call(ctx, "patchable_payload", a);
        if (da15_is_raise(pa)) { json_free(ce); json_free(cp); json_free(dp); json_free(desired); json_free(existing); json_free(used_existing); json_free(summary); return pa; }
        a = json_object(); json_set(a, "payload", da15_copyv(pl));
        json_t *pb = da15_call(ctx, "patchable_payload", a);
        if (da15_is_raise(pb)) { json_free(ce); json_free(cp); json_free(dp); json_free(pa); json_free(desired); json_free(existing); json_free(used_existing); json_free(summary); return pb; }
        bool patch_eq = da15_json_eq(pa, pb);
        json_free(ce); json_free(cp); json_free(dp); json_free(pa); json_free(pb);
        if (mutcount > 0) {
            json_t *s = da15_call(ctx, "sleep_between_mutations", json_object());
            if (da15_is_raise(s)) { json_free(desired); json_free(existing); json_free(used_existing); json_free(summary); return s; }
            json_free(s);
        }
        a = json_object(); json_set(a, "app_id", da15_copyv(appv));
        if (patch_eq) {
            json_set(a, "desired", da15_copyv(pl));
            json_t *x = da15_call(ctx, "upsert_global_command", a);
            if (da15_is_raise(x)) { json_free(desired); json_free(existing); json_free(used_existing); json_free(summary); return x; }
            json_free(x);
            recreated++;
        } else {
            json_set(a, "command_id", da15_copyv(da15_getattr(existing->c.items[found], "id")));
            json_set(a, "desired", da15_copyv(pl));
            json_t *x = da15_call(ctx, "edit_global_command", a);
            if (da15_is_raise(x)) { json_free(desired); json_free(existing); json_free(used_existing); json_free(summary); return x; }
            json_free(x);
            updated++;
        }
        mutcount++;
    }
    json_set(summary, "total", json_number((double)ndes));
    json_set(summary, "deleted", json_number((double)deleted));
    json_set(summary, "created", json_number((double)created));
    json_set(summary, "unchanged", json_number((double)unchanged));
    json_set(summary, "recreated", json_number((double)recreated));
    json_set(summary, "updated", json_number((double)updated));
    json_free(desired); json_free(existing); json_free(used_existing);
    return summary;
}

/* outer exception handler for post-connect; returns the final json */
static json_t *da15_pc_outer(da15_ctx_t *ctx, const json_t *e, const char *name) {
    const char *kind = da15_raise_kind(e);
    if (da15_chain_has(kind, "TimeoutError")) {
        json_t *ar = json_array();
        json_append(ar, json_string("[%s] Slash command sync timed out — Discord rate-limit bucket may be saturated; will retry on next reconnect"));
        json_append(ar, json_string(name ? name : "bot"));
        json_t *r = da15_logcall(ctx, "logger_warning", ar, false);
        json_free((json_t *)e);
        return r;
    }
    if (da15_chain_has(kind, "CancelledError")) return (json_t *)e;
    json_t *ar = json_array();
    json_append(ar, json_string("[%s] Slash command sync failed: %s"));
    json_append(ar, json_string(name ? name : "bot"));
    json_append(ar, json_copy(e));
    json_free((json_t *)e);
    return da15_logcall(ctx, "logger_warning", ar, true);
}

/* PoP: da15_run_post_connect_initialization @ plugins/platforms/discord/adapter.py:_run_post_connect_initialization */
static json_t *da15_run_post_connect_initialization(da15_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *client = da15_state_get(ctx, "_client");
    if (da15_is_null(client)) return json_null();
    const char *name = da15_arg_str(args, "name", "bot");
    json_t *policy = da15_call(ctx, "command_sync_policy", json_object());
    if (da15_is_raise(policy)) return da15_pc_outer(ctx, policy, name);
    const char *ps = da15_str(policy);
    bool off = ps && strcmp(ps, "off") == 0;
    bool bulk = ps && strcmp(ps, "bulk") == 0;
    json_free(policy);
    if (off) {
        json_t *ar = json_array();
        json_append(ar, json_string("[%s] Skipping Discord slash command sync (policy=off)"));
        json_append(ar, json_string(name));
        return da15_logcall(ctx, "logger_info", ar, false);
    }
    if (bulk) {
        json_t *a = json_object(); json_set(a, "timeout", json_number(30));
        json_t *w = da15_call(ctx, "wait_for", a);
        if (da15_is_raise(w)) return da15_pc_outer(ctx, w, name);
        json_free(w);
        json_t *sy = da15_call(ctx, "tree_sync", json_object());
        if (da15_is_raise(sy)) return da15_pc_outer(ctx, sy, name);
        int n = 0;
        if (sy && sy->type == JSON_ARRAY) n = (int)sy->c.count;
        else if (sy && sy->type == JSON_OBJECT) n = (int)sy->c.count;
        else if (sy && sy->type == JSON_STRING) n = (int)strlen(sy->str_val);
        json_free(sy);
        json_t *ar = json_array();
        json_append(ar, json_string("[%s] Synced %d slash command(s) via bulk tree sync"));
        json_append(ar, json_string(name));
        json_append(ar, json_number(n));
        return da15_logcall(ctx, "logger_info", ar, false);
    }
    const json_t *appv = da15_getattr(client, "application_id");
    if (da15_is_null(appv) || !da15_truthy(appv)) appv = da15_getattr(da15_getattr(client, "user"), "id");
    json_t *a = json_object(); json_set(a, "app_id", da15_copyv(appv));
    json_t *fp = da15_call(ctx, "desired_sync_fingerprint", json_object());
    if (da15_is_raise(fp)) return da15_pc_outer(ctx, fp, name);
    json_set(a, "fingerprint", json_copy(fp));
    json_t *skip = da15_call(ctx, "command_sync_skip_reason", a);
    if (da15_is_raise(skip)) { json_free(fp); return da15_pc_outer(ctx, skip, name); }
    if (da15_truthy(skip)) {
        json_t *ar = json_array();
        json_append(ar, json_string("[%s] Skipping Discord slash command sync: %s"));
        json_append(ar, json_string(name));
        json_append(ar, json_copy(skip));
        json_free(skip); json_free(fp);
        return da15_logcall(ctx, "logger_info", ar, false);
    }
    json_free(skip);
    json_t *att = json_object(); json_set(att, "app_id", da15_copyv(appv)); json_set(att, "fingerprint", json_copy(fp));
    json_t *ra1 = da15_call(ctx, "record_command_sync_attempt", att);
    if (da15_is_raise(ra1)) { json_free(fp); return da15_pc_outer(ctx, ra1, name); }
    json_free(ra1);
    /* http ratelimit attr juggling (no seams) */
    json_t *wargs = json_object(); json_set(wargs, "timeout", json_number(600));
    json_t *w = da15_call(ctx, "wait_for", wargs);
    if (da15_is_raise(w)) {
        json_t *e = w;
        if (!da15_is_exception(da15_raise_kind(e))) { json_free(fp); return da15_pc_outer(ctx, e, name); }
        json_t *a2 = json_object(); json_set(a2, "error", json_copy(e));
        json_t *isrl = da15_call(ctx, "is_discord_rate_limit", a2);
        if (da15_is_raise(isrl)) { json_free(e); json_free(fp); return da15_pc_outer(ctx, isrl, name); }
        bool rl = da15_truthy(isrl); json_free(isrl);
        if (!rl) { json_free(fp); return da15_pc_outer(ctx, e, name); }
        json_t *a2b = json_object(); json_set(a2b, "error", json_copy(e));
        json_t *re = da15_call(ctx, "extract_retry_after", a2b);
        if (da15_is_raise(re)) { json_free(e); json_free(fp); return da15_pc_outer(ctx, re, name); }
        json_t *retry = re;
        if (da15_is_null(retry)) { json_free(retry); retry = json_number(DA15_RATE_SLEEP); }
        json_t *a3 = json_object(); json_set(a3, "app_id", da15_copyv(appv));
        json_set(a3, "fingerprint", json_copy(fp)); json_set(a3, "retry_after", json_copy(retry));
        json_t *rr = da15_call(ctx, "record_command_sync_rate_limit", a3);
        if (da15_is_raise(rr)) { json_free(retry); json_free(e); json_free(fp); return da15_pc_outer(ctx, rr, name); }
        json_free(rr);
        json_t *ar = json_array();
        json_append(ar, json_string("[%s] Discord rate-limited slash command sync; retrying after %.0fs"));
        json_append(ar, json_string(name));
        json_append(ar, json_copy(retry));
        json_free(retry); json_free(e); json_free(fp);
        return da15_logcall(ctx, "logger_warning", ar, false);
    }
    json_free(w);
    json_t *summary = da15_call(ctx, "safe_sync", json_object());
    if (da15_is_raise(summary)) {
        json_t *e = summary;
        if (!da15_is_exception(da15_raise_kind(e))) { json_free(fp); return da15_pc_outer(ctx, e, name); }
        json_t *a2c = json_object(); json_set(a2c, "error", json_copy(e));
        json_t *isrl2 = da15_call(ctx, "is_discord_rate_limit", a2c);
        if (da15_is_raise(isrl2)) { json_free(e); json_free(fp); return da15_pc_outer(ctx, isrl2, name); }
        bool rl2 = da15_truthy(isrl2); json_free(isrl2);
        if (!rl2) { json_free(fp); return da15_pc_outer(ctx, e, name); }
        json_t *a2d = json_object(); json_set(a2d, "error", json_copy(e));
        json_t *re2 = da15_call(ctx, "extract_retry_after", a2d);
        if (da15_is_raise(re2)) { json_free(e); json_free(fp); return da15_pc_outer(ctx, re2, name); }
        json_t *retry2 = re2;
        if (da15_is_null(retry2)) { json_free(retry2); retry2 = json_number(DA15_RATE_SLEEP); }
        json_t *a3b = json_object(); json_set(a3b, "app_id", da15_copyv(appv));
        json_set(a3b, "fingerprint", json_copy(fp)); json_set(a3b, "retry_after", json_copy(retry2));
        json_t *rr2 = da15_call(ctx, "record_command_sync_rate_limit", a3b);
        if (da15_is_raise(rr2)) { json_free(retry2); json_free(e); json_free(fp); return da15_pc_outer(ctx, rr2, name); }
        json_free(rr2);
        json_t *ar2 = json_array();
        json_append(ar2, json_string("[%s] Discord rate-limited slash command sync; retrying after %.0fs"));
        json_append(ar2, json_string(name));
        json_append(ar2, json_copy(retry2));
        json_free(retry2); json_free(e); json_free(fp);
        return da15_logcall(ctx, "logger_warning", ar2, false);
    }
    json_t *a4 = json_object(); json_set(a4, "app_id", da15_copyv(appv));
    json_set(a4, "fingerprint", json_copy(fp));
    json_set(a4, "summary", json_copy(summary));
    json_t *scv = da15_call(ctx, "record_command_sync_success", a4);
    if (da15_is_raise(scv)) { json_free(summary); json_free(fp); return da15_pc_outer(ctx, scv, name); }
    json_free(scv);
    json_t *ar = json_array();
    json_append(ar, json_string("[%s] Safely reconciled %d slash command(s): unchanged=%d updated=%d recreated=%d created=%d deleted=%d"));
    json_append(ar, json_string(name));
    const char *keys[] = {"total", "unchanged", "updated", "recreated", "created", "deleted"};
    double nums[6];
    for (int i = 0; i < 6; i++) nums[i] = da15_num(json_obj_get(summary, keys[i]));
    json_free(summary); json_free(fp);
    for (int i = 0; i < 6; i++) json_append(ar, json_number(nums[i]));
    return da15_logcall(ctx, "logger_info", ar, false);
}

/* outer exception handler for missed-message backfill */
static json_t *da15_mmb_outer(da15_ctx_t *ctx, const json_t *e, const char *sid,
                              const json_t *counts, const char *name) {
    const char *kind = da15_raise_kind(e);
    if (da15_chain_has(kind, "CancelledError")) {
        json_t *a = json_object(); json_set(a, "scan_id", json_string(sid));
        json_set(a, "status", json_string("cancelled")); json_set(a, "counts", json_copy((json_t *)counts));
        json_set(a, "error", json_null());
        json_t *r = da15_call(ctx, "finish_recovery_scan", a);
        json_t *ecopy = json_copy(e);
        json_free((json_t *)e);
        if (da15_is_raise(r)) { json_free(ecopy); return r; }
        json_free(r); return ecopy;
    }
    if (da15_is_exception(kind)) {
        char *estr = da15_strdup(json_get_str(e, "message", ""));
        json_t *a = json_object(); json_set(a, "scan_id", json_string(sid));
        json_set(a, "status", json_string("failed")); json_set(a, "counts", json_copy((json_t *)counts));
        json_set(a, "error", json_string(estr));
        json_t *r = da15_call(ctx, "finish_recovery_scan", a);
        if (da15_is_raise(r)) { free(estr); json_free((json_t *)e); return r; }
        json_free(r);
        json_t *ar = json_array();
        json_append(ar, json_string("[%s] Missed-message backfill failed: %s"));
        json_append(ar, json_string(name));
        json_append(ar, json_copy(e));
        free(estr); json_free((json_t *)e);
        return da15_logcall(ctx, "logger_warning", ar, true);
    }
    return (json_t *)e;
}

/* PoP: da15_run_missed_message_backfill @ plugins/platforms/discord/adapter.py:_run_missed_message_backfill */
static json_t *da15_run_missed_message_backfill(da15_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *client = da15_state_get(ctx, "_client");
    if (da15_is_null(client)) return json_null();
    const char *name = da15_arg_str(args, "name", "bot");
    json_t *channels = da15_call(ctx, "backfill_channels", json_object());
    if (da15_is_raise(channels)) return channels;
    json_t *ledger = da15_call(ctx, "ledger_db", json_object());
    if (da15_is_raise(ledger)) { json_free(channels); return ledger; }
    bool lok = da15_truthy(ledger); json_free(ledger);
    if (!lok) {
        json_t *ar = json_array();
        json_append(ar, json_string("[%s] Missed-message recovery aborted: durable ledger unavailable"));
        json_append(ar, json_string(name));
        json_free(channels);
        return da15_logcall(ctx, "logger_error", ar, false);
    }
    json_t *a = json_object(); json_set(a, "channels", json_copy(channels));
    json_t *sidv = da15_call(ctx, "record_recovery_scan_start", a);
    if (da15_is_raise(sidv)) { json_free(channels); return sidv; }
    const char *sid = da15_str(sidv) ? da15_str(sidv) : da15_py_str(sidv);
    size_t nch = (channels && channels->type == JSON_ARRAY) ? channels->c.count : 0;
    if (nch == 0) {
        json_t *ar = json_array();
        json_append(ar, json_string("[%s] Missed-message backfill enabled but no channels configured"));
        json_append(ar, json_string(name));
        json_t *lg = da15_logcall(ctx, "logger_info", ar, false);
        if (da15_is_raise(lg)) { json_free(channels); json_free(sidv); return lg; }
        json_free(lg);
        a = json_object(); json_set(a, "scan_id", json_copy(sidv));
        json_set(a, "status", json_string("skipped"));
        json_t *cnt = json_object(); json_set(cnt, "scanned", json_number(0));
        json_set(cnt, "missed", json_number(0)); json_set(cnt, "dispatched", json_number(0));
        json_set(a, "counts", cnt); json_set(a, "error", json_null());
        json_t *r = da15_call(ctx, "finish_recovery_scan", a);
        json_free(channels); json_free(sidv);
        return r;
    }
    json_t *mdv = da15_call(ctx, "max_dispatches", json_object());
    if (da15_is_raise(mdv)) { json_free(channels); json_free(sidv); return mdv; }
    double maxd = da15_num(mdv); json_free(mdv);
    json_t *counts = json_object(); json_set(counts, "scanned", json_number(0));
    json_set(counts, "missed", json_number(0)); json_set(counts, "dispatched", json_number(0));
    a = json_object(); json_set(a, "channels", json_copy(channels));
    json_t *cands = da15_call(ctx, "iter_backfill_candidates", a);
    json_free(channels);
    if (da15_is_raise(cands)) {
        json_t *out = da15_mmb_outer(ctx, cands, sid, counts, name);
        json_free(sidv); return out;
    }
    size_t nmsg = (cands && cands->type == JSON_ARRAY) ? cands->c.count : 0;
    for (size_t i = 0; i < nmsg; i++) {
        const json_t *msg = cands->c.items[i];
        json_set(counts, "scanned", json_number(da15_num(json_obj_get(counts, "scanned")) + 1));
        char *mid = da15_id_str(msg, "id");
        a = json_object(); json_set(a, "message", da15_copyv(msg)); json_set(a, "status", json_string("discovered"));
        json_t *r = da15_call(ctx, "record_discord_message_seen", a);
        if (da15_is_raise(r)) { free(mid); json_free(cands); json_free(sidv); return da15_mmb_outer(ctx, r, sid, counts, name); }
        json_free(r);
        a = json_object(); json_set(a, "message_id", json_string(mid));
        r = da15_call(ctx, "dedup_contains", a);
        if (da15_is_raise(r)) { free(mid); json_free(cands); json_free(sidv); return da15_mmb_outer(ctx, r, sid, counts, name); }
        bool dup = da15_truthy(r); json_free(r);
        if (dup) { free(mid); continue; }
        a = json_object(); json_set(a, "message", da15_copyv(msg));
        r = da15_call(ctx, "should_backfill", a);
        if (da15_is_raise(r)) { free(mid); json_free(cands); json_free(sidv); return da15_mmb_outer(ctx, r, sid, counts, name); }
        bool need = da15_truthy(r); json_free(r);
        if (!need) { free(mid); continue; }
        json_set(counts, "missed", json_number(da15_num(json_obj_get(counts, "missed")) + 1));
        char *chanid = da15_id_str(da15_getattr(msg, "channel"), "id");
        if (chanid[0] == '\0') { free(chanid); chanid = da15_strdup("unknown"); }
        json_t *ar = json_array();
        json_append(ar, json_string("[%s] Backfilling missed Discord message %s in channel %s"));
        json_append(ar, json_string(name));
        json_append(ar, json_copy(da15_getattr(msg, "id")));
        json_append(ar, json_string(chanid));
        json_t *lg = da15_logcall(ctx, "logger_info", ar, false);
        free(chanid);
        if (da15_is_raise(lg)) { free(mid); json_free(cands); json_free(sidv); return da15_mmb_outer(ctx, lg, sid, counts, name); }
        json_free(lg);
        a = json_object(); json_set(a, "message", da15_copyv(msg));
        json_set(a, "status", json_string("queued")); json_set(a, "error", json_null());
        r = da15_call(ctx, "record_recovery_attempt", a);
        if (da15_is_raise(r)) { free(mid); json_free(cands); json_free(sidv); return da15_mmb_outer(ctx, r, sid, counts, name); }
        json_free(r);
        a = json_object(); json_set(a, "message", da15_copyv(msg));
        json_t *disp = da15_call(ctx, "dispatch_recovered", a);
        if (da15_is_raise(disp)) {
            char *estr = da15_strdup(json_get_str(disp, "message", ""));
            a = json_object(); json_set(a, "message_id", json_string(mid));
            json_t *dd = da15_call(ctx, "dedup_discard", a);
            if (da15_is_raise(dd)) { free(estr); free(mid); json_free(disp); json_free(cands); json_free(sidv); return dd; }
            json_free(dd);
            a = json_object(); json_set(a, "message", da15_copyv(msg));
            json_set(a, "status", json_string("failed")); json_set(a, "error", json_string(estr));
            json_t *rr = da15_call(ctx, "record_recovery_attempt", a);
            free(estr);
            if (da15_is_raise(rr)) { free(mid); json_free(disp); json_free(cands); json_free(sidv); return rr; }
            json_free(rr);
            free(mid); json_free(cands); json_free(sidv);
            return da15_mmb_outer(ctx, disp, sid, counts, name);
        }
        bool admitted = da15_truthy(disp); json_free(disp);
        if (admitted) json_set(counts, "dispatched", json_number(da15_num(json_obj_get(counts, "dispatched")) + 1));
        free(mid);
        if (da15_num(json_obj_get(counts, "dispatched")) >= maxd) break;
    }
    json_free(cands);
    a = json_object(); json_set(a, "scan_id", json_copy(sidv));
    json_set(a, "status", json_string("success")); json_set(a, "counts", json_copy(counts));
    json_set(a, "error", json_null());
    json_t *r = da15_call(ctx, "finish_recovery_scan", a);
    if (da15_is_raise(r)) { json_free(sidv); return r; }
    json_free(r);
    json_t *ar = json_array();
    json_append(ar, json_string("[%s] Missed-message backfill complete: scanned=%d missed=%d dispatched=%d"));
    json_append(ar, json_string(name));
    json_append(ar, json_number(da15_num(json_obj_get(counts, "scanned"))));
    json_append(ar, json_number(da15_num(json_obj_get(counts, "missed"))));
    json_append(ar, json_number(da15_num(json_obj_get(counts, "dispatched"))));
    json_t *lg = da15_logcall(ctx, "logger_info", ar, false);
    json_free(sidv);
    return lg;
}

/* PoP: da15_resolve_allowed_usernames @ plugins/platforms/discord/adapter.py:_resolve_allowed_usernames */
static json_t *da15_resolve_allowed_usernames(da15_ctx_t *ctx, const json_t *args) {
    const char *name = da15_arg_str(args, "name", "bot");
    const json_t *prior = da15_state_get(ctx, "_username_to_id");
    const json_t *entries = da15_arg(args, "allowed_user_ids");
    size_t nent = (entries && entries->type == JSON_ARRAY) ? entries->c.count : 0;
    json_t *umap = json_object();
    if (prior && prior->type == JSON_OBJECT)
        for (size_t i = 0; i < prior->c.count; i++) {
            const json_t *v = prior->c.items[i];
            if (v && v->type == JSON_STRING && da15_arr_has(entries, v->str_val))
                json_set(umap, prior->c.keys[i], json_copy(v));
        }
    da15_state_set(ctx, "_username_to_id", json_copy(umap));
    json_t *resolved = json_array();
    for (size_t i = 0; i < umap->c.count; i++)
        da15_arr_add_unique(resolved, da15_str(umap->c.items[i]) ? da15_str(umap->c.items[i]) : "");
    json_t *rs = da15_sorted_strings(resolved);
    da15_state_set(ctx, "_username_resolved_ids", json_copy(rs));
    const json_t *client = da15_state_get(ctx, "_client");
    if (nent == 0 || da15_is_null(client)) { json_free(umap); json_free(rs); return json_null(); }
    json_t *numeric = json_array();
    json_t *to_resolve = json_array();
    for (size_t i = 0; i < nent; i++) {
        const char *e = da15_str(entries->c.items[i]) ? da15_str(entries->c.items[i]) : "";
        if (da15_isdigit(e) || strcmp(e, "*") == 0) da15_arr_add_unique(numeric, e);
        else { char *lo = da15_lower(e); da15_arr_add_unique(to_resolve, lo); free(lo); }
    }
    if (to_resolve->c.count == 0) { json_free(umap); json_free(rs); json_free(numeric); json_free(to_resolve); return json_null(); }
    char *joined = da15_fmt("%.*s", 0, "");
    size_t jcap = 1;
    for (size_t i = 0; i < to_resolve->c.count; i++) jcap += strlen(to_resolve->c.items[i]->str_val) + 2;
    joined = malloc(jcap); joined[0] = '\0';
    for (size_t i = 0; i < to_resolve->c.count; i++) {
        if (i) strcat(joined, ", ");
        strcat(joined, to_resolve->c.items[i]->str_val);
    }
    char *txt = da15_fmt("[%s] Resolving %zu username(s): %s", name, to_resolve->c.count, joined);
    json_t *a = json_object(); json_set(a, "text", json_string(txt)); free(txt);
    json_t *pr = da15_call(ctx, "au_print", a);
    if (da15_is_raise(pr)) { free(joined); json_free(umap); json_free(rs); json_free(numeric); json_free(to_resolve); return pr; }
    json_free(pr);
    free(joined);
    json_t *display_only = json_array();
    const json_t *guilds = da15_getattr(client, "guilds");
    size_t ng = (guilds && guilds->type == JSON_ARRAY) ? guilds->c.count : 0;
    for (size_t gi = 0; gi < ng && to_resolve->c.count; gi++) {
        const json_t *g = guilds->c.items[gi];
        const json_t *members = da15_getattr(g, "members");
        size_t nm = (members && members->type == JSON_ARRAY) ? members->c.count : 0;
        int mc = (int)da15_num(da15_getattr(g, "member_count"));
        if (nm < (size_t)mc) {
            a = json_object(); json_set(a, "guild", da15_copyv(da15_getattr(g, "name")));
            json_t *fm = da15_call(ctx, "au_fetch_members", a);
            if (da15_is_raise(fm)) {
                json_t *ar = json_array();
                json_append(ar, json_string("Failed to fetch members for guild %s: %s"));
                json_append(ar, da15_copyv(da15_getattr(g, "name")));
                json_append(ar, json_copy(fm));
                json_t *lg = da15_logcall(ctx, "logger_warning", ar, false);
                if (da15_is_raise(lg)) { json_free(lg); json_free(fm); json_free(umap); json_free(rs); json_free(numeric); json_free(to_resolve); json_free(display_only); return lg; }
                json_free(lg); json_free(fm);
                continue;
            }
            members = fm;
            nm = (members && members->type == JSON_ARRAY) ? members->c.count : 0;
        }
        for (size_t mi = 0; mi < nm; mi++) {
            const json_t *m = members->c.items[mi];
            char *lo = da15_lower(da15_str(da15_getattr(m, "name")) ? da15_str(da15_getattr(m, "name")) : "");
            if (da15_arr_has(to_resolve, lo)) {
                char *uid = da15_id_str(m, "id");
                da15_arr_add_unique(numeric, uid);
                da15_arr_add_unique(resolved, uid);
                json_set(umap, lo, json_string(uid));
                for (size_t k = 0; k < to_resolve->c.count; k++)
                    if (strcmp(to_resolve->c.items[k]->str_val, lo) == 0) {
                        json_array_remove(to_resolve, k); break;
                    }
                char *mtxt = da15_fmt("[%s] Resolved '%s' -> %s (%s#%s)", name, lo, uid,
                    da15_str(da15_getattr(m, "name")) ? da15_str(da15_getattr(m, "name")) : "",
                    da15_str(da15_getattr(m, "discriminator")) ? da15_str(da15_getattr(m, "discriminator")) : "");
                a = json_object(); json_set(a, "text", json_string(mtxt)); free(mtxt);
                json_t *p2 = da15_call(ctx, "au_print", a);
                if (da15_is_raise(p2)) { free(lo); free(uid); json_free(umap); json_free(rs); json_free(numeric); json_free(to_resolve); json_free(display_only); return p2; }
                json_free(p2);
                free(lo); free(uid);
                continue;
            }
            free(lo);
            const json_t *showns[2] = { da15_getattr(m, "display_name"), da15_getattr(m, "global_name") };
            for (int si = 0; si < 2; si++) {
                if (da15_truthy(showns[si])) {
                    char *sl = da15_lower(da15_str(showns[si]) ? da15_str(showns[si]) : "");
                    if (da15_arr_has(to_resolve, sl)) da15_arr_add_unique(display_only, sl);
                    free(sl);
                }
            }
        }
    }
    if (to_resolve->c.count) {
        jcap = 1;
        for (size_t i = 0; i < to_resolve->c.count; i++) jcap += strlen(to_resolve->c.items[i]->str_val) + 2;
        char *j2 = malloc(jcap); j2[0] = '\0';
        for (size_t i = 0; i < to_resolve->c.count; i++) {
            if (i) strcat(j2, ", ");
            strcat(j2, to_resolve->c.items[i]->str_val);
        }
        char *t2 = da15_fmt("[%s] Could not resolve usernames: %s", name, j2);
        a = json_object(); json_set(a, "text", json_string(t2)); free(t2);
        json_t *p3 = da15_call(ctx, "au_print", a);
        if (da15_is_raise(p3)) { free(j2); json_free(umap); json_free(rs); json_free(numeric); json_free(to_resolve); json_free(display_only); return p3; }
        json_free(p3);
        free(j2);
        json_t *both = json_array();
        for (size_t i = 0; i < to_resolve->c.count; i++)
            if (da15_arr_has(display_only, to_resolve->c.items[i]->str_val))
                json_append(both, json_copy(to_resolve->c.items[i]));
        json_t *sb = da15_sorted_strings(both); json_free(both);
        for (size_t i = 0; i < sb->c.count; i++) {
            char *t3 = da15_fmt("[%s] '%s' matches only a display name or server nickname, which any member can set; allowlist that member's username or numeric user ID instead", name, sb->c.items[i]->str_val);
            a = json_object(); json_set(a, "text", json_string(t3)); free(t3);
            json_t *p4 = da15_call(ctx, "au_print", a);
            if (da15_is_raise(p4)) { json_free(sb); json_free(umap); json_free(rs); json_free(numeric); json_free(to_resolve); json_free(display_only); return p4; }
            json_free(p4);
        }
        json_free(sb);
    }
    json_t *nsorted = da15_sorted_strings(numeric);
    da15_state_set(ctx, "_allowed_user_ids", json_copy(nsorted));
    const json_t *snap = da15_state_get(ctx, "_gate_env_snapshot");
    if (!da15_is_null(snap) && snap && snap->type == JSON_OBJECT) {
        char *v = da15_fmt("%.*s", 0, "");
        size_t cap = 1;
        for (size_t i = 0; i < nsorted->c.count; i++) cap += strlen(nsorted->c.items[i]->str_val) + 1;
        v = malloc(cap); v[0] = '\0';
        for (size_t i = 0; i < nsorted->c.count; i++) {
            if (i) strcat(v, ",");
            strcat(v, nsorted->c.items[i]->str_val);
        }
        json_set((json_t *)snap, "DISCORD_ALLOWED_USERS", json_string(v));
        free(v);
    }
    json_t *mux = da15_call(ctx, "multiplex_active", json_object());
    if (da15_is_raise(mux)) { json_free(nsorted); json_free(umap); json_free(rs); json_free(numeric); json_free(to_resolve); json_free(display_only); return mux; }
    bool single = !da15_truthy(mux); json_free(mux);
    if (single) {
        char *v = da15_fmt("%.*s", 0, "");
        size_t cap = 1;
        for (size_t i = 0; i < nsorted->c.count; i++) cap += strlen(nsorted->c.items[i]->str_val) + 1;
        v = malloc(cap); v[0] = '\0';
        for (size_t i = 0; i < nsorted->c.count; i++) {
            if (i) strcat(v, ",");
            strcat(v, nsorted->c.items[i]->str_val);
        }
        a = json_object();
        json_set(a, "key", json_string("DISCORD_ALLOWED_USERS"));
        json_set(a, "value", json_string(v));
        free(v);
        json_t *rep = da15_call(ctx, "replace_bridged_env", a);
        if (da15_is_raise(rep)) { json_free(nsorted); json_free(umap); json_free(rs); json_free(numeric); json_free(to_resolve); json_free(display_only); return rep; }
        json_free(rep);
    }
    json_free(nsorted);
    json_t *rs2 = da15_sorted_strings(resolved);
    if (rs2->c.count) {
        char *t4 = da15_fmt("[%s] Updated DISCORD_ALLOWED_USERS with %zu resolved ID(s)", name, rs2->c.count);
        a = json_object(); json_set(a, "text", json_string(t4)); free(t4);
        json_t *p5 = da15_call(ctx, "au_print", a);
        if (da15_is_raise(p5)) { json_free(rs2); json_free(umap); json_free(rs); json_free(numeric); json_free(to_resolve); json_free(display_only); return p5; }
        json_free(p5);
    }
    da15_state_set(ctx, "_username_to_id", json_copy(umap));
    json_t *final_res = da15_sorted_strings(resolved);
    da15_state_set(ctx, "_username_resolved_ids", json_copy(final_res));
    json_free(final_res);
    json_free(rs2); json_free(umap); json_free(rs); json_free(numeric);
    json_free(resolved); json_free(to_resolve); json_free(display_only);
    return json_null();
}
