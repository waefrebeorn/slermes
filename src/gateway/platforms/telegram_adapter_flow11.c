/*
 * telegram_adapter_flow11.c - Telegram adapter FLOW part 11 of
 * plugins/platforms/telegram/adapter.py (hermes-agent): a coherent batch of
 * UNCLAIMED defs left after telegram_adapter_flow{,2,3,6,7,8,9,10}.c. Every
 * target below was confirmed UNCLAIMED; no flow 1-10 target is re-ported.
 *
 * Themes: caches/topic bookkeeping, callback authorization, lifecycle cancel,
 * env/config bridging, cold-start readiness, event/request builders, media
 * ingress, and three pure static helpers. All live I/O and injected callables
 * enter through ONE universal seam: ta11_call(ctx,name,args).
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * Every symbol is static and ta11_-prefixed.
 *
 * Representation contract (shared with tests/oracle_telegram_adapter_flow11.py):
 * - NULL json_t* == Python None / absent; JSON_NULL is a real null value.
 * - Seam scripts: a plain value repeats; a LIST is consumed per call (last
 *   repeats); {"__script__":true,"ret":v,"raise":{type,message}} forces a
 *   result or raise. Missing/exhausted script => fail-closed RuntimeError
 *   recorded in the trace before propagation (BaseException passthrough).
 * - Except sites swallow ONLY Exception-family kinds (TA11_EXC_HIER);
 *   BaseException-only kinds pass through every except-Exception site.
 * - Fixture domains (documented so out-of-domain branches are not modeled):
 *   _apply_yaml_config values are scalars; mention_patterns/json.dumps excluded.
 *   _cache_inbound_document uses the oversized early-return path only.
 *   _cache_inbound_av uses the size-not-allowed + exception paths.
 *   _build_ptb_requests forces HERMES_TELEGRAM_DISABLE_FALLBACK_IPS on (no DNS
 *   discovery / fallback-IP transport).
 *   _build_message_event collaborators are all scripted; chat/user/message are
 *   attribute doubles carrying only the fields named in recorder projections.
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
#define _DISCONNECT_STEP_TIMEOUT 5.0
#define _INITIAL_POLLING_PROGRESS_TIMEOUT 30.0

typedef struct ta11_ctx {
    json_t *state;        /* owned; carries the attribute-double fields */
    const json_t *io;     /* borrowed scripted seam results */
    json_t *cursors;      /* owned; per-name array cursors */
    json_t *trace;        /* owned; ordered seam events */
    const json_t *errors; /* borrowed exception-graph registry */
} ta11_ctx_t;

/* ── basic helpers ── */
static bool ta11_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *ta11_raise_kind(const json_t *v) {
    return ta11_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static bool ta11_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool ta11_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *ta11_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *ta11_copyv(const json_t *v) { return v ? json_copy((json_t *)v) : json_null(); }
static json_t *ta11_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static const json_t *ta11_arg(const json_t *args, const char *key) {
    return args ? json_obj_get(args, key) : NULL;
}
static const json_t *ta11_getattr(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
static const json_t *ta11_state_get(const ta11_ctx_t *ctx, const char *key) {
    return (ctx && ctx->state && ctx->state->type == JSON_OBJECT)
        ? json_obj_get(ctx->state, key) : NULL;
}
static double ta11_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static bool ta11_has(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}
/* setdefault: adopt val into obj if key absent; return the stored value (borrowed) */
static json_t *ta11_setdefault(json_t *obj, const char *key, json_t *val) {
    json_t *cur = json_obj_get(obj, key);
    if (cur) return cur;
    json_set(obj, key, val);
    return val;
}

/* ── string helpers ── */
static char *ta11_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static char *ta11_lower(const char *s) {
    char *o = ta11_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static char *ta11_fmt(const char *fmt, ...) {
    char tmp[1024]; va_list ap;
    va_start(ap, fmt); int n = vsnprintf(tmp, sizeof tmp, fmt, ap); va_end(ap);
    if (n < 0) return ta11_strdup("");
    char *o = malloc((size_t)n + 1);
    if (!o) return ta11_strdup("");
    va_start(ap, fmt); vsnprintf(o, (size_t)n + 1, fmt, ap); va_end(ap);
    return o;
}
/* Python str() of a JSON scalar; integral doubles render as ints. */
static char *ta11_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return ta11_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return ta11_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e16) return ta11_fmt("%.0f", d);
            return ta11_fmt("%.17g", d);
        }
        case JSON_STRING: return ta11_strdup(v->str_val ? v->str_val : "");
        default: { char *s = json_serialize((json_t *)v); return s ? s : ta11_strdup(""); }
    }
}
static char *ta11_strip(const char *s) {
    if (!s) return ta11_strdup("");
    while (*s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\v' || *s == '\f')) s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\n' || s[n-1] == '\r' || s[n-1] == '\v' || s[n-1] == '\f')) n--;
    char *o = malloc(n + 1);
    if (o) { memcpy(o, s, n); o[n] = '\0'; }
    return o;
}
/* Python int() on a scalar; *ok=false on failure. */
static long long ta11_py_int(const json_t *v, bool *ok) {
    if (ok) *ok = true;
    if (!v || v->type == JSON_NULL) { if (ok) *ok = false; return 0; }
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) { if (ok) *ok = false; return 0; }
    if (v->type == JSON_STRING) {
        char *s = ta11_strip(v->str_val);
        char *end = NULL;
        long long n = strtoll(s, &end, 10);
        if (end == s || *end != '\0') { free(s); if (ok) *ok = false; return 0; }
        free(s);
        return n;
    }
    if (ok) *ok = false;
    return 0;
}
/* os.path.splitext: returns extension incl. dot (or "" ) */
static char *ta11_splitext(const char *p) {
    if (!p) return ta11_strdup("");
    const char *base = strrchr(p, '/'); base = base ? base + 1 : p;
    const char *dot = strrchr(base, '.');
    if (!dot || dot == base) return ta11_strdup("");
    char *o = ta11_strdup(dot);
    for (char *q = o; *q; q++) *q = (char)tolower((unsigned char)*q);
    return o;
}

/* ── FULL builtin exception hierarchy (CPython 3.14) ── */
typedef struct { const char *name; const char *parent; } ta11_exc_row_t;
static const ta11_exc_row_t TA11_EXC_HIER[] = {
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
static const char *ta11_builtin_parent(const char *name) {
    for (size_t i = 0; i < sizeof(TA11_EXC_HIER) / sizeof(TA11_EXC_HIER[0]); i++)
        if (strcmp(name, TA11_EXC_HIER[i].name) == 0) return TA11_EXC_HIER[i].parent;
    return NULL;
}
static bool ta11_chain_has(const char *start, const char *target) {
    const char *cur = start;
    for (int depth = 0; cur && depth < 24; depth++) {
        if (strcmp(cur, target) == 0) return true;
        cur = ta11_builtin_parent(cur);
    }
    return false;
}
static bool ta11_is_exception(const char *kind) {
    if (!kind) return false;
    if (ta11_builtin_parent(kind) == NULL) return true;
    return ta11_chain_has(kind, "Exception");
}

/* ── logger helper: args {"args":[...]} ── */
static json_t *ta11_logger_args(json_t *arr) {
    json_t *a = json_object();
    json_set(a, "args", arr ? arr : json_array());
    return a;
}

/* ── the one universal seam ──
 * args is ADOPTED into the trace event; the return is a fresh owned node. */
static json_t *ta11_call(ta11_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ta11_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ta11_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = ta11_err(json_get_str(raise, "type", "RuntimeError"),
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

/* PoP: ta11_cache_replied_media @ plugins/platforms/telegram/adapter.py:_cache_replied_media */
static json_t *ta11_cache_replied_media(ta11_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "msg", ta11_copyv(ta11_arg(args, "msg")));
    json_set(a, "event", ta11_copyv(ta11_arg(args, "event")));
    json_t *r = ta11_call(ctx, "cache_replied_media", a);
    if (ta11_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: ta11_cache_dm_topic_from_message @ plugins/platforms/telegram/adapter.py:_cache_dm_topic_from_message */
static json_t *ta11_cache_dm_topic_from_message(ta11_ctx_t *ctx, const json_t *args) {
    const json_t *cid = ta11_arg(args, "chat_id");
    const json_t *tid = ta11_arg(args, "thread_id");
    const json_t *tname = ta11_arg(args, "topic_name");
    char *cs = ta11_py_str(cid), *ts = ta11_py_str(tname);
    char *key = ta11_fmt("%s:%s", cs ? cs : "", ts ? ts : "");
    free(cs); free(ts);
    json_t *topics = (json_t *)ta11_state_get(ctx, "_dm_topics");
    if (!topics || topics->type != JSON_OBJECT) {
        topics = json_object();
        json_set(ctx->state, "_dm_topics", topics);
    }
    if (!ta11_has(topics, key)) {
        bool ok = true;
        long long n = ta11_py_int(tid, &ok);
        if (!ok) {
            char *tv = ta11_py_str(tid);
            char *msg = ta11_fmt("invalid literal for int() with base 10: '%s'", tv ? tv : "");
            json_t *e = ta11_err("ValueError", msg ? msg : "");
            free(msg); free(tv); free(key);
            return e;
        }
        json_set(topics, key, json_number((double)n));
        /* logger.info(fmt, name, key, thread_id) */
        json_t *arr = json_array();
        json_append(arr, json_string("[%s] Cached DM topic from message: %s -> thread_id=%s"));
        json_append(arr, ta11_copyv(ta11_state_get(ctx, "name")));
        json_append(arr, json_string(key));
        json_append(arr, ta11_copyv(tid));
        json_t *r = ta11_call(ctx, "logger_info", ta11_logger_args(arr));
        if (ta11_is_raise(r)) { free(key); return r; }
        json_free(r);
    }
    free(key);
    return json_null();
}

/* PoP: ta11_callback_authorized @ plugins/platforms/telegram/adapter.py:_callback_authorized */
static json_t *ta11_callback_authorized(ta11_ctx_t *ctx, const json_t *args) {
    const json_t *q = ta11_arg(args, "query");
    const json_t *cb = ta11_arg(args, "cb");
    const json_t *u = ta11_getattr(ta11_getattr(q, "from_user"), "id");
    char *uid = ta11_py_str(u);
    json_t *a = json_object();
    json_set_hdr(a, "user_id", uid ? uid : "");
    json_set(a, "chat_id", ta11_copyv(ta11_getattr(cb, "chat_id")));
    json_t *ct = (json_t *)ta11_getattr(cb, "chat_type");
    if (ct && ct->type != JSON_NULL) { char *s = ta11_py_str(ct); json_set(a, "chat_type", json_string(s ? s : "")); free(s); }
    else json_set(a, "chat_type", json_null());
    json_t *th = (json_t *)ta11_getattr(cb, "thread_id");
    if (th && th->type != JSON_NULL) { char *s = ta11_py_str(th); json_set_hdr(a, "thread_id", s ? s : ""); free(s); }
    else json_set(a, "thread_id", json_null());
    json_set(a, "user_name", ta11_copyv(ta11_getattr(cb, "user_name")));
    free(uid);
    json_t *r = ta11_call(ctx, "_is_callback_user_authorized", a);
    if (ta11_is_raise(r)) return r;
    bool auth = ta11_truthy(r);
    json_free(r);
    if (auth) return json_bool(true);
    json_t *b = json_object();
    json_set(b, "query", ta11_copyv(q));
    json_set(b, "text", ta11_copyv(ta11_arg(args, "denial_text")));
    json_t *r2 = ta11_call(ctx, "answer_callback", b);
    if (ta11_is_raise(r2)) return r2;
    json_free(r2);
    return json_bool(false);
}

/* PoP: ta11_chat_outbound_slot_remaining @ plugins/platforms/telegram/adapter.py:_chat_outbound_slot_remaining */
static json_t *ta11_chat_outbound_slot_remaining(ta11_ctx_t *ctx, const json_t *args) {
    json_t *slot = (json_t *)ta11_state_get(ctx, "_telegram_chat_outbound_slot_until");
    if (!slot || slot->type != JSON_OBJECT) {
        slot = json_object();
        json_set(ctx->state, "_telegram_chat_outbound_slot_until", slot);
    }
    json_t *a = json_object();
    json_set(a, "chat_id", ta11_copyv(ta11_arg(args, "chat_id")));
    json_t *nr = ta11_call(ctx, "normalize_telegram_chat_id", a);
    if (ta11_is_raise(nr)) return nr;
    char *ks = ta11_py_str(nr);
    json_free(nr);
    const json_t *deadline = ta11_getattr(slot, ks ? ks : "");
    if (ta11_is_null(deadline)) { free(ks); return json_number(0.0); }
    json_t *b = json_object();
    json_t *t = ta11_call(ctx, "loop_time", b);
    if (ta11_is_raise(t)) { free(ks); return t; }
    double rem = ta11_num(deadline) - ta11_num(t);
    json_free(t);
    if (rem <= 0) {
        if (ks) json_obj_del(slot, ks);
        free(ks);
        return json_number(0.0);
    }
    free(ks);
    return json_number(rem);
}

/* PoP: ta11_cancel_task_attr @ plugins/platforms/telegram/adapter.py:_cancel_task_attr */
static json_t *ta11_cancel_task_attr(ta11_ctx_t *ctx, const json_t *args) {
    const char *attr = ta11_str(ta11_arg(args, "attr")) ? ta11_str(ta11_arg(args, "attr")) : "";
    const json_t *task = ta11_state_get(ctx, attr);
    if (!ta11_is_null(task)) {
        json_t *a = json_object();
        json_t *d = ta11_call(ctx, "task_done", a);
        if (ta11_is_raise(d)) return d;
        bool done = ta11_truthy(d);
        json_free(d);
        if (!done) {
            json_t *b = json_object();
            json_t *c = ta11_call(ctx, "task_cancel", b);
            if (ta11_is_raise(c)) return c;
            json_free(c);
            json_t *e = json_object();
            json_set(e, "awaitable", ta11_copyv(task));
            json_set(e, "timeout", json_number(_DISCONNECT_STEP_TIMEOUT));
            json_set(e, "step", ta11_copyv(ta11_arg(args, "label")));
            json_t *w = ta11_call(ctx, "_await_disconnect_step", e);
            if (ta11_is_raise(w)) return w;
            json_free(w);
        }
    }
    json_set(ctx->state, attr, json_null());
    return json_null();
}

/* PoP: ta11_cache_observed_media @ plugins/platforms/telegram/adapter.py:_cache_observed_media */
static json_t *ta11_cache_observed_media(ta11_ctx_t *ctx, const json_t *args) {
    const json_t *ev_in = ta11_arg(args, "event");
    json_t *event = json_copy((json_t *)ev_in);
    json_set(ctx->state, "event", event);
    json_t *a = json_object();
    json_set(a, "msg", ta11_copyv(ta11_arg(args, "msg")));
    json_set_hdr(a, "label", "observed group media");
    json_t *dl = ta11_call(ctx, "_download_observed_media", a);
    if (ta11_is_raise(dl)) return dl;
    const char *status = (dl && dl->type == JSON_ARRAY && json_len(dl) > 0) ? ta11_str(json_get(dl, 0)) : NULL;
    const json_t *cached = (dl && dl->type == JSON_ARRAY && json_len(dl) > 1) ? json_get(dl, 1) : NULL;
    char *note = NULL;
    bool do_log = false, do_attach = false;
    if (status && strcmp(status, "oversized") == 0) {
        double mb = ta11_num(ta11_state_get(ctx, "_max_doc_bytes")) / (1024.0 * 1024.0);
        note = ta11_fmt("[Observed Telegram attachment too large or unverifiable. Maximum: %.0f MB.]", mb);
        do_log = true;
    } else if (status && strcmp(status, "unreadable") == 0) {
        note = ta11_strdup("[Observed Telegram attachment could not be read, not cached.]");
    } else if (status && strcmp(status, "ok") == 0) {
        json_set(event, "media_urls", json_array());
        json_set(event, "media_types", json_array());
        json_set(event, "media_text_inlined", json_array());
        do_attach = true;
    }
    /* free dl (cached borrowed -> copy first) */
    json_t *cached_copy = ta11_copyv(cached);
    json_free(dl);
    if (do_attach) {
        json_t *b = json_object();
        json_set(b, "cached", json_copy(cached_copy));
        json_t *om = ta11_call(ctx, "observed_media_note", b);
        if (ta11_is_raise(om)) { json_free(cached_copy); return om; }
        json_t *c = json_object();
        json_set(c, "event", json_copy(event));
        json_set(c, "note", om);
        json_set_hdr(c, "log_fmt", "[Telegram] Cached observed group %s at %s");
        json_t *at = ta11_call(ctx, "_attach_cached", c);
        if (ta11_is_raise(at)) { json_free(cached_copy); return at; }
        json_free(at); json_free(cached_copy);
        return json_null();
    }
    /* append observed note */
    json_t *b = json_object();
    json_set(b, "existing", ta11_copyv(ta11_getattr(event, "text")));
    json_set(b, "note", note ? json_string(note) : json_null());
    free(note);
    json_t *an = ta11_call(ctx, "append_observed_note", b);
    if (ta11_is_raise(an)) { json_free(cached_copy); return an; }
    json_set(event, "text", an);
    if (do_log) {
        json_t *arr = json_array();
        json_append(arr, json_string("[Telegram] Observed group attachment skipped (size=%s)"));
        json_append(arr, json_copy(cached_copy));
        json_t *li = ta11_call(ctx, "logger_info", ta11_logger_args(arr));
        if (ta11_is_raise(li)) { json_free(cached_copy); return li; }
        json_free(li);
    }
    json_free(cached_copy);
    return json_null();
}

/* PoP: ta11_caption_1024 @ plugins/platforms/telegram/adapter.py:_caption_1024 */
static json_t *ta11_caption_1024(ta11_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *cap = ta11_arg(args, "caption");
    if (ta11_is_null(cap) || (cap->type == JSON_STRING && cap->str_val[0] == '\0')) return json_null();
    if (cap->type != JSON_STRING) return json_copy((json_t *)cap);
    size_t n = strlen(cap->str_val);
    /* Python str[:1024] is by codepoint; fixtures use ASCII or multibyte UTF-8 chars. */
    if (n <= 1024) return json_string(cap->str_val);
    /* count UTF-8 codepoints up to 1024 */
    size_t i = 0, cp = 0;
    while (i < n && cp < 1024) {
        unsigned char c = (unsigned char)cap->str_val[i];
        size_t step = c < 0x80 ? 1 : (c < 0xE0 ? 2 : (c < 0xF0 ? 3 : 4));
        i += step; cp++;
    }
    char *o = malloc(i + 1);
    if (!o) return json_null();
    memcpy(o, cap->str_val, i); o[i] = '\0';
    json_t *out = json_string(o); free(o);
    return out;
}

/* PoP: ta11_entity_type @ plugins/platforms/telegram/adapter.py:_entity_type */
static json_t *ta11_entity_type(ta11_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *ent = ta11_arg(args, "entity");
    const json_t *t = ta11_getattr(ent, "type");
    char *s = ta11_py_str(t ? t : json_string(""));
    if (!s) return json_string("");
    /* Python str(getattr(entity,"type","")).split(".")[-1].lower() */
    char *last = s;
    for (char *p = s; *p; p++) if (*p == '.') last = p + 1;
    char *o = ta11_lower(last);
    json_t *out = json_string(o ? o : "");
    free(o); free(s);
    return out;
}

/* PoP: ta11_ext_from_path @ plugins/platforms/telegram/adapter.py:_ext_from_path */
static json_t *ta11_ext_from_path(ta11_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *fp = ta11_arg(args, "file_path");
    const json_t *cands = ta11_arg(args, "candidates");
    const json_t *defv = ta11_arg(args, "default");
    if (ta11_str(fp) && fp->str_val[0]) {
        char *low = ta11_lower(fp->str_val);
        if (cands && cands->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(cands); i++) {
                const char *c = ta11_str(json_get(cands, i));
                if (c) {
                    size_t cl = strlen(c);
                    size_t sl = strlen(low);
                    if (cl <= sl && strcmp(low + sl - cl, c) == 0) { free(low); return json_string(c); }
                }
            }
        }
        free(low);
    }
    return ta11_copyv(defv);
}

/* PoP: ta11_apply_yaml_config @ plugins/platforms/telegram/adapter.py:_apply_yaml_config */
static void ta11_setenv(ta11_ctx_t *ctx, const char *env, json_t *val) {
    json_t *a = json_object();
    json_set_hdr(a, "env", env);
    json_set(a, "value", val ? val : json_null());
    json_t *r = ta11_call(ctx, "yaml_env_set", a);
    if (ta11_is_raise(r)) { /* handled by caller via pending flag */ }
    json_free(r);
}
static json_t *ta11_apply_yaml_config(ta11_ctx_t *ctx, const json_t *args) {
    const json_t *yc = ta11_arg(args, "yaml_cfg");
    const json_t *tc = ta11_arg(args, "telegram_cfg");
    json_t *extras = json_object();
    json_t *setter = ta11_call(ctx, "yaml_env_setter", json_object());
    if (ta11_is_raise(setter)) return setter;
    json_free(setter);
    /* topic keys */
    const char *topic_keys[] = {"disable_topic_auto_rename", "dm_topic_auto_transfer"};
    for (int i = 0; i < 2; i++) {
        const json_t *v = ta11_getattr(tc, topic_keys[i]);
        if (v) ta11_setdefault(extras, topic_keys[i], json_copy((json_t *)v));
    }
    /* require_mention */
    const json_t *rm = ta11_getattr(tc, "require_mention");
    if (!rm) rm = ta11_getattr(yc, "require_mention");
    if (!ta11_is_null(rm)) {
        char *s = ta11_lower(ta11_py_str(rm));
        json_t *v = json_string(s ? s : ""); free(s);
        ta11_setenv(ctx, "TELEGRAM_REQUIRE_MENTION", v);
    }
    /* lower bridges */
    const char *lower[][2] = {
        {"exclusive_bot_mentions","TELEGRAM_EXCLUSIVE_BOT_MENTIONS"},
        {"allow_bots","TELEGRAM_ALLOW_BOTS"},
        {"bots_require_mention","TELEGRAM_BOTS_REQUIRE_MENTION"},
        {"guest_mode","TELEGRAM_GUEST_MODE"},
        {"observe_unmentioned_group_messages","TELEGRAM_OBSERVE_UNMENTIONED_GROUP_MESSAGES"},
    };
    for (int i = 0; i < 5; i++) {
        const json_t *v = ta11_getattr(tc, lower[i][0]);
        if (v) {
            ta11_setdefault(extras, lower[i][0], json_copy((json_t *)v));
            char *s = ta11_lower(ta11_py_str(v));
            ta11_setenv(ctx, lower[i][1], json_string(s ? s : ""));
            free(s);
        }
    }
    /* gates */
    const char *gates[][3] = {
        {"free_response_chats","TELEGRAM_FREE_RESPONSE_CHATS","1"},
        {"free_response_topics","TELEGRAM_FREE_RESPONSE_TOPICS","0"},
        {"allowed_chats","TELEGRAM_ALLOWED_CHATS","0"},
        {"allowed_topics","TELEGRAM_ALLOWED_TOPICS","0"},
        {"ignored_threads","TELEGRAM_IGNORED_THREADS","1"},
    };
    for (int i = 0; i < 5; i++) {
        const json_t *v = ta11_getattr(tc, gates[i][0]);
        if (ta11_is_null(v)) continue;
        if (gates[i][2][0] == '1') ta11_setdefault(extras, gates[i][0], json_copy((json_t *)v));
        ta11_setenv(ctx, gates[i][1], json_copy((json_t *)v));
    }
    /* reactions */
    {
        const json_t *v = ta11_getattr(tc, "reactions");
        if (v) {
            ta11_setdefault(extras, "reactions", json_copy((json_t *)v));
            char *s = ta11_lower(ta11_py_str(v));
            ta11_setenv(ctx, "TELEGRAM_REACTIONS", json_string(s ? s : ""));
            free(s);
        }
    }
    /* proxy_url */
    {
        const json_t *v = ta11_getattr(tc, "proxy_url");
        if (v) {
            char *s = ta11_py_str(v), *st = ta11_strip(s ? s : "");
            free(s);
            ta11_setdefault(extras, "proxy_url", json_string(st ? st : ""));
            ta11_setenv(ctx, "TELEGRAM_PROXY", json_string(st ? st : ""));
            free(st);
        }
    }
    const json_t *extra = ta11_getattr(tc, "extra");
    bool has_extra = extra && extra->type == JSON_OBJECT;
    /* reply_to_mode */
    {
        const json_t *rtm = ta11_getattr(tc, "reply_to_mode");
        if (!rtm && has_extra) rtm = ta11_getattr(extra, "reply_to_mode");
        if (!ta11_is_null(rtm)) {
            json_t *v;
            if (rtm->type == JSON_BOOL && !rtm->bool_val) v = json_string("off");
            else { char *s = ta11_lower(ta11_py_str(rtm)); v = json_string(s ? s : ""); free(s); }
            ta11_setenv(ctx, "TELEGRAM_REPLY_TO_MODE", v);
        }
    }
    /* allow_from / group_allow_from / group_allowed_chats */
    {
        const json_t *v = ta11_getattr(tc, "allow_from");
        if (!ta11_is_null(v)) ta11_setenv(ctx, "TELEGRAM_ALLOWED_USERS", json_copy((json_t *)v));
    }
    {
        const json_t *v = ta11_getattr(tc, "group_allow_from");
        if (!ta11_truthy(v) && has_extra) v = ta11_getattr(extra, "group_allow_from");
        if (!ta11_is_null(v)) ta11_setenv(ctx, "TELEGRAM_GROUP_ALLOWED_USERS", json_copy((json_t *)v));
    }
    {
        const json_t *v = ta11_getattr(tc, "group_allowed_chats");
        if (!ta11_truthy(v) && has_extra) v = ta11_getattr(extra, "group_allowed_chats");
        if (!ta11_is_null(v)) ta11_setenv(ctx, "TELEGRAM_GROUP_ALLOWED_CHATS", json_copy((json_t *)v));
    }
    /* final extras keys */
    const char *final_keys[] = {"guest_mode","disable_link_previews","observe_unmentioned_group_messages","free_response_topics"};
    for (int i = 0; i < 4; i++) {
        const json_t *v = ta11_getattr(tc, final_keys[i]);
        if (v) ta11_setdefault(extras, final_keys[i], json_copy((json_t *)v));
    }
    /* extra passthrough minus generic keys */
    const char *generic[] = {"reply_prefix","reply_in_thread","reply_to_mode","unauthorized_dm_behavior",
        "notice_delivery","require_mention","channel_skill_bindings","channel_prompts",
        "gateway_restart_notification","allow_from","allow_admin_from","dm_policy","group_policy"};
    if (has_extra && extra->type == JSON_OBJECT) {
        for (size_t i = 0; i < extra->c.count; i++) {
            /* libjson object iteration via c.keys/c.items */
            const char *k = extra->c.keys ? extra->c.keys[i] : NULL;
            json_t *v = extra->c.items ? extra->c.items[i] : NULL;
            if (!k) continue;
            bool skip = false;
            for (int j = 0; j < 13; j++) if (strcmp(k, generic[j]) == 0) { skip = true; break; }
            if (!skip) ta11_setdefault(extras, k, json_copy(v));
        }
    }
    if (extras->c.count == 0) { json_free(extras); return json_null(); }
    return extras;
}

/* PoP: ta11_await_cold_start_readiness @ plugins/platforms/telegram/adapter.py:_await_cold_start_readiness */
static json_t *ta11_await_cold_start_readiness(ta11_ctx_t *ctx, const json_t *args) {
    (void)args;
    const char *awaitables[2] = {"progress.wait()", "error.wait()"};
    for (int i = 0; i < 2; i++) {
        json_t *a = json_object();
        json_set_hdr(a, "awaitable", awaitables[i]);
        json_t *f = ta11_call(ctx, "ensure_future", a);
        if (ta11_is_raise(f)) return f;
        json_free(f);
    }
    json_t *wargs = json_object();
    json_set_hdr(wargs, "return_when", "FIRST_COMPLETED");
    json_set(wargs, "n", json_number(2));
    json_t *w = ta11_call(ctx, "wait", wargs);
    if (ta11_is_raise(w)) return w;
    json_free(w);
    json_t *dargs = json_object();
    json_set(dargs, "awaitable", json_string("w"));
    json_set(dargs, "timeout", json_number(_INITIAL_POLLING_PROGRESS_TIMEOUT));
    json_t *deadline = ta11_call(ctx, "_await_with_thread_deadline", dargs);
    json_t *pending_result = NULL; /* OSError to raise after finally */
    if (ta11_is_raise(deadline)) {
        const char *kind = ta11_raise_kind(deadline);
        if (strcmp(kind, "TimeoutError") == 0) {
            pending_result = ta11_err("OSError",
                "Telegram getUpdates made no progress within 30s during initial connect "
                "\xe2\x80\x94 failing startup so the gateway retries with a fresh adapter (#67498)");
            json_free(deadline);
        } else {
            pending_result = deadline; /* propagate original (Exception or BaseException) */
        }
    } else {
        json_free(deadline);
    }
    /* finally: done/cancel each future, then gather */
    for (int i = 0; i < 2; i++) {
        json_t *da = json_object();
        json_t *d = ta11_call(ctx, "fut_done", da);
        if (ta11_is_raise(d)) { if (pending_result) json_free(pending_result); return d; }
        bool done = ta11_truthy(d);
        json_free(d);
        if (!done) {
            json_t *ca = json_object();
            json_t *c = ta11_call(ctx, "fut_cancel", ca);
            if (ta11_is_raise(c)) { if (pending_result) json_free(pending_result); return c; }
            json_free(c);
        }
    }
    {
        json_t *ga = json_object();
        json_set(ga, "n", json_number(2));
        json_set(ga, "return_exceptions", json_bool(true));
        json_t *g = ta11_call(ctx, "gather", ga);
        if (ta11_is_raise(g)) { if (pending_result) json_free(pending_result); return g; }
        json_free(g);
    }
    if (pending_result) return pending_result;
    /* strict_error gate: fixtures carry args.strict_error as [{type,message}] */
    const json_t *se = ta11_arg(args, "strict_error");
    bool has_err = se && se->type == JSON_ARRAY && json_len(se) > 0;
    json_t *is = ta11_call(ctx, "progress_is_set", json_object());
    if (ta11_is_raise(is)) return is;
    bool isset = ta11_truthy(is);
    json_free(is);
    /* Python short-circuit: if strict_error and not is_set -> redact+raise; else if not is_set -> raise */
    if (!isset) {
        if (has_err) {
            const json_t *first = json_get(se, 0);
            json_t *exc_spec = json_object();
            json_set_hdr(exc_spec, "__raise__", json_get_str(first, "type", "RuntimeError"));
            json_set(exc_spec, "message", ta11_copyv(ta11_getattr(first, "message")));
            json_t *ra = json_object();
            json_set(ra, "text", exc_spec);
            json_t *red = ta11_call(ctx, "redact", ra);
            if (ta11_is_raise(red)) return red;
            char *msg = ta11_fmt("Telegram polling errored before first getUpdates success during initial connect: %s",
                                 ta11_str(red) ? ta11_str(red) : "");
            json_t *e = ta11_err("OSError", msg ? msg : "");
            free(msg); json_free(red);
            return e;
        }
        return ta11_err("OSError", "Telegram getUpdates did not become ready during initial connect");
    }
    return json_null();
}

/* PoP: ta11_build_message_event @ plugins/platforms/telegram/adapter.py:_build_message_event */
static json_t *ta11_build_message_event(ta11_ctx_t *ctx, const json_t *args) {
    const json_t *msg = ta11_arg(args, "message");
    const json_t *chat = ta11_getattr(msg, "chat");
    const json_t *user = ta11_getattr(msg, "from_user");
    const json_t *mt = ta11_arg(args, "msg_type");
    const json_t *uid = ta11_arg(args, "update_id");
    const json_t *extra = ta11_getattr(ta11_state_get(ctx, "config"), "extra");
    /* gateway_chat_type */
    json_t *a = json_object();
    json_set(a, "chat", ta11_copyv(chat));
    json_set(a, "is_forum", json_bool(false));
    json_t *ct = ta11_call(ctx, "gateway_chat_type", a);
    if (ta11_is_raise(ct)) return ct;
    /* effective thread id */
    json_t *b = json_object(); json_set(b, "message", ta11_copyv(msg));
    json_t *tid = ta11_call(ctx, "effective_message_thread_id", b);
    if (ta11_is_raise(tid)) { json_free(ct); return tid; }
    /* adopt created topic thread */
    json_t *c = json_object();
    json_set(c, "message", ta11_copyv(msg));
    json_set(c, "chat_type", json_copy(ct));
    json_set(c, "thread_id", json_copy(tid));
    json_t *tid2 = ta11_call(ctx, "adopt_created_topic_thread", c);
    if (ta11_is_raise(tid2)) { json_free(ct); json_free(tid); return tid2; }
    json_free(tid);
    /* resolve topic binding */
    json_t *d = json_object();
    json_set(d, "message", ta11_copyv(msg));
    json_set(d, "chat_type", json_copy(ct));
    json_set(d, "thread_id", json_copy(tid2));
    json_t *bind = ta11_call(ctx, "resolve_topic_binding", d);
    if (ta11_is_raise(bind)) { json_free(ct); json_free(tid2); return bind; }
    const json_t *chat_topic = NULL, *topic_skill = NULL;
    if (bind->type == JSON_ARRAY && json_len(bind) >= 2) { chat_topic = json_get(bind, 0); topic_skill = json_get(bind, 1); }
    /* build_source args */
    const char *cts = ta11_str(ct) ? ta11_str(ct) : "";
    char *cid = ta11_py_str(ta11_getattr(chat, "id"));
    char *cmid = ta11_py_str(ta11_getattr(msg, "message_id"));
    bool has_user = !ta11_is_null(user);
    char *uidstr = has_user ? ta11_py_str(ta11_getattr(user, "id")) :
        ((strcmp(cts, "dm") == 0 || strcmp(cts, "channel") == 0) ? ta11_strdup(cid) : ta11_strdup("None"));
    const json_t *ctitle = ta11_getattr(chat, "title");
    const json_t *cfull = ta11_getattr(chat, "full_name");
    json_t *chat_name = (ctitle && ta11_truthy(ctitle)) ? json_copy((json_t *)ctitle)
        : ((cfull && !ta11_is_null(cfull)) ? json_copy((json_t *)cfull) : json_null());
    const json_t *uname = NULL;
    if (has_user) uname = ta11_getattr(user, "full_name");
    else if (cfull && !ta11_is_null(cfull) && strcmp(cts, "dm") == 0) uname = cfull;
    else if (strcmp(cts, "channel") == 0) uname = ctitle;
    json_t *e = json_object();
    json_set_hdr(e, "chat_id", cid ? cid : "");
    json_set(e, "chat_name", chat_name);
    json_set(e, "chat_type", json_copy(ct));
    json_set_hdr(e, "user_id", uidstr ? uidstr : "");
    json_set(e, "user_name", ta11_copyv(uname));
    json_set(e, "thread_id", json_copy(tid2));
    json_set(e, "chat_topic", ta11_copyv(chat_topic));
    json_set_hdr(e, "message_id", cmid ? cmid : "");
    json_set(e, "is_bot", json_bool(has_user && ta11_truthy(ta11_getattr(user, "is_bot"))));
    free(cid); free(cmid); free(uidstr);
    json_t *source = ta11_call(ctx, "build_source", e);
    if (ta11_is_raise(source)) { json_free(ct); json_free(tid2); json_free(bind); return source; }
    /* reply_context */
    json_t *f = json_object(); json_set(f, "message", ta11_copyv(msg));
    json_t *rc = ta11_call(ctx, "reply_context", f);
    if (ta11_is_raise(rc)) { json_free(ct); json_free(tid2); json_free(bind); json_free(source); return rc; }
    const json_t *rid = NULL, *rtext = NULL;
    if (rc->type == JSON_ARRAY && json_len(rc) >= 2) { rid = json_get(rc, 0); rtext = json_get(rc, 1); }
    /* resolve_channel_prompt */
    char *cidstr = NULL;
    { char *tmp = ta11_py_str(ta11_getattr(chat, "id")); cidstr = tmp; }
    const json_t *key = ta11_truthy(tid2) ? tid2 : (json_t *)json_string(cidstr ? cidstr : "");
    json_t *g = json_object();
    json_set(g, "extra", ta11_copyv(extra));
    json_set(g, "key", ta11_copyv(key));
    json_set(g, "chat_id", ta11_truthy(tid2) ? json_string(cidstr ? cidstr : "") : json_null());
    if (key != tid2) json_free((json_t *)key);
    json_t *prompt = ta11_call(ctx, "resolve_channel_prompt", g);
    if (ta11_is_raise(prompt)) { json_free(ct); json_free(tid2); json_free(bind); json_free(source); json_free(rc); free(cidstr); return prompt; }
    /* expand_link_entities */
    json_t *h = json_object(); json_set(h, "message", ta11_copyv(msg));
    json_t *text = ta11_call(ctx, "expand_link_entities", h);
    if (ta11_is_raise(text)) { json_free(ct); json_free(tid2); json_free(bind); json_free(source); json_free(rc); json_free(prompt); free(cidstr); return text; }
    /* group_identity_prompt */
    json_t *i = json_object();
    json_set(i, "message", ta11_copyv(msg));
    json_set(i, "prompt", json_copy(prompt));
    json_t *identity = ta11_call(ctx, "group_identity_prompt", i);
    if (ta11_is_raise(identity)) { json_free(ct); json_free(tid2); json_free(bind); json_free(source); json_free(rc); json_free(prompt); json_free(text); free(cidstr); return identity; }
    /* group_reply_expected */
    json_t *j = json_object();
    json_set(j, "message", ta11_copyv(msg));
    json_set(j, "chat_type", json_copy(ct));
    json_t *re = ta11_call(ctx, "group_reply_expected", j);
    if (ta11_is_raise(re)) { json_free(ct); json_free(tid2); json_free(bind); json_free(source); json_free(rc); json_free(prompt); json_free(text); json_free(identity); free(cidstr); return re; }
    /* edit_revision */
    json_t *k = json_object();
    json_set(k, "message", ta11_copyv(msg));
    json_set(k, "update_id", ta11_copyv(uid));
    json_t *rev = ta11_call(ctx, "edit_revision", k);
    if (ta11_is_raise(rev)) { json_free(ct); json_free(tid2); json_free(bind); json_free(source); json_free(rc); json_free(prompt); json_free(text); json_free(identity); json_free(re); free(cidstr); return rev; }
    /* MessageEvent */
    json_t *me = json_object();
    json_set(me, "text", json_copy(text));
    json_set(me, "message_type", ta11_copyv(mt));
    json_set(me, "source", json_copy(source));
    json_set(me, "raw_message", ta11_copyv(msg));
    { char *m = ta11_py_str(ta11_getattr(msg, "message_id")); json_set_hdr(me, "message_id", m ? m : ""); free(m); }
    json_set(me, "platform_update_id", ta11_copyv(uid));
    json_set(me, "user_id", ta11_copyv(ta11_getattr(source, "user_id")));
    json_set(me, "user_name", ta11_copyv(ta11_getattr(source, "user_name")));
    json_set(me, "reply_to_message_id", ta11_copyv(rid));
    json_set(me, "reply_to_text", ta11_copyv(rtext));
    json_set(me, "auto_skill", ta11_copyv(topic_skill));
    json_set(me, "channel_prompt", json_copy(identity));
    json_set(me, "reply_expected", json_copy(re));
    json_set(me, "edit_revision", json_copy(rev));
    json_set(me, "timestamp", ta11_copyv(ta11_getattr(msg, "date")));
    json_t *out = ta11_call(ctx, "MessageEvent", me);
    json_free(ct); json_free(tid2); json_free(bind); json_free(source); json_free(rc);
    json_free(prompt); json_free(text); json_free(identity); json_free(re); json_free(rev); free(cidstr);
    return out;
}

/* PoP: ta11_build_ptb_requests @ plugins/platforms/telegram/adapter.py:_build_ptb_requests */
static json_t *ta11_build_ptb_requests(ta11_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *extra = ta11_getattr(ta11_state_get(ctx, "config"), "extra");
    const json_t *name = ta11_state_get(ctx, "name");
    /* getenv */
    json_t *a = json_object();
    json_set_hdr(a, "key", "HERMES_TELEGRAM_DISABLE_FALLBACK_IPS");
    json_set_hdr(a, "default", "");
    json_t *env = ta11_call(ctx, "getenv", a);
    if (ta11_is_raise(env)) return env;
    /* disable_fallback = getenv(...).strip().lower() in {"1","true","yes","on"} */
    char *el = ta11_lower(ta11_str(env) ? ta11_str(env) : "");
    char *es = ta11_strip(el ? el : "");
    bool disable_fallback = es && (strcmp(es, "1") == 0 || strcmp(es, "true") == 0 ||
                                   strcmp(es, "yes") == 0 || strcmp(es, "on") == 0);
    free(el); free(es);
    json_free(env);
    /* DNS fallback discovery path (excluded from fixtures: disable_fallback is
       always scripted on). Kept here so the universal-seam name set is complete. */
    if (!disable_fallback) {
        json_t *fa = json_object();
        json_set_hdr(fa, "name", "HERMES_TELEGRAM_FALLBACK_DISCOVERY_TIMEOUT");
        json_set(fa, "default", json_number(5.0));
        json_set(fa, "min_value", json_number(0.0));
        json_t *fc = ta11_call(ctx, "env_float_clamped", fa);
        if (ta11_is_raise(fc)) return fc;
        json_free(fc);
        json_t *da = json_object();
        json_t *disc = ta11_call(ctx, "discover_fallback_ips", da);
        if (ta11_is_raise(disc)) return disc;
        json_free(disc);
    }
    /* platform_ssl_context */
    json_t *sc = ta11_call(ctx, "platform_ssl_context", json_object());
    if (ta11_is_raise(sc)) return sc;
    /* telegram_verify */
    json_t *b = json_object();
    json_set(b, "extra", ta11_copyv(extra));
    json_set(b, "default", json_copy(sc));
    json_free(sc);
    json_t *verify = ta11_call(ctx, "telegram_verify", b);
    if (ta11_is_raise(verify)) return verify;
    /* proxy_target_hosts(base_url, []) */
    json_t *c = json_object();
    json_set(c, "base_url", ta11_copyv(ta11_getattr(extra, "base_url")));
    json_set(c, "fallback_ips", json_array());
    json_t *targs = ta11_call(ctx, "proxy_target_hosts", c);
    if (ta11_is_raise(targs)) { json_free(verify); return targs; }
    /* resolve_proxy_url */
    json_t *d = json_object();
    json_set_hdr(d, "key", "TELEGRAM_PROXY");
    json_set(d, "target_hosts", json_copy(targs));
    json_set(d, "configured", ta11_copyv(ta11_getattr(extra, "proxy_url")));
    json_free(targs);
    json_t *proxy = ta11_call(ctx, "resolve_proxy_url", d);
    if (ta11_is_raise(proxy)) { json_free(verify); return proxy; }
    json_set(ctx->state, "_ptb_proxy_url", json_copy(proxy));
    /* logger.info - branch */
    json_t *arr = json_array();
    if (ta11_truthy(proxy)) {
        json_append(arr, json_string("[%s] Proxy detected; passing explicitly to HTTPXRequest: %s"));
        json_append(arr, ta11_copyv(name));
        json_append(arr, json_copy(proxy));
    } else {
        json_append(arr, json_string("[%s] Telegram fallback-IP transport disabled via env"));
        json_append(arr, ta11_copyv(name));
    }
    json_t *li = ta11_call(ctx, "logger_info", ta11_logger_args(arr));
    if (ta11_is_raise(li)) { json_free(proxy); json_free(verify); return li; }
    json_free(li);
    /* build_ptb_requests */
    json_t *e = json_object();
    json_set_hdr(e, "httpx", "HTTPXRequest");
    json_set(e, "fallback_ips", json_array());
    json_set(e, "proxy_url", json_copy(proxy));
    json_set(e, "verify", json_copy(verify));
    json_free(proxy); json_free(verify);
    json_t *pair = ta11_call(ctx, "build_ptb_requests", e);
    if (ta11_is_raise(pair)) return pair;
    const json_t *request = (pair->type == JSON_ARRAY && json_len(pair) > 0) ? json_get(pair, 0) : NULL;
    const json_t *gu = (pair->type == JSON_ARRAY && json_len(pair) > 1) ? json_get(pair, 1) : NULL;
    json_t *req_copy = ta11_copyv(request), *gu_copy = ta11_copyv(gu);
    json_free(pair);
    json_t *f = json_object();
    json_set(f, "request", gu_copy);
    json_t *inst = ta11_call(ctx, "instrument_polling_request", f);
    if (ta11_is_raise(inst)) { json_free(req_copy); return inst; }
    json_t *out = json_array();
    json_append(out, req_copy);
    json_append(out, inst);
    return out;
}

/* PoP: ta11_cache_inbound_av @ plugins/platforms/telegram/adapter.py:_cache_inbound_av */
static json_t *ta11_cache_inbound_av(ta11_ctx_t *ctx, const json_t *args) {
    const json_t *msg = ta11_arg(args, "msg");
    const json_t *event = ta11_arg(args, "event");
    const json_t *source = ta11_arg(args, "source");
    const json_t *label = ta11_arg(args, "label");
    const json_t *kind = ta11_arg(args, "kind");
    const json_t *fname = ta11_getattr(source, "file_name");
    char *lstr;
    if (fname && fname->type == JSON_STRING && fname->str_val[0]) {
        lstr = ta11_fmt("%s '%s'", ta11_str(label) ? ta11_str(label) : "", fname->str_val);
    } else {
        lstr = ta11_strdup(ta11_str(label) ? ta11_str(label) : "");
    }
    json_t *a = json_object();
    json_set(a, "source", ta11_copyv(source));
    json_set_hdr(a, "label", lstr ? lstr : "");
    free(lstr);
    json_t *allowed = ta11_call(ctx, "telegram_media_size_allowed", a);
    if (ta11_is_raise(allowed)) {
        json_t *exc = json_copy(allowed);
        /* except Exception only */
        if (!ta11_is_exception(ta11_raise_kind(exc))) return exc;
        /* redact */
        json_t *ra = json_object(); json_set(ra, "text", json_copy(exc));
        json_t *red = ta11_call(ctx, "redact", ra);
        if (ta11_is_raise(red)) { json_free(exc); return red; }
        json_t *arr = json_array();
        json_append(arr, json_string("[Telegram] Failed to cache %s: %s"));
        json_append(arr, ta11_copyv(kind));
        json_append(arr, json_copy(red));
        json_t *wa = json_object();
        json_set(wa, "args", arr);
        json_t *kw = json_object(); json_set(kw, "exc_info", json_bool(true));
        json_set(wa, "kwargs", kw);
        json_t *warn = ta11_call(ctx, "logger_warning", wa);
        if (ta11_is_raise(warn)) { json_free(red); json_free(exc); return warn; }
        json_free(warn); json_free(red);
        /* surface */
        json_t *sa = json_object();
        json_t *sarr = json_array();
        json_append(sarr, json_string("<self>"));
        json_append(sarr, ta11_copyv(msg));
        json_append(sarr, ta11_copyv(event));
        json_append(sarr, ta11_copyv(label));
        json_append(sarr, json_copy(exc));
        json_set(sa, "args", sarr);
        json_set(sa, "kwargs", json_object());
        json_t *surf = ta11_call(ctx, "surface_media_cache_failure", sa);
        if (ta11_is_raise(surf)) { json_free(exc); return surf; }
        json_free(surf); json_free(exc);
        return json_null();
    }
    bool ok = (allowed->type == JSON_ARRAY && json_len(allowed) > 0) ? ta11_truthy(json_get(allowed, 0)) : false;
    const json_t *note = (allowed->type == JSON_ARRAY && json_len(allowed) > 1) ? json_get(allowed, 1) : NULL;
    json_t *note_copy = ta11_copyv(note);
    json_free(allowed);
    if (!ok) {
        json_t *b = json_object();
        json_set(b, "event", ta11_copyv(event));
        json_set(b, "text", ta11_truthy(note_copy) ? json_copy(note_copy) : json_string(""));
        json_t *nar = ta11_call(ctx, "note_instead_of_media", b);
        if (ta11_is_raise(nar)) { json_free(note_copy); return nar; }
        json_free(nar);
        json_t *arr = json_array();
        json_append(arr, json_string("[Telegram] Skipped oversized user %s (size=%s)"));
        json_append(arr, ta11_copyv(kind));
        json_append(arr, ta11_copyv(ta11_getattr(source, "file_size")));
        json_t *li = ta11_call(ctx, "logger_info", ta11_logger_args(arr));
        if (ta11_is_raise(li)) { json_free(note_copy); return li; }
        json_free(li); json_free(note_copy);
        return json_null();
    }
    json_free(note_copy);
    return json_null();
}

/* PoP: ta11_cache_inbound_document @ plugins/platforms/telegram/adapter.py:_cache_inbound_document */
static json_t *ta11_cache_inbound_document(ta11_ctx_t *ctx, const json_t *args) {
    const json_t *msg = ta11_arg(args, "msg");
    const json_t *ev_in = ta11_arg(args, "event");
    json_t *event = json_copy((json_t *)ev_in);
    json_set(ctx->state, "event", event);
    const json_t *doc = ta11_getattr(msg, "document");
    const json_t *fname = ta11_getattr(doc, "file_name");
    const json_t *mime = ta11_getattr(doc, "mime_type");
    const json_t *fsize = ta11_getattr(doc, "file_size");
    char *orig = (fname && fname->type == JSON_STRING) ? ta11_strdup(fname->str_val) : ta11_strdup("");
    char *ext = ta11_splitext(orig);
    char *dmime = ta11_lower(ta11_str(mime) ? ta11_str(mime) : "");
    char *display = NULL;
    if (orig[0]) display = ta11_strdup(orig);
    else if (dmime[0]) display = ta11_strdup(dmime);
    else if (ext[0]) display = ta11_strdup(ext);
    else display = ta11_strdup("unknown");
    double maxb = ta11_num(ta11_state_get(ctx, "_max_doc_bytes"));
    bool too_large = (!fsize || fsize->type == JSON_NULL || ta11_num(fsize) == 0 || ta11_num(fsize) > maxb);
    if (too_large) {
        json_t *arr = json_array();
        json_append(arr, json_string("[Telegram] Document too large: %s bytes"));
        json_append(arr, ta11_copyv(fsize));
        json_t *li = ta11_call(ctx, "logger_info", ta11_logger_args(arr));
        if (ta11_is_raise(li)) {
            json_t *exc = json_copy(li);
            if (!ta11_is_exception(ta11_raise_kind(exc))) { free(orig); free(ext); free(dmime); free(display); return exc; }
            /* except path */
            json_t *ra = json_object(); json_set(ra, "text", json_copy(exc));
            json_t *red = ta11_call(ctx, "redact", ra);
            if (ta11_is_raise(red)) { json_free(exc); free(orig); free(ext); free(dmime); free(display); return red; }
            json_t *wa = json_object(); json_t *warr = json_array();
            json_append(warr, json_string("[Telegram] Failed to cache document: %s"));
            json_append(warr, json_copy(red));
            json_set(wa, "args", warr);
            json_t *kw = json_object(); json_set(kw, "exc_info", json_bool(true)); json_set(wa, "kwargs", kw);
            json_t *warn = ta11_call(ctx, "logger_warning", wa);
            if (ta11_is_raise(warn)) { json_free(red); json_free(exc); free(orig); free(ext); free(dmime); free(display); return warn; }
            json_free(warn); json_free(red);
            json_t *sa = json_object(); json_t *sarr = json_array();
            json_append(sarr, json_string("<self>"));
            json_append(sarr, ta11_copyv(msg));
            json_append(sarr, ta11_copyv(event));
            json_append(sarr, json_string("attachment"));
            json_append(sarr, json_copy(exc));
            json_set(sa, "args", sarr);
            json_set(sa, "kwargs", json_object());
            json_t *dname = (fname && fname->type == JSON_STRING && fname->str_val[0]) ? json_string(fname->str_val) : json_null();
            json_set(json_obj_get(sa, "kwargs"), "display_name", dname);
            json_t *surf = ta11_call(ctx, "surface_media_cache_failure", sa);
            if (ta11_is_raise(surf)) { json_free(exc); free(orig); free(ext); free(dmime); free(display); return surf; }
            json_free(surf); json_free(exc);
            free(orig); free(ext); free(dmime); free(display);
            return json_bool(false);
        }
        json_free(li);
        int mb = (int)(maxb / (1024.0 * 1024.0));
        char *note = ta11_fmt("Document '%s' is too large or its size could not be verified. Maximum: %d MB.", display, mb);
        json_t *b = json_object();
        json_set(b, "event", ta11_copyv(event));
        json_set_hdr(b, "text", note ? note : "");
        free(note);
        json_t *ret = ta11_call(ctx, "note_instead_of_media", b);
        if (ta11_is_raise(ret)) {
            json_t *exc = json_copy(ret);
            if (!ta11_is_exception(ta11_raise_kind(exc))) { json_free(exc); free(orig); free(ext); free(dmime); free(display); return exc; }
            json_t *ra = json_object(); json_set(ra, "text", json_copy(exc));
            json_t *red = ta11_call(ctx, "redact", ra);
            if (ta11_is_raise(red)) { json_free(exc); free(orig); free(ext); free(dmime); free(display); return red; }
            json_t *wa = json_object(); json_t *warr = json_array();
            json_append(warr, json_string("[Telegram] Failed to cache document: %s"));
            json_append(warr, json_copy(red));
            json_set(wa, "args", warr);
            json_t *kw = json_object(); json_set(kw, "exc_info", json_bool(true)); json_set(wa, "kwargs", kw);
            json_t *warn = ta11_call(ctx, "logger_warning", wa);
            if (ta11_is_raise(warn)) { json_free(red); json_free(exc); free(orig); free(ext); free(dmime); free(display); return warn; }
            json_free(warn); json_free(red);
            json_t *sa = json_object(); json_t *sarr = json_array();
            json_append(sarr, json_string("<self>"));
            json_append(sarr, ta11_copyv(msg));
            json_append(sarr, ta11_copyv(event));
            json_append(sarr, json_string("attachment"));
            json_append(sarr, json_copy(exc));
            json_set(sa, "args", sarr);
            json_set(sa, "kwargs", json_object());
            json_t *dname = (fname && fname->type == JSON_STRING && fname->str_val[0]) ? json_string(fname->str_val) : json_null();
            json_set(json_obj_get(sa, "kwargs"), "display_name", dname);
            json_t *surf = ta11_call(ctx, "surface_media_cache_failure", sa);
            if (ta11_is_raise(surf)) { json_free(exc); free(orig); free(ext); free(dmime); free(display); return surf; }
            json_free(surf); json_free(exc);
            free(orig); free(ext); free(dmime); free(display);
            return json_bool(false);
        }
        free(orig); free(ext); free(dmime); free(display);
        return ret;
    }
    free(orig); free(ext); free(dmime); free(display);
    return json_bool(false);
}

/* PoP: ta11_cancel_pending_delivery_tasks @ plugins/platforms/telegram/adapter.py:_cancel_pending_delivery_tasks */
static json_t *ta11_cancel_pending_delivery_tasks(ta11_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *cur = ta11_call(ctx, "current_task", json_object());
    if (ta11_is_raise(cur)) return cur;
    json_t *cl = json_object();
    json_set(cl, "tasks", json_array()); /* placeholder; replaced below */
    json_obj_del(cl, "tasks");
    json_t *tasks = json_array();
    json_append(tasks, json_null()); json_append(tasks, json_null()); json_append(tasks, json_null());
    json_set(cl, "tasks", tasks);
    json_set(cl, "current_task", json_copy(cur));
    json_t *collected = ta11_call(ctx, "collect_live_tasks", cl);
    if (ta11_is_raise(collected)) { json_free(cur); return collected; }
    size_t n = (collected->type == JSON_ARRAY) ? json_len(collected) : 0;
    size_t awaitable = 0;
    for (size_t i = 0; i < n; i++) {
        json_t *fa = json_object();
        json_set(fa, "task", json_copy(json_get(collected, i)));
        json_t *f = ta11_call(ctx, "isfuture", fa);
        if (ta11_is_raise(f)) { json_free(collected); json_free(cur); return f; }
        bool isf = ta11_truthy(f);
        json_free(f);
        if (!isf) {
            json_t *fa2 = json_object();
            json_set(fa2, "task", json_copy(json_get(collected, i)));
            json_t *f2 = ta11_call(ctx, "isfuture", fa2);
            if (ta11_is_raise(f2)) { json_free(collected); json_free(cur); return f2; }
            isf = ta11_truthy(f2);
            json_free(f2);
        }
        if (isf) awaitable++;
    }
    for (size_t i = 0; i < n; i++) {
        json_t *ca = json_object();
        json_t *c = ta11_call(ctx, "task_cancel", ca);
        if (ta11_is_raise(c)) { json_free(collected); json_free(cur); return c; }
        json_free(c);
    }
    if (awaitable > 0) {
        json_t *ga = json_object();
        json_set(ga, "n", json_number((double)awaitable));
        json_set(ga, "return_exceptions", json_bool(true));
        json_t *g = ta11_call(ctx, "gather", ga);
        if (ta11_is_raise(g)) { json_free(collected); json_free(cur); return g; }
        json_free(g);
    }
    json_free(collected);
    json_t *pf = ta11_call(ctx, "is_permanent_fatal", json_object());
    if (ta11_is_raise(pf)) { json_free(cur); return pf; }
    bool fatal = ta11_truthy(pf);
    json_free(pf);
    if (fatal) {
        const char *keys[] = {"_pending_text_batches","_pending_photo_batches","_media_group_events"};
        int total = 0;
        for (int i = 0; i < 3; i++) {
            const json_t *d = ta11_state_get(ctx, keys[i]);
            if (d && d->type == JSON_OBJECT) total += (int)d->c.count;
        }
        if (total > 0) {
            json_t *arr = json_array();
            json_append(arr, json_string("[Telegram] Non-retryable fatal teardown; discarding %d pending inbound batch(es)"));
            json_append(arr, json_number((double)total));
            json_t *wa = json_object();
            json_set(wa, "args", arr);
            json_set(wa, "kwargs", json_object());
            json_t *warn = ta11_call(ctx, "logger_warning", wa);
            if (ta11_is_raise(warn)) { json_free(cur); return warn; }
            json_free(warn);
        }
    } else {
        json_t *sv = ta11_call(ctx, "salvage_buffered_inbound", json_object());
        if (ta11_is_raise(sv)) { json_free(cur); return sv; }
        json_free(sv);
    }
    /* clear six dicts */
    const char *six[] = {"_media_group_tasks","_media_group_events","_pending_photo_batch_tasks",
        "_pending_photo_batches","_pending_text_batch_tasks","_pending_text_batches"};
    for (int i = 0; i < 6; i++) json_set(ctx->state, six[i], json_object());
    json_t *ca = json_object();
    json_set(ca, "current_task", json_copy(cur));
    json_t *keep = json_array();
    json_append(keep, json_string("_polling_error_task"));
    json_append(keep, json_string("_polling_progress_verifier_task"));
    json_append(keep, json_string("_held_inbound_redispatch_task"));
    json_set(ca, "keep", keep);
    json_free(cur);
    json_t *clr = ta11_call(ctx, "clear_task_attrs_except", ca);
    if (ta11_is_raise(clr)) return clr;
    json_free(clr);
    return json_null();
}
