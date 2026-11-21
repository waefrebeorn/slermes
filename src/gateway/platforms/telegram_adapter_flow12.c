/*
 * telegram_adapter_flow12.c - Telegram adapter FLOW part 12 of
 * plugins/platforms/telegram/adapter.py (hermes-agent): a coherent batch of
 * UNCLAIMED defs left after telegram_adapter_flow{,2,3,6,7,8,9,10,11}. Every
 * target below was confirmed UNCLAIMED; no flow 1-11 target is re-ported.
 *
 * Themes: send locks & lifecycle guards, callback claim, watchdog, task
 * collection, DM-topic creation, webhook teardown, media download/drain,
 * PTB retry-loop disarm, image compression and pure env/format helpers.
 * All live I/O and injected callables enter through ONE universal seam:
 * ta12_call(ctx,name,args).
 *
 * Self-contained: harness #includes this .c directly; libjson only.
 * Every symbol is static and ta12_-prefixed.
 *
 * Representation contract (shared with tests/oracle_telegram_adapter_flow12.py):
 * - NULL json_t* == Python None / absent; JSON_NULL is a real null value.
 * - Seam scripts: a plain value repeats; a LIST is consumed per call (last
 *   repeats); {"__script__":true,"ret":v,"raise":{type,message}} forces a
 *   result or raise. Missing/exhausted script => fail-closed RuntimeError
 *   recorded in the trace before propagation (BaseException passthrough).
 * - Except sites swallow ONLY Exception-family kinds (TA12_EXC_HIER);
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

#define _UPDATER_START_TIMEOUT 30.0
#define _POLLING_STALL_TIMEOUT 150.0
#define _IMG_COMPRESS_THRESHOLD_BYTES 1048576.0

typedef struct ta12_ctx {
    json_t *state;        /* owned; carries the attribute-double fields */
    const json_t *io;     /* borrowed scripted seam results */
    json_t *cursors;      /* owned; per-name array cursors */
    json_t *trace;        /* owned; ordered seam events */
    const json_t *errors; /* borrowed exception-graph registry (unused placeholder) */
} ta12_ctx_t;

/* ── basic helpers ── */
static bool ta12_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *ta12_raise_kind(const json_t *v) {
    return ta12_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static bool ta12_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool ta12_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *ta12_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *ta12_copyv(const json_t *v) { return v ? json_copy((json_t *)v) : json_null(); }
static json_t *ta12_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static const json_t *ta12_arg(const json_t *args, const char *key) {
    return args ? json_obj_get(args, key) : NULL;
}
static const json_t *ta12_getattr(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
static const json_t *ta12_state_get(const ta12_ctx_t *ctx, const char *key) {
    return (ctx && ctx->state && ctx->state->type == JSON_OBJECT)
        ? json_obj_get(ctx->state, key) : NULL;
}
static double ta12_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static bool ta12_has(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}
static bool ta12_json_eq(const json_t *a, const json_t *b) {
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
                if (!ta12_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                if (!a->c.keys[i] || !b->c.keys[i] || strcmp(a->c.keys[i], b->c.keys[i]) != 0)
                    return false;
                if (!ta12_json_eq(a->c.items[i], b->c.items[i])) return false;
            }
            return true;
        default: return false;
    }
}

/* ── string helpers ── */
static char *ta12_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static char *ta12_lower(const char *s) {
    char *o = ta12_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static char *ta12_fmt(const char *fmt, ...) {
    char tmp[1200]; va_list ap;
    va_start(ap, fmt); int n = vsnprintf(tmp, sizeof tmp, fmt, ap); va_end(ap);
    if (n < 0) return ta12_strdup("");
    char *o = malloc((size_t)n + 1);
    if (!o) return ta12_strdup("");
    va_start(ap, fmt); vsnprintf(o, (size_t)n + 1, fmt, ap); va_end(ap);
    return o;
}
static char *ta12_strip(const char *s) {
    if (!s) return ta12_strdup("");
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n-1])) n--;
    char *o = malloc(n + 1);
    if (o) { memcpy(o, s, n); o[n] = '\0'; }
    return o;
}
/* Python str() of a JSON scalar; integral doubles render as ints. */
static char *ta12_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return ta12_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return ta12_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e16) return ta12_fmt("%.0f", d);
            return ta12_fmt("%.17g", d);
        }
        case JSON_STRING: return ta12_strdup(v->str_val ? v->str_val : "");
        default: { char *s = json_serialize((json_t *)v); return s ? s : ta12_strdup(""); }
    }
}
/* os.path.splitext ext incl dot, lowercased */
static char *ta12_splitext_lower(const char *p) {
    if (!p) return ta12_strdup("");
    const char *base = strrchr(p, '/'); base = base ? base + 1 : p;
    const char *dot = strrchr(base, '.');
    if (!dot || dot == base) return ta12_strdup("");
    return ta12_lower(dot);
}
static char *ta12_basename(const char *p) {
    if (!p || !*p) return ta12_strdup("");
    const char *slash = strrchr(p, '/');
    return ta12_strdup(slash ? slash + 1 : p);
}

/* ── FULL builtin exception hierarchy (CPython 3.14) ── */
typedef struct { const char *name; const char *parent; } ta12_exc_row_t;
static const ta12_exc_row_t TA12_EXC_HIER[] = {
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
static const char *ta12_builtin_parent(const char *name) {
    for (size_t i = 0; i < sizeof(TA12_EXC_HIER) / sizeof(TA12_EXC_HIER[0]); i++)
        if (strcmp(name, TA12_EXC_HIER[i].name) == 0) return TA12_EXC_HIER[i].parent;
    return NULL;
}
static bool ta12_chain_has(const char *start, const char *target) {
    const char *cur = start;
    for (int depth = 0; cur && depth < 24; depth++) {
        if (strcmp(cur, target) == 0) return true;
        cur = ta12_builtin_parent(cur);
    }
    return false;
}
static bool ta12_is_exception(const char *kind) {
    if (!kind) return false;
    if (ta12_builtin_parent(kind) == NULL) return true; /* custom = Exception subclass */
    return ta12_chain_has(kind, "Exception");
}

/* ── logger helper ── */
static json_t *ta12_logger_args(json_t *arr) {
    json_t *a = json_object();
    json_set(a, "args", arr ? arr : json_array());
    return a;
}
static json_t *ta12_logger_kw(json_t *arr) {
    json_t *a = json_object();
    json_set(a, "args", arr ? arr : json_array());
    json_t *kw = json_object();  /* empty kwargs */
    json_set(a, "kwargs", kw);
    return a;
}
static json_t *ta12_logger_exc(json_t *arr) {
    json_t *a = json_object();
    json_set(a, "args", arr ? arr : json_array());
    json_t *kw = json_object();
    json_set(kw, "exc_info", json_bool(true));
    json_set(a, "kwargs", kw);
    return a;
}

/* ── the one universal seam ──
 * args is ADOPTED into the trace event; return is a fresh owned node. */
static json_t *ta12_call(ta12_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ta12_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ta12_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = ta12_err(json_get_str(raise, "type", "RuntimeError"),
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

/* PoP: ta12_clear_reactions @ plugins/platforms/telegram/adapter.py:_clear_reactions */
static json_t *ta12_clear_reactions(ta12_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "chat_id", ta12_copyv(ta12_arg(args, "chat_id")));
    json_set(a, "message_id", ta12_copyv(ta12_arg(args, "message_id")));
    json_set(a, "reaction", json_null());
    json_t *r = ta12_call(ctx, "_set_reaction", a);
    return r; /* exception spec or the seam result */
}

/* PoP: ta12_clear_task_attrs_except @ plugins/platforms/telegram/adapter.py:_clear_task_attrs_except */
static json_t *ta12_clear_task_attrs_except(ta12_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *cur = ta12_arg(args, "current");
    const json_t *attrs = ta12_arg(args, "attrs");
    if (attrs && attrs->type == JSON_ARRAY) {
        for (size_t i = 0; i < attrs->c.count; i++) {
            const char *key = ta12_str(attrs->c.items[i]);
            if (!key) continue;
            const json_t *v = json_obj_get(ctx->state, key);
            if (!ta12_json_eq(v, cur)) json_set(ctx->state, key, json_null());
        }
    }
    return json_null();
}

/* PoP: ta12_consume_abandoned_task @ plugins/platforms/telegram/adapter.py:_consume_abandoned_task */
static json_t *ta12_consume_abandoned_task(ta12_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "task", ta12_copyv(ta12_arg(args, "task")));
    json_t *r = ta12_call(ctx, "task_exception", a);
    if (ta12_is_raise(r)) {
        const char *k = ta12_raise_kind(r);
        if (k && strcmp(k, "CancelledError") == 0) { json_free(r); return json_null(); }
        if (ta12_is_exception(k)) {
            json_t *arr = json_array();
            json_append(arr, json_string("Abandoned Telegram init task failed after timeout"));
            json_t *d = ta12_call(ctx, "logger_debug", ta12_logger_exc(arr));
            if (ta12_is_raise(d)) return d;
            json_free(d); json_free(r);
            return json_null();
        }
        return r; /* BaseException-only passthrough */
    }
    json_free(r);
    return json_null();
}

/* PoP: ta12_claim_callback_state @ plugins/platforms/telegram/adapter.py:_claim_callback_state */
static json_t *ta12_claim_callback_state(ta12_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "query", ta12_copyv(ta12_arg(args, "query")));
    json_set(a, "cb", ta12_copyv(ta12_arg(args, "cb")));
    json_set(a, "denial", ta12_copyv(ta12_arg(args, "denial")));
    json_t *auth = ta12_call(ctx, "_callback_authorized", a);
    if (ta12_is_raise(auth)) return auth;
    bool ok = ta12_truthy(auth);
    json_free(auth);
    if (!ok) return json_null();
    /* private copy: args is borrowed from the harness record and echoed back */
    json_t *st = ta12_copyv(ta12_getattr(args, "state"));
    const char *key = ta12_str(ta12_arg(args, "key"));
    bool pop = json_get_bool(args, "pop", true);
    json_t *val = (st && key) ? json_obj_get(st, key) : NULL;
    json_t *vcp = ta12_copyv(val);
    if (pop && val && key) json_obj_del(st, key);
    json_free(st);
    if (!ta12_truthy(vcp)) {
        json_t *b = json_object();
        json_set(b, "query", ta12_copyv(ta12_arg(args, "query")));
        json_set(b, "text", ta12_copyv(ta12_arg(args, "resolved")));
        json_t *ans = ta12_call(ctx, "answer_callback", b);
        if (ta12_is_raise(ans)) { json_free(vcp); return ans; }
        json_free(ans);
    }
    return vcp;
}

/* PoP: ta12_check_polling_stall @ plugins/platforms/telegram/adapter.py:_check_polling_stall */
static json_t *ta12_check_polling_stall(ta12_ctx_t *ctx, const json_t *args) {
    (void)args;
    if (ta12_truthy(ta12_state_get(ctx, "_webhook_mode")) ||
        ta12_truthy(ta12_state_get(ctx, "_teardown_started")) ||
        ta12_truthy(ta12_state_get(ctx, "has_fatal_error"))) return json_null();
    json_t *r = ta12_call(ctx, "recovery_in_flight", json_object());
    if (ta12_is_raise(r)) return r;
    bool rec = ta12_truthy(r); json_free(r);
    if (rec) return json_null();
    json_t *t = ta12_call(ctx, "monotonic", json_object());
    if (ta12_is_raise(t)) return t;
    double now = ta12_num(t); json_free(t);
    const json_t *lp = ta12_state_get(ctx, "_polling_last_progress_monotonic");
    const json_t *gs = ta12_state_get(ctx, "_polling_generation_started_monotonic");
    double stalled;
    if (!ta12_is_null(lp)) stalled = now - ta12_num(lp);
    else if (!ta12_is_null(gs)) stalled = now - ta12_num(gs);
    else return json_null();
    if (stalled <= _POLLING_STALL_TIMEOUT) return json_null();
    const json_t *gen = ta12_state_get(ctx, "_polling_generation");
    char *gs2 = (ta12_is_null(gen)) ? ta12_strdup("0") : ta12_py_str(gen);
    char *msg = ta12_fmt("getUpdates made no progress for %.0fs (generation %s; polling stall watchdog)",
                         stalled, gs2 ? gs2 : "0");
    free(gs2);
    json_t *a = json_object();
    json_set(a, "error", json_string(msg ? msg : ""));
    json_set(a, "reason", json_string("polling stall watchdog"));
    free(msg);
    json_t *s = ta12_call(ctx, "schedule_polling_recovery", a);
    if (ta12_is_raise(s)) return s;
    json_free(s);
    return json_null();
}

/* PoP: ta12_collect_live_tasks @ plugins/platforms/telegram/adapter.py:_collect_live_tasks */
static json_t *ta12_collect_live_tasks(ta12_ctx_t *ctx, const json_t *args) {
    const json_t *cands = ta12_arg(args, "candidates");
    const json_t *cur = ta12_arg(args, "current");
    json_t *out = json_array();
    json_t *seen = json_array();
    if (cands && cands->type == JSON_ARRAY) {
        for (size_t i = 0; i < cands->c.count; i++) {
            const json_t *task = cands->c.items[i];
            if (ta12_is_null(task)) continue;
            json_t *a = json_object();
            json_set(a, "task", ta12_copyv(task));
            json_t *d = ta12_call(ctx, "task_done", a);
            if (ta12_is_raise(d)) { json_free(out); json_free(seen); return d; }
            bool done = ta12_truthy(d); json_free(d);
            if (done) continue;
            if (ta12_json_eq(task, cur)) continue;
            char *id = ta12_py_str(ta12_getattr(task, "id"));
            bool dup = false;
            for (size_t j = 0; j < seen->c.count; j++) {
                if (seen->c.items[j] && seen->c.items[j]->type == JSON_STRING &&
                    id && strcmp(seen->c.items[j]->str_val, id) == 0) { dup = true; break; }
            }
            if (dup) { free(id); continue; }
            json_append(seen, json_string(id ? id : ""));
            free(id);
            json_append(out, ta12_copyv(task));
        }
    }
    json_free(seen);
    return out;
}

/* PoP: ta12_create_dm_topic @ plugins/platforms/telegram/adapter.py:_create_dm_topic */
static json_t *ta12_create_dm_topic(ta12_ctx_t *ctx, const json_t *args) {
    if (json_get_bool(args, "bot_none", false)) return json_null();
    json_t *a = json_object();
    json_set(a, "chat_id", ta12_copyv(ta12_arg(args, "chat_id")));
    json_set(a, "name", ta12_copyv(ta12_arg(args, "name")));
    const json_t *ic = ta12_arg(args, "icon_color");
    if (!ta12_is_null(ic)) json_set(a, "icon_color", ta12_copyv(ic));
    const json_t *ie = ta12_arg(args, "icon_custom_emoji_id");
    if (ta12_truthy(ie)) json_set(a, "icon_custom_emoji_id", ta12_copyv(ie));
    json_t *r = ta12_call(ctx, "create_forum_topic", a);
    if (ta12_is_raise(r)) {
        if (!ta12_is_exception(ta12_raise_kind(r))) return r; /* BaseException passthrough */
        const char *msg = json_get_str(r, "message", "");
        char *low = ta12_lower(msg ? msg : "");
        json_t *s;
        if ((low && strstr(low, "topic_name_duplicate")) || (low && strstr(low, "already"))) {
            json_t *arr = json_array();
            json_append(arr, json_string("[%s] DM topic '%s' already exists in chat %s (will be mapped from incoming messages)"));
            json_append(arr, json_string("bot"));
            json_append(arr, ta12_copyv(ta12_arg(args, "name")));
            json_append(arr, ta12_copyv(ta12_arg(args, "chat_id")));
            s = ta12_call(ctx, "logger_info", ta12_logger_args(arr));
        } else if ((low && (strstr(low, "not a forum") || strstr(low, "forums_disabled")))) {
            json_t *arr = json_array();
            json_append(arr, json_string("[%s] Cannot create DM topic '%s' in chat %s: Threaded Mode is not enabled. The bot owner must open the BotFather Mini App (search 'botfather' in Telegram, tap Open on the search result) -> My bots -> this bot -> Bot Settings -> Threads Settings -> enable Threaded Mode. This cannot be enabled from the DM chat, nor from the BotFather /mybots text menu."));
            json_append(arr, json_string("bot"));
            json_append(arr, ta12_copyv(ta12_arg(args, "name")));
            json_append(arr, ta12_copyv(ta12_arg(args, "chat_id")));
            s = ta12_call(ctx, "logger_warning", ta12_logger_kw(arr));
        } else {
            json_t *ra = json_object();
            json_set(ra, "text", ta12_copyv(r));
            json_t *rd = ta12_call(ctx, "redact", ra);
            if (ta12_is_raise(rd)) { free(low); json_free(r); return rd; }
            json_t *arr = json_array();
            json_append(arr, json_string("[%s] Failed to create DM topic '%s' in chat %s: %s"));
            json_append(arr, json_string("bot"));
            json_append(arr, ta12_copyv(ta12_arg(args, "name")));
            json_append(arr, ta12_copyv(ta12_arg(args, "chat_id")));
            json_append(arr, rd);
            s = ta12_call(ctx, "logger_warning", ta12_logger_kw(arr));
        }
        free(low); json_free(r);
        if (ta12_is_raise(s)) return s;
        json_free(s);
        return json_null();
    }
    json_t *tid = ta12_copyv(ta12_getattr(r, "message_thread_id"));
    json_free(r);
    json_t *arr = json_array();
    json_append(arr, json_string("[%s] Created DM topic '%s' in chat %s -> thread_id=%s"));
    json_append(arr, json_string("bot"));
    json_append(arr, ta12_copyv(ta12_arg(args, "name")));
    json_append(arr, ta12_copyv(ta12_arg(args, "chat_id")));
    json_append(arr, json_copy(tid));
    json_t *s = ta12_call(ctx, "logger_info", ta12_logger_args(arr));
    if (ta12_is_raise(s)) { json_free(tid); return s; }
    json_free(s);
    return tid;
}

/* helper: the except-Exception branch of _delete_webhook_best_effort */
static json_t *ta12_delete_webhook_err(ta12_ctx_t *ctx, const json_t *args, json_t *e) {
    json_t *ne = json_object();
    json_set(ne, "error", ta12_copyv(e));
    json_t *n = ta12_call(ctx, "looks_like_network_error", ne);
    if (ta12_is_raise(n)) { json_free(e); return n; }
    bool net = ta12_truthy(n); json_free(n);
    if (!net) return e;
    if (json_get_bool(args, "require_success", false)) {
        json_free(e);
        return ta12_err("OSError", "Telegram deleteWebhook did not complete during initial connect");
    }
    json_t *ra = json_object();
    json_set(ra, "text", ta12_copyv(e));
    json_t *rd = ta12_call(ctx, "redact", ra);
    if (ta12_is_raise(rd)) { json_free(e); return rd; }
    json_t *arr = json_array();
    json_append(arr, json_string("[%s] deleteWebhook failed with a recoverable network error; continuing to polling so getUpdates/retry can recover: %s"));
    json_append(arr, json_string("bot"));
    json_append(arr, rd);
    json_t *w = ta12_call(ctx, "logger_warning", ta12_logger_kw(arr));
    if (ta12_is_raise(w)) { json_free(e); return w; }
    json_free(w); json_free(e);
    json_set(ctx->state, "_send_path_degraded", json_bool(true));
    return json_bool(false);
}

/* PoP: ta12_delete_webhook_best_effort @ plugins/platforms/telegram/adapter.py:_delete_webhook_best_effort */
static json_t *ta12_delete_webhook_best_effort(ta12_ctx_t *ctx, const json_t *args) {
    const char *bk = ta12_str(ta12_arg(args, "bot_kind"));
    if (bk && strcmp(bk, "none") == 0) return json_bool(false);
    if (bk && strcmp(bk, "no_method") == 0) return json_bool(true);
    json_t *a = json_object();
    json_set(a, "drop_pending_updates", json_bool(false));
    json_t *dw = ta12_call(ctx, "delete_webhook", a);
    if (ta12_is_raise(dw)) {
        if (ta12_is_exception(ta12_raise_kind(dw)))
            return ta12_delete_webhook_err(ctx, args, dw);
        return dw;
    }
    json_t *b = json_object();
    json_set(b, "awaitable", json_copy(dw));
    json_set(b, "timeout", json_number(_UPDATER_START_TIMEOUT));
    json_t *aw = ta12_call(ctx, "await_with_thread_deadline", b);
    json_free(dw);
    if (ta12_is_raise(aw)) {
        if (ta12_is_exception(ta12_raise_kind(aw)))
            return ta12_delete_webhook_err(ctx, args, aw);
        return aw;
    }
    json_free(aw);
    return json_bool(true);
}

/* helper: the except-Exception branch of _download_observed_media */
static json_t *ta12_download_err(ta12_ctx_t *ctx, const json_t *args, json_t *e) {
    json_t *ra = json_object();
    json_set(ra, "text", ta12_copyv(e));
    json_t *rd = ta12_call(ctx, "redact", ra);
    if (ta12_is_raise(rd)) { json_free(e); return rd; }
    json_t *arr = json_array();
    json_append(arr, json_string("[Telegram] Failed to cache %s: %s"));
    json_append(arr, ta12_copyv(ta12_arg(args, "what")));
    json_append(arr, rd);
    json_t *w = ta12_call(ctx, "logger_warning", ta12_logger_exc(arr));
    if (ta12_is_raise(w)) { json_free(e); return w; }
    json_free(w); json_free(e);
    json_t *out = json_array();
    json_append(out, json_string("failed"));
    json_append(out, json_null());
    return out;
}

/* PoP: ta12_download_observed_media @ plugins/platforms/telegram/adapter.py:_download_observed_media */
static json_t *ta12_download_observed_media(ta12_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "msg", ta12_copyv(ta12_arg(args, "msg")));
    json_t *src = ta12_call(ctx, "observed_media_source", a);
    if (ta12_is_raise(src)) return src;
    json_t *source = (src && src->type == JSON_ARRAY && src->c.count > 0) ? (json_t *)src->c.items[0] : NULL;
    json_t *filename = (src && src->type == JSON_ARRAY && src->c.count > 1) ? (json_t *)src->c.items[1] : NULL;
    json_t *mime = (src && src->type == JSON_ARRAY && src->c.count > 2) ? (json_t *)src->c.items[2] : NULL;
    json_t *kind = (src && src->type == JSON_ARRAY && src->c.count > 3) ? (json_t *)src->c.items[3] : NULL;
    if (ta12_is_null(source)) {
        json_free(src);
        json_t *out = json_array(); json_append(out, json_string("none")); json_append(out, json_null());
        return out;
    }
    const json_t *fs = ta12_getattr(source, "file_size");
    json_t *b = json_object();
    json_set(b, "value", ta12_copyv(fs));
    json_t *iv = ta12_call(ctx, "int_or_zero", b);
    if (ta12_is_raise(iv)) { json_free(src); return iv; }
    double n = ta12_num(iv); json_free(iv);
    const json_t *mx = ta12_state_get(ctx, "_max_doc_bytes");
    double maxb = ta12_is_null(mx) ? 20.0 * 1024.0 * 1024.0 : ta12_num(mx);
    if (!(n > 0 && n <= maxb)) {
        json_t *out = json_array();
        json_append(out, json_string("oversized"));
        json_append(out, ta12_copyv(fs));
        json_free(src);
        return out;
    }
    json_t *lr = ta12_call(ctx, "local_root", json_object());
    if (ta12_is_raise(lr)) { json_free(src); return lr; }
    json_t *c = json_object();
    json_set(c, "source", ta12_copyv(source));
    json_set(c, "what", ta12_copyv(ta12_arg(args, "what")));
    json_set(c, "local_root", json_copy(lr));
    json_free(lr);
    json_t *f = ta12_call(ctx, "fetch_telegram_file", c);
    if (ta12_is_raise(f)) {
        json_free(src);
        if (ta12_is_exception(ta12_raise_kind(f))) return ta12_download_err(ctx, args, f);
        return f;
    }
    json_t *fo = (f && f->type == JSON_ARRAY && f->c.count > 0) ? (json_t *)f->c.items[0] : NULL;
    json_t *data = (f && f->type == JSON_ARRAY && f->c.count > 1) ? (json_t *)f->c.items[1] : NULL;
    char *fname = NULL;
    if (ta12_truthy(filename)) fname = ta12_py_str(filename);
    else {
        const json_t *fp = ta12_getattr(fo, "file_path");
        char *raw = ta12_py_str(fp);
        fname = ta12_basename(raw && raw[0] ? raw : "");
        free(raw);
    }
    json_t *d = json_object();
    json_set(d, "data", ta12_copyv(data));
    json_set(d, "filename", json_string(fname ? fname : ""));
    json_set(d, "mime_type", ta12_copyv(mime));
    json_set(d, "default_kind", ta12_copyv(kind));
    free(fname);
    json_t *cached = ta12_call(ctx, "cache_media_bytes_async", d);
    json_free(f); json_free(src);
    if (ta12_is_raise(cached)) {
        if (ta12_is_exception(ta12_raise_kind(cached))) return ta12_download_err(ctx, args, cached);
        return cached;
    }
    json_t *out = json_array();
    if (ta12_is_null(cached)) {
        json_append(out, json_string("unreadable"));
        json_append(out, json_null());
    } else {
        json_append(out, json_string("ok"));
        json_append(out, json_copy(cached));
    }
    json_free(cached);
    return out;
}

/* PoP: ta12_drain_general_connections_after_pool_timeout @ plugins/platforms/telegram/adapter.py:_drain_general_connections_after_pool_timeout */
static json_t *ta12_drain_general_connections_after_pool_timeout(ta12_ctx_t *ctx, const json_t *args) {
    const char *bk = ta12_str(ta12_arg(args, "bot_kind"));
    if ((bk && strcmp(bk, "none") == 0) || (bk && strcmp(bk, "bad_request") == 0))
        return json_null();
    json_t *g = ta12_call(ctx, "get_general_request_drain_lock", json_object());
    if (ta12_is_raise(g)) return g;
    json_free(g);
    json_t *s = ta12_call(ctx, "general_req_shutdown", json_object());
    if (ta12_is_raise(s)) return s;
    json_t *a = json_object();
    json_set(a, "awaitable", json_copy(s));
    json_set(a, "message", json_string("General request shutdown failed/timed out after pool timeout (non-fatal)"));
    json_t *b1 = ta12_call(ctx, "bounded_request_step", a);
    if (ta12_is_raise(b1)) { json_free(s); return b1; }
    json_free(b1); json_free(s);
    json_t *i = ta12_call(ctx, "general_req_initialize", json_object());
    if (ta12_is_raise(i)) return i;
    json_t *a2 = json_object();
    json_set(a2, "awaitable", json_copy(i));
    json_set(a2, "message", json_string("General request re-initialize failed/timed out after pool timeout (non-fatal)"));
    json_t *b2 = ta12_call(ctx, "bounded_request_step", a2);
    if (ta12_is_raise(b2)) { json_free(i); return b2; }
    bool ok = ta12_truthy(b2);
    json_free(b2); json_free(i);
    if (ok) {
        json_t *arr = json_array();
        json_append(arr, json_string("[%s] General request pool drained after Telegram pool timeout"));
        json_append(arr, json_string("bot"));
        json_t *w = ta12_call(ctx, "logger_warning", ta12_logger_kw(arr));
        if (ta12_is_raise(w)) return w;
        json_free(w);
    }
    return json_null();
}

/* PoP: ta12_disarm_ptb_retry_loop @ plugins/platforms/telegram/adapter.py:_disarm_ptb_retry_loop */
static json_t *ta12_disarm_ptb_retry_loop(ta12_ctx_t *ctx, const json_t *args) {
    const char *ak = ta12_str(ta12_arg(args, "app_kind"));
    if ((ak && strcmp(ak, "none") == 0) || (ak && strcmp(ak, "no_updater") == 0))
        return json_null();
    const json_t *attrs = ta12_arg(args, "attrs");
    if (attrs && attrs->type == JSON_ARRAY) {
        for (size_t k = 0; k < attrs->c.count; k++) {
            const char *attr = ta12_str(attrs->c.items[k]);
            json_t *is = ta12_call(ctx, "event_is_set", json_object());
            if (ta12_is_raise(is)) return is;
            bool set = ta12_truthy(is); json_free(is);
            if (!set) {
                json_t *sv = ta12_call(ctx, "event_set", json_object());
                if (ta12_is_raise(sv)) return sv;
                json_free(sv);
                json_t *arr = json_array();
                json_append(arr, json_string("[%s] Disarmed PTB polling retry loop via %s"));
                json_append(arr, json_string("bot"));
                json_append(arr, json_string(attr ? attr : ""));
                json_t *d = ta12_call(ctx, "logger_debug", ta12_logger_kw(arr));
                if (ta12_is_raise(d)) return d;
                json_free(d);
            }
            return json_null();
        }
    }
    json_t *arr = json_array();
    json_append(arr, json_string("[%s] Could not disarm PTB polling retry loop (stop_event not found on this PTB version); falling back to async stop()"));
    json_append(arr, json_string("bot"));
    json_t *d = ta12_call(ctx, "logger_debug", ta12_logger_kw(arr));
    if (ta12_is_raise(d)) return d;
    json_free(d);
    return json_null();
}

/* PoP: ta12_chat_send_lock @ plugins/platforms/telegram/adapter.py:_chat_send_lock */
static json_t *ta12_chat_send_lock(ta12_ctx_t *ctx, const json_t *args) {
    json_t *na = json_object();
    json_set(na, "chat_id", ta12_copyv(ta12_arg(args, "chat_id")));
    json_t *nr = ta12_call(ctx, "normalize_telegram_chat_id", na);
    if (ta12_is_raise(nr)) return nr;
    char *key = ta12_py_str(nr); json_free(nr);
    json_t *locks = (json_t *)ta12_state_get(ctx, "_telegram_chat_send_locks");
    if (!locks || locks->type != JSON_OBJECT) {
        locks = json_object(); json_set(ctx->state, "_telegram_chat_send_locks", locks);
    }
    json_t *owners = (json_t *)ta12_state_get(ctx, "_telegram_chat_send_lock_owners");
    if (!owners || owners->type != JSON_OBJECT) {
        owners = json_object(); json_set(ctx->state, "_telegram_chat_send_lock_owners", owners);
    }
    json_t *task = ta12_call(ctx, "current_task", json_object());
    if (ta12_is_raise(task)) { free(key); return task; }
    if (ta12_json_eq(json_obj_get(owners, key ? key : ""), task)) {
        json_free(task); free(key); return json_null();
    }
    if (!ta12_has(locks, key ? key : "")) {
        json_t *lk = ta12_call(ctx, "new_lock", json_object());
        if (ta12_is_raise(lk)) { json_free(task); free(key); return lk; }
        json_free(lk);
        json_set(locks, key ? key : "", json_object());
    }
    json_t *ent = ta12_call(ctx, "lock_aenter", json_object());
    if (ta12_is_raise(ent)) { json_free(task); free(key); return ent; }
    json_free(ent);
    json_set(owners, key ? key : "", json_copy(task));
    json_free(task);
    json_t *w = ta12_call(ctx, "lock_waiters", json_object());
    bool wraise = ta12_is_raise(w);
    bool waiters = !wraise && ta12_truthy(w);
    if (!wraise && !waiters) json_obj_del(locks, key ? key : "");
    if (!wraise) json_free(w);
    json_obj_del(owners, key ? key : "");
    json_t *ex = ta12_call(ctx, "lock_aexit", json_object());
    free(key);
    if (ta12_is_raise(ex)) { if (wraise) json_free(w); return ex; }
    json_free(ex);
    /* A raise from the _waiters read is swallowed by the live generator's
     * async-for teardown; the ordered trace still records it. */
    if (wraise) { json_free(w); return json_null(); }
    return json_null();
}

/* helper: logger.warning for the compress exception path */
static json_t *ta12_compress_warn(ta12_ctx_t *ctx, const char *fmt, json_t *extra) {
    json_t *arr = json_array();
    json_append(arr, json_string(fmt));
    json_append(arr, json_string("bot"));
    if (extra) json_append(arr, extra);
    json_t *w = ta12_call(ctx, "logger_warning", ta12_logger_kw(arr));
    return w;
}

/* PoP: ta12_compress_image_to_jpeg @ plugins/platforms/telegram/adapter.py:_compress_image_to_jpeg */
static json_t *ta12_compress_image_to_jpeg(ta12_ctx_t *ctx, const json_t *args) {
    const char *path = ta12_str(ta12_arg(args, "image_path"));
    json_t *a = json_object();
    json_set(a, "path", ta12_copyv(ta12_arg(args, "image_path")));
    json_t *sz = ta12_call(ctx, "getsize", a);
    if (ta12_is_raise(sz)) {
        if (ta12_is_exception(ta12_raise_kind(sz))) { json_free(sz); return json_null(); }
        return sz;
    }
    double size = ta12_num(sz); json_free(sz);
    char *ext = ta12_splitext_lower(path);
    bool jpeg = ext && (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0);
    free(ext);
    if (jpeg) return json_null();
    if (size < _IMG_COMPRESS_THRESHOLD_BYTES) return json_null();
    json_t *sa = json_object();
    json_set(sa, "path", ta12_copyv(ta12_arg(args, "image_path")));
    json_t *sn = ta12_call(ctx, "sniff_raster", sa);
    if (ta12_is_raise(sn)) return sn;
    bool raster = !ta12_is_null(sn); json_free(sn);
    if (!raster) return json_null();
    json_t *pi = ta12_call(ctx, "pil_import", json_object());
    if (ta12_is_raise(pi)) {
        if (ta12_is_exception(ta12_raise_kind(pi))) {
            json_free(pi);
            json_t *w = ta12_compress_warn(ctx, "[%s] Pillow not available for image compression", NULL);
            if (ta12_is_raise(w)) return w;
            json_free(w);
            return json_null();
        }
        return pi;
    }
    json_free(pi);
    json_t *pa = json_object();
    json_set(pa, "path", ta12_copyv(ta12_arg(args, "image_path")));
    json_t *pw = ta12_call(ctx, "pil_work", pa);
    if (ta12_is_raise(pw)) {
        json_t *rd = json_copy(pw);
        json_free(pw);
        if (ta12_is_exception(ta12_raise_kind(rd))) {
            json_t *w = ta12_compress_warn(ctx, "[%s] Image compression failed, uploading original: %s", rd);
            if (ta12_is_raise(w)) return w;
            json_free(w);
            return json_null();
        }
        return rd;
    }
    json_free(pw);
    json_t *ma = json_object();
    json_set(ma, "suffix", json_string(".jpg"));
    json_set(ma, "prefix", json_string("tg_compress_"));
    json_t *mk = ta12_call(ctx, "mkstemp", ma);
    if (ta12_is_raise(mk)) {
        json_t *rd = json_copy(mk); json_free(mk);
        if (ta12_is_exception(ta12_raise_kind(rd))) {
            json_t *w = ta12_compress_warn(ctx, "[%s] Image compression failed, uploading original: %s", rd);
            if (ta12_is_raise(w)) return w;
            json_free(w);
            return json_null();
        }
        return rd;
    }
    char *tmp = ta12_py_str(mk);
    json_free(mk);
    json_t *ga = json_object();
    json_set(ga, "path", json_string(tmp ? tmp : ""));
    json_t *gt = ta12_call(ctx, "getsize", ga);
    if (ta12_is_raise(gt)) {
        json_t *rd = json_copy(gt); json_free(gt);
        free(tmp);
        if (ta12_is_exception(ta12_raise_kind(rd))) {
            json_t *w = ta12_compress_warn(ctx, "[%s] Image compression failed, uploading original: %s", rd);
            if (ta12_is_raise(w)) return w;
            json_free(w);
            return json_null();
        }
        return rd;
    }
    double tsz = ta12_num(gt); json_free(gt);
    json_t *arr = json_array();
    json_append(arr, json_string("[%s] Pre-compressed %s (%.1fKB → %s %.1fKB) for Telegram upload"));
    json_append(arr, json_string("bot"));
    json_append(arr, ta12_copyv(ta12_arg(args, "image_path")));
    json_append(arr, json_number(size / 1024.0));
    json_append(arr, json_string(tmp ? tmp : ""));
    json_append(arr, json_number(tsz / 1024.0));
    json_t *li = ta12_call(ctx, "logger_info", ta12_logger_args(arr));
    if (ta12_is_raise(li)) { free(tmp); return li; }
    json_free(li);
    json_t *ret = json_string(tmp ? tmp : "");
    free(tmp);
    return ret;
}

/* PoP: ta12_normalize_chat_type @ plugins/platforms/telegram/adapter.py:_normalize_chat_type */
static json_t *ta12_normalize_chat_type(ta12_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *ct = ta12_arg(args, "chat_type");
    char *raw = ta12_truthy(ct) ? ta12_py_str(ct) : ta12_strdup("dm");
    char *s = ta12_strip(raw ? raw : "");
    char *low = ta12_lower(s);
    free(raw); free(s);
    if (!low || !*low) { free(low); return json_string("dm"); }
    if (strcmp(low, "private") == 0) { free(low); return json_string("dm"); }
    if (strcmp(low, "supergroup") == 0) {
        free(low);
        return json_string(json_get_bool(args, "is_forum", false) ? "forum" : "group");
    }
    json_t *r = json_string(low); free(low); return r;
}

/* Parse Python float(raw) semantics; returns false on ValueError. */
static bool ta12_parse_float(const char *s, double *out) {
    if (!s) return false;
    char *t = ta12_strip(s);
    if (!t || !*t) { free(t); return false; }
    char *end = NULL;
    double d = strtod(t, &end);
    if (end == t || *end != '\0') { free(t); return false; }
    free(t);
    *out = d;
    return true;
}

/* PoP: ta12_env_float_clamped @ plugins/platforms/telegram/adapter.py:_env_float_clamped */
static json_t *ta12_env_float_clamped(ta12_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "key", ta12_copyv(ta12_arg(args, "name")));
    json_t *r = ta12_call(ctx, "getenv", a);
    if (ta12_is_raise(r)) return r;
    double def = ta12_num(ta12_arg(args, "default"));
    double v = def;
    if (!ta12_is_null(r)) {
        char *s = ta12_py_str(r);
        if (!ta12_parse_float(s, &v)) v = def;
        free(s);
    }
    json_free(r);
    if (!isfinite(v)) v = def;
    const json_t *mn = ta12_arg(args, "min_value");
    const json_t *mx = ta12_arg(args, "max_value");
    if (!ta12_is_null(mn)) { double m = ta12_num(mn); if (v < m) v = m; }
    if (!ta12_is_null(mx)) { double m = ta12_num(mx); if (v > m) v = m; }
    return json_number(v);
}

/* PoP: ta12_notification_kwargs @ plugins/platforms/telegram/adapter.py:_notification_kwargs */
static json_t *ta12_notification_kwargs(ta12_ctx_t *ctx, const json_t *args) {
    const json_t *mode = ta12_state_get(ctx, "_notifications_mode");
    const char *ms = mode ? ((mode->type == JSON_STRING) ? ta12_str(mode) : NULL) : "important";
    if (!ms || strcmp(ms, "important") != 0) return json_object();
    const json_t *meta = ta12_arg(args, "metadata");
    if (meta && meta->type == JSON_OBJECT) {
        if (ta12_truthy(json_obj_get((json_t *)meta, "notify")) ||
            ta12_truthy(json_obj_get((json_t *)meta, "is_approval_prompt")))
            return json_object();
    }
    json_t *o = json_object();
    json_set(o, "disable_notification", json_bool(true));
    return o;
}

/* PoP: ta12_is_id_like @ plugins/platforms/telegram/adapter.py:_is_id_like */
static json_t *ta12_is_id_like(ta12_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *v = ta12_arg(args, "value");
    if (!v) return json_bool(false);
    if (v->type == JSON_STRING) return json_bool(true);
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        return json_bool(d == floor(d) && fabs(d) < 1e16);
    }
    return json_bool(false);
}

/* PoP: ta12_coerce_bool_extra @ plugins/platforms/telegram/adapter.py:_coerce_bool_extra */
static json_t *ta12_coerce_bool_extra(ta12_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *key = ta12_str(ta12_arg(args, "key"));
    bool def = json_get_bool(args, "default", false);
    const json_t *extra = ta12_arg(args, "extra");
    if (!extra || extra->type != JSON_OBJECT || extra->c.count == 0) return json_bool(def);
    const json_t *v = key ? json_obj_get((json_t *)extra, key) : NULL;
    if (ta12_is_null(v)) return json_bool(def);
    if (v->type == JSON_STRING) {
        char *lo = ta12_lower(v->str_val);
        char *tr = ta12_strip(lo ? lo : "");
        free(lo);
        if (tr && (strcmp(tr, "true") == 0 || strcmp(tr, "1") == 0 ||
                   strcmp(tr, "yes") == 0 || strcmp(tr, "on") == 0)) { free(tr); return json_bool(true); }
        if (tr && (strcmp(tr, "false") == 0 || strcmp(tr, "0") == 0 ||
                   strcmp(tr, "no") == 0 || strcmp(tr, "off") == 0)) { free(tr); return json_bool(false); }
        free(tr);
        return json_bool(def);
    }
    return json_bool(ta12_truthy(v));
}
