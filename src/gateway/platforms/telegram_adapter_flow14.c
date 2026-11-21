/*
 * telegram_adapter_flow14.c - Telegram adapter FLOW part 14 of
 * plugins/platforms/telegram/adapter.py (hermes-agent): 16 remaining UNCLAIMED
 * defs (the callback/finalize/batch/auth helpers). No flow 1-13 target is
 * re-ported. Every target below was confirmed UNCLAIMED in the dotted-name
 * aware live parity scan.
 *
 * Themes: thin flush wrappers, DM-topic lookup, request-drain lock, fatal
 * network handoff, command handling, inline-keyboard callbacks (choice,
 * model, exec-approval, gmail-triage, clarify, inline search) and pure
 * attribute/send helpers. All live I/O and injected callables enter through
 * ONE universal seam: ta14_call(ctx, name, args).
 *
 * Self-contained: harness #includes this .c directly; libjson only.
 * Every symbol is static and ta14_-prefixed.
 *
 * Representation contract (shared with tests/oracle_telegram_adapter_flow14.py):
 * - NULL json_t* == Python None / absent; JSON_NULL is a real null value.
 * - Seam scripts: a plain value repeats; a LIST is consumed per call (last
 *   repeats); {"__script__":true,"ret":v,"raise":{type,message}} forces a
 *   result or raise. Missing/exhausted script => fail-closed RuntimeError
 *   recorded in the trace before propagation (BaseException passthrough).
 * - Except sites swallow ONLY Exception-family kinds (TA14_EXC_HIER);
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

#define _TOAST_LIMIT 200
#define _SPLIT_DEFAULT 4000

typedef struct ta14_ctx {
    json_t *state;        /* owned; carries the attribute-double fields */
    const json_t *io;     /* borrowed scripted seam results */
    json_t *cursors;      /* owned; per-name array cursors */
    json_t *trace;        /* owned; ordered seam events */
    const json_t *errors; /* borrowed placeholder */
} ta14_ctx_t;

/* ── basic helpers ── */
static bool ta14_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *ta14_raise_kind(const json_t *v) {
    return ta14_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static const char *ta14_raise_msg(const json_t *v) {
    return ta14_is_raise(v) ? json_get_str(v, "message", "") : "";
}
static bool ta14_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool ta14_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *ta14_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *ta14_copyv(const json_t *v) { return v ? json_copy((json_t *)v) : json_null(); }
static json_t *ta14_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static const json_t *ta14_arg(const json_t *args, const char *key) {
    return args ? json_obj_get(args, key) : NULL;
}
static const json_t *ta14_getattr(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
static const json_t *ta14_state_get(const ta14_ctx_t *ctx, const char *key) {
    return (ctx && ctx->state && ctx->state->type == JSON_OBJECT)
        ? json_obj_get(ctx->state, key) : NULL;
}
static double ta14_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static bool ta14_has(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}
static bool ta14_json_eq(const json_t *a, const json_t *b) {
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
                if (!ta14_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                if (!a->c.keys[i] || !b->c.keys[i] || strcmp(a->c.keys[i], b->c.keys[i]) != 0)
                    return false;
                if (!ta14_json_eq(a->c.items[i], b->c.items[i])) return false;
            }
            return true;
        default: return false;
    }
}

/* ── string helpers ── */
static char *ta14_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static char *ta14_lower(const char *s) {
    char *o = ta14_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static char *ta14_fmt(const char *fmt, ...) {
    char tmp[2400]; va_list ap;
    va_start(ap, fmt); int n = vsnprintf(tmp, sizeof tmp, fmt, ap); va_end(ap);
    if (n < 0) return ta14_strdup("");
    char *o = malloc((size_t)n + 1);
    if (!o) return ta14_strdup("");
    va_start(ap, fmt); vsnprintf(o, (size_t)n + 1, fmt, ap); va_end(ap);
    return o;
}
static char *ta14_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return ta14_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return ta14_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e16) return ta14_fmt("%.0f", d);
            return ta14_fmt("%.17g", d);
        }
        case JSON_STRING: return ta14_strdup(v->str_val ? v->str_val : "");
        default: { char *s = json_serialize((json_t *)v); return s ? s : ta14_strdup(""); }
    }
}
static char *ta14_join(const json_t *arr, const char *sep) {
    if (!arr || arr->type != JSON_ARRAY) return ta14_strdup("");
    size_t cap = 64, len = 0;
    char *o = malloc(cap);
    if (!o) return ta14_strdup("");
    o[0] = '\0';
    for (size_t i = 0; i < arr->c.count; i++) {
        char *s = ta14_py_str(arr->c.items[i]);
        size_t sl = strlen(s ? s : ""), spl = strlen(sep);
        size_t need = len + sl + spl + 1;
        if (need > cap) {
            while (cap < need) cap *= 2;
            char *nb = realloc(o, cap);
            if (!nb) { free(o); free(s); return ta14_strdup(""); }
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
static int ta14_split3(const char *s, char **a, char **b, char **c) {
    if (!s) return 0;
    const char *p1 = strchr(s, ':');
    if (!p1) { *a = ta14_strdup(s); return 1; }
    *a = (char *)malloc((size_t)(p1 - s) + 1);
    memcpy(*a, s, (size_t)(p1 - s)); (*a)[p1 - s] = '\0';
    const char *p2 = strchr(p1 + 1, ':');
    if (!p2) {
        *b = ta14_strdup(p1 + 1); *c = ta14_strdup(""); return 2;
    }
    *b = (char *)malloc((size_t)(p2 - p1 - 1) + 1);
    memcpy(*b, p1 + 1, (size_t)(p2 - p1 - 1)); (*b)[p2 - p1 - 1] = '\0';
    *c = ta14_strdup(p2 + 1);
    return 3;
}

/* int(v) with CPython-style ValueError on failure. */
static bool ta14_parse_int(const json_t *v, double *out) {
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
typedef struct { const char *name; const char *parent; } ta14_exc_row_t;
static const ta14_exc_row_t TA14_EXC_HIER[] = {
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
static const char *ta14_builtin_parent(const char *name) {
    for (size_t i = 0; i < sizeof(TA14_EXC_HIER) / sizeof(TA14_EXC_HIER[0]); i++)
        if (strcmp(name, TA14_EXC_HIER[i].name) == 0) return TA14_EXC_HIER[i].parent;
    return NULL;
}
static bool ta14_chain_has(const char *start, const char *target) {
    const char *cur = start;
    for (int depth = 0; cur && depth < 24; depth++) {
        if (strcmp(cur, target) == 0) return true;
        cur = ta14_builtin_parent(cur);
    }
    return false;
}
static bool ta14_is_exception(const char *kind) {
    if (!kind) return false;
    if (ta14_builtin_parent(kind) == NULL) return true;
    return ta14_chain_has(kind, "Exception");
}

/* ── the one universal seam ── */
static json_t *ta14_call(ta14_ctx_t *ctx, const char *name, json_t *args);

/* logger helper ── */
static json_t *ta14_logger_args(const json_t *arr) {
    json_t *a = json_object();
    json_set(a, "args", arr ? arr : json_array());
    return a;
}
/* toast key -> {"args":[key],"kwargs":{...}} */
static json_t *ta14_toast_kw(ta14_ctx_t *ctx, const char *key, json_t *kwargs) {
    json_t *a = json_object();
    json_t *arr = json_array();
    json_append(arr, json_string(key));
    json_set(a, "args", arr);
    json_set(a, "kwargs", kwargs ? kwargs : json_object());
    return ta14_call(ctx, "toast", a);
}
static json_t *ta14_toast(ta14_ctx_t *ctx, const char *key) {
    return ta14_toast_kw(ctx, key, NULL);
}

/* ── the one universal seam ── */
static json_t *ta14_call(ta14_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256]; snprintf(msg, sizeof msg, "no script for %s", name);
        ret = ta14_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256]; snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = ta14_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT)
                ret = ta14_err(json_get_str(raise, "type", "RuntimeError"),
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

/* ═══════════════════════ ported flows ═══════════════════════ */

/* PoP: ta14_should_drop_delayed_delivery @ plugins/platforms/telegram/adapter.py:_should_drop_delayed_delivery */
static json_t *ta14_should_drop_delayed_delivery(ta14_ctx_t *ctx, const json_t *args) {
    (void)args;
    return json_bool(ta14_truthy(ta14_state_get(ctx, "_drop_delayed_deliveries")));
}

/* PoP: ta14_send_retry_is_final @ plugins/platforms/telegram/adapter.py:_send_retry_is_final */
static json_t *ta14_send_retry_is_final(ta14_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *raw = ta14_getattr(ta14_arg(args, "result"), "raw_response");
    if (!raw || raw->type != JSON_OBJECT) return json_bool(false);
    return json_bool(ta14_truthy(json_obj_get(raw, "delivery_uncertain")));
}

/* PoP: ta14_observed_media_source @ plugins/platforms/telegram/adapter.py:_observed_media_source */
static json_t *ta14_observed_media_source(ta14_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *msg = ta14_arg(args, "msg");
    json_t *arr = json_array();
    const json_t *photo = ta14_getattr(msg, "photo");
    if (ta14_truthy(photo) && photo->type == JSON_ARRAY && photo->c.count > 0) {
        json_append(arr, ta14_copyv(photo->c.items[photo->c.count - 1]));
        json_append(arr, json_string("")); json_append(arr, json_string(""));
        json_append(arr, json_string("image")); return arr;
    }
    const json_t *v = ta14_getattr(msg, "video");
    const json_t *vn = ta14_getattr(msg, "video_note");
    const json_t *vid = ta14_truthy(v) ? v : (ta14_truthy(vn) ? vn : NULL);
    if (vid) {
        json_append(arr, ta14_copyv(vid)); json_append(arr, json_string(""));
        json_append(arr, json_string("video/mp4")); json_append(arr, json_string("video"));
        return arr;
    }
    const json_t *voice = ta14_getattr(msg, "voice");
    if (ta14_truthy(voice)) {
        json_append(arr, ta14_copyv(voice)); json_append(arr, json_string("voice.ogg"));
        json_append(arr, json_string("audio/ogg")); json_append(arr, json_string("audio"));
        return arr;
    }
    const json_t *audio = ta14_getattr(msg, "audio");
    if (ta14_truthy(audio)) {
        const char *fn = ta14_str(ta14_getattr(audio, "file_name"));
        json_append(arr, ta14_copyv(audio));
        json_append(arr, json_string(fn ? fn : "")); json_append(arr, json_string(""));
        json_append(arr, json_string("audio")); return arr;
    }
    const json_t *doc = ta14_getattr(msg, "document");
    if (ta14_truthy(doc)) {
        const char *fn = ta14_str(ta14_getattr(doc, "file_name"));
        const char *mt = ta14_str(ta14_getattr(doc, "mime_type"));
        char *low = ta14_lower(mt ? mt : "");
        json_append(arr, ta14_copyv(doc));
        json_append(arr, json_string(fn ? fn : ""));
        json_append(arr, json_string(low ? low : "")); free(low);
        json_append(arr, json_null()); return arr;
    }
    json_append(arr, json_null()); json_append(arr, json_string(""));
    json_append(arr, json_string("")); json_append(arr, json_null());
    return arr;
}

/* PoP: ta14_flush_media_group_event @ plugins/platforms/telegram/adapter.py:_flush_media_group_event */
static json_t *ta14_flush_media_group_event(ta14_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "self", json_string("<self>"));
    json_set(a, "media_group_id", ta14_copyv(ta14_arg(args, "media_group_id")));
    json_t *r = ta14_call(ctx, "flush_media_group_event", a);
    if (ta14_is_raise(r)) return r;
    json_free(r); return json_null();
}

/* PoP: ta14_flush_photo_batch @ plugins/platforms/telegram/adapter.py:_flush_photo_batch */
static json_t *ta14_flush_photo_batch(ta14_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    const json_t *pb = ta14_state_get(ctx, "_pending_photo_batches");
    const json_t *pt = ta14_state_get(ctx, "_pending_photo_batch_tasks");
    json_set(a, "pending", pb ? json_copy((json_t *)pb) : json_object());
    json_set(a, "tasks", pt ? json_copy((json_t *)pt) : json_object());
    json_set(a, "key", ta14_copyv(ta14_arg(args, "batch_key")));
    const json_t *del = ta14_state_get(ctx, "_media_batch_delay_seconds");
    json_set(a, "delay", del ? json_copy((json_t *)del) : json_number(0));
    json_set(a, "where", json_string("photo"));
    json_set(a, "log_fn", json_string("<lambda>"));
    json_set(a, "pause", json_null());
    json_t *r = ta14_call(ctx, "flush_buffered", a);
    if (ta14_is_raise(r)) return r;
    json_free(r); return json_null();
}

/* PoP: ta14_flush_text_batch @ plugins/platforms/telegram/adapter.py:_flush_text_batch */
static json_t *ta14_flush_text_batch(ta14_ctx_t *ctx, const json_t *args) {
    const json_t *key = ta14_arg(args, "key");
    const json_t *pb = ta14_state_get(ctx, "_pending_text_batches");
    if (!pb || pb->type != JSON_OBJECT) {
        pb = json_object();
        json_set(ctx->state, "_pending_text_batches", (json_t *)pb);
    }
    json_t *ga = json_object();
    json_set(ga, "pending", pb ? json_copy((json_t *)pb) : json_object());
    json_set(ga, "key", ta14_copyv(key));
    json_t *existing = ta14_call(ctx, "text_batches_get", ga);
    if (ta14_is_raise(existing)) return existing;
    json_t *da = json_object();
    json_set(da, "existing", json_copy(existing));
    json_t *delay = ta14_call(ctx, "text_batch_delay_for", da);
    if (ta14_is_raise(delay)) { json_free(existing); return delay; }
    json_t *a = json_object();
    json_set(a, "pending", pb ? json_copy((json_t *)pb) : json_object());
    const json_t *pt = ta14_state_get(ctx, "_pending_text_batch_tasks");
    json_set(a, "tasks", pt ? json_copy((json_t *)pt) : json_object());
    json_set(a, "key", ta14_copyv(key));
    json_set(a, "delay", json_copy(delay));
    json_set(a, "where", json_string("text"));
    json_set(a, "log_fn", json_string("<lambda>"));
    json_set(a, "pause", json_string("<lambda>"));
    json_free(delay); json_free(existing);
    json_t *r = ta14_call(ctx, "flush_buffered", a);
    if (ta14_is_raise(r)) return r;
    json_free(r); return json_null();
}

/* lookup helper for _get_dm_topic_info */
static json_t *ta14_dm_lookup(ta14_ctx_t *ctx, const char *chat_id, double tid) {
    const json_t *topics = ta14_state_get(ctx, "_dm_topics");
    if (!topics || topics->type != JSON_OBJECT) return NULL;
    char *prefix = ta14_fmt("%s:", chat_id ? chat_id : "");
    size_t plen = strlen(prefix);
    for (size_t i = 0; i < topics->c.count; i++) {
        const char *k = topics->c.keys[i];
        const json_t *cv = topics->c.items[i];
        if (!k || strncmp(k, prefix, plen) != 0) continue;
        if (cv->type != JSON_NUMBER || cv->num_val != tid) continue;
        const char *colon = strchr(k, ':');
        const char *tname = colon ? colon + 1 : "";
        const json_t *cfg = ta14_state_get(ctx, "_dm_topics_config");
        if (cfg && cfg->type == JSON_ARRAY) {
            for (size_t j = 0; j < cfg->c.count; j++) {
                const json_t *entry = cfg->c.items[j];
                char *cid = ta14_py_str(ta14_getattr(entry, "chat_id"));
                bool match = cid && strcmp(cid, chat_id ? chat_id : "") == 0;
                free(cid);
                if (!match) continue;
                const json_t *ts = ta14_getattr(entry, "topics");
                if (ts && ts->type == JSON_ARRAY) {
                    for (size_t z = 0; z < ts->c.count; z++) {
                        const json_t *t = ts->c.items[z];
                        const char *nm = ta14_str(ta14_getattr(t, "name"));
                        if (nm && strcmp(nm, tname) == 0) { free(prefix); return json_copy((json_t *)t); }
                    }
                }
            }
        }
        json_t *r = json_object();
        json_set(r, "name", json_string(tname));
        free(prefix); return r;
    }
    free(prefix); return NULL;
}

/* PoP: ta14_get_dm_topic_info @ plugins/platforms/telegram/adapter.py:_get_dm_topic_info */
static json_t *ta14_get_dm_topic_info(ta14_ctx_t *ctx, const json_t *args) {
    const json_t *tidv = ta14_arg(args, "thread_id");
    if (!ta14_truthy(tidv)) return json_null();
    const char *cid = ta14_str(ta14_arg(args, "chat_id"));
    double tid = 0;
    if (!ta14_parse_int(tidv, &tid)) {
        char *s = ta14_py_str(tidv);
        char *msg = ta14_fmt("invalid literal for int() with base 10: '%s'", s ? s : "");
        free(s); json_t *e = ta14_err("ValueError", msg); free(msg); return e;
    }
    json_t *found = ta14_dm_lookup(ctx, cid, tid);
    if (found) return found;
    json_t *r = ta14_call(ctx, "reload_dm_topics_from_config", json_object());
    if (ta14_is_raise(r)) return r;
    json_free(r);
    return ta14_dm_lookup(ctx, cid, tid);
}

/* PoP: ta14_get_general_request_drain_lock @ plugins/platforms/telegram/adapter.py:_get_general_request_drain_lock */
static json_t *ta14_get_general_request_drain_lock(ta14_ctx_t *ctx, const json_t *args) {
    (void)args;
    const json_t *lock = ta14_state_get(ctx, "_general_request_drain_lock");
    if (!ta14_is_null(lock)) return json_copy((json_t *)lock);
    json_t *r = ta14_call(ctx, "asyncio_lock", json_object());
    if (ta14_is_raise(r)) return r;
    json_set(ctx->state, "_general_request_drain_lock", json_copy(r));
    return r;
}

/* PoP: ta14_go_fatal_network @ plugins/platforms/telegram/adapter.py:_go_fatal_network */
static json_t *ta14_go_fatal_network(ta14_ctx_t *ctx, const json_t *args) {
    const json_t *msg = ta14_arg(args, "message");
    const json_t *lm = ta14_arg(args, "log_message");
    json_t *arr = json_array();
    json_append(arr, ta14_copyv(lm));
    const json_t *la = ta14_arg(args, "log_args");
    if (la && la->type == JSON_ARRAY)
        for (size_t i = 0; i < la->c.count; i++) json_append(arr, json_copy(la->c.items[i]));
    json_t *e = ta14_call(ctx, "logger_error", ta14_logger_args(arr));
    if (ta14_is_raise(e)) return e;
    json_free(e);
    json_t *a = json_object();
    json_set(a, "kind", json_string("telegram_network_error"));
    json_set(a, "message", ta14_copyv(msg));
    json_set(a, "retryable", json_bool(true));
    e = ta14_call(ctx, "set_fatal_error", a);
    if (ta14_is_raise(e)) return e;
    json_free(e);
    e = ta14_call(ctx, "handoff_polling_fatal_error", json_object());
    if (ta14_is_raise(e)) return e;
    json_free(e); return json_null();
}

/* PoP: ta14_handle_command @ plugins/platforms/telegram/adapter.py:_handle_command */
static json_t *ta14_handle_command(ta14_ctx_t *ctx, const json_t *args) {
    json_t *ua = json_object();
    json_set(ua, "update", ta14_copyv(ta14_arg(args, "update")));
    json_t *msg = ta14_call(ctx, "effective_update_message", ua);
    if (ta14_is_raise(msg)) return msg;
    if (!ta14_truthy(msg) || !ta14_truthy(ta14_getattr(msg, "text"))) {
        json_free(msg); return json_null();
    }
    json_t *a = json_object();
    json_set(a, "msg", json_copy(msg));
    json_set(a, "is_command", json_bool(true));
    json_t *r = ta14_call(ctx, "should_process_message", a);
    if (ta14_is_raise(r)) { json_free(msg); return r; }
    bool proceed = ta14_truthy(r); json_free(r);
    if (!proceed) { json_free(msg); return json_null(); }
    a = json_object(); json_set(a, "msg", json_copy(msg));
    r = ta14_call(ctx, "is_user_authorized_from_message", a);
    if (ta14_is_raise(r)) { json_free(msg); return r; }
    bool auth = ta14_truthy(r); json_free(r);
    if (!auth) {
        a = json_object(); json_set(a, "msg", json_copy(msg));
        r = ta14_call(ctx, "log_blocked_user", a);
        json_free(msg);
        if (ta14_is_raise(r)) return r;
        json_free(r); return json_null();
    }
    a = json_object(); json_set(a, "msg", json_copy(msg));
    r = ta14_call(ctx, "ensure_forum_commands", a);
    if (ta14_is_raise(r)) { json_free(msg); return r; }
    json_free(r);
    a = json_object();
    json_set(a, "msg", json_copy(msg));
    json_set(a, "update", ta14_copyv(ta14_arg(args, "update")));
    json_set(a, "message_type", json_string("COMMAND"));
    json_t *event = ta14_call(ctx, "build_triggered_event", a);
    if (ta14_is_raise(event)) { json_free(msg); return event; }
    const char *evtext = ta14_str(ta14_getattr(event, "text"));
    double threshold = json_get_num(args, "split_threshold", _SPLIT_DEFAULT);
    json_t *r2 = NULL;
    if ((double)(evtext ? strlen(evtext) : 0) >= threshold) {
        json_t *ka = json_object();
        json_set(ka, "msg", json_copy(msg));
        r2 = ta14_call(ctx, "root_lobby_key", ka);
        if (ta14_is_raise(r2)) { json_free(event); json_free(msg); return r2; }
        json_set((json_t *)event, "_lobby_split_owner", json_copy(r2));
        json_free(r2);
        json_t *ea = json_object();
        json_set(ea, "event", json_copy(event));
        r2 = ta14_call(ctx, "enqueue_text_event", ea);
    } else {
        json_t *ea = json_object();
        json_set(ea, "event", json_copy(event));
        r2 = ta14_call(ctx, "handle_message", ea);
    }
    json_free(event); json_free(msg);
    if (ta14_is_raise(r2)) return r2;
    json_free(r2);
    return json_null();
}

/* helper: answer_callback(query, text=None, **kwargs) */
static json_t *ta14_answer(ta14_ctx_t *ctx, const json_t *q, const json_t *text, bool show_alert) {
    json_t *a = json_object();
    json_set(a, "query", ta14_copyv(q));
    json_set(a, "text", text ? json_copy((json_t *)text) : json_null());
    if (show_alert) {
        json_t *kw = json_object();
        json_set(kw, "show_alert", json_bool(true));
        json_set(a, "kwargs", kw);
    }
    return ta14_call(ctx, "answer_callback", a);
}

/* helper: t(key, **kwargs) */
static json_t *ta14_translate(ta14_ctx_t *ctx, const char *key, json_t *kwargs) {
    json_t *a = json_object();
    json_t *arr = json_array();
    json_append(arr, json_string(key));
    json_set(a, "args", arr);
    json_set(a, "kwargs", kwargs ? kwargs : json_object());
    return ta14_call(ctx, "translate", a);
}

/* PoP: ta14_handle_choice_picker_callback @ plugins/platforms/telegram/adapter.py:_handle_choice_picker_callback */
static json_t *ta14_handle_choice_picker_callback(ta14_ctx_t *ctx, const json_t *args) {
    const json_t *q = ta14_arg(args, "query");
    const char *data = ta14_str(ta14_arg(args, "data"));
    const char *chat = ta14_str(ta14_arg(args, "chat_id"));
    const json_t *cps = ta14_state_get(ctx, "_choice_picker_state");
    const json_t *state = (cps && cps->type == JSON_OBJECT && chat) ? json_obj_get((json_t *)cps, chat) : NULL;
    json_t *r = NULL;
    if (!ta14_truthy(state)) {
        r = ta14_toast(ctx, "platform.telegram.picker.expired_rerun");
        if (ta14_is_raise(r)) return r;
        json_t *a = json_object();
        json_set(a, "self", json_string("<self>")); json_set(a, "query", ta14_copyv(q));
        json_set(a, "text", json_copy(r)); json_free(r);
        r = ta14_call(ctx, "answer_or_note", a);
        if (ta14_is_raise(r)) return r;
        json_free(r); return json_null();
    }
    char *s1 = NULL, *s2 = NULL, *s3 = NULL;
    ta14_split3(data, &s1, &s2, &s3);
    free(s1); free(s2); free(s3);
    const char *rest = (data && strlen(data) > 3) ? data + 3 : "";
    json_t *num = json_string(rest);
    double idx = 0; bool ok = ta14_parse_int(num, &idx); json_free(num);
    const json_t *choices = ta14_getattr(state, "choices");
    json_t *choice = NULL;
    if (ok && choices && choices->type == JSON_ARRAY && idx >= 0 && idx < (double)choices->c.count)
        choice = choices->c.items[(size_t)idx];
    if (!choice) {
        r = ta14_toast(ctx, "platform.telegram.picker.invalid_selection");
        if (ta14_is_raise(r)) return r;
        r = ta14_answer(ctx, q, r, false);
        if (ta14_is_raise(r)) return r;
        json_free(r); return json_null();
    }
    const json_t *cbk = ta14_getattr(state, "on_choice_selected");
    if (!ta14_truthy(cbk)) {
        r = ta14_toast(ctx, "platform.telegram.picker.expired");
        if (ta14_is_raise(r)) return r;
        json_t *a = json_object();
        json_set(a, "self", json_string("<self>")); json_set(a, "query", ta14_copyv(q));
        json_set(a, "text", json_copy(r)); json_free(r);
        r = ta14_call(ctx, "answer_or_note", a);
        if (ta14_is_raise(r)) return r;
        json_free(r); return json_null();
    }
    const json_t *cv = ta14_getattr(choice, "value");
    char *val = ta14_truthy(cv) ? ta14_py_str(cv) : ta14_strdup("");
    json_t *ca = json_object();
    json_set(ca, "chat_id", ta14_copyv(ta14_arg(args, "chat_id")));
    json_set(ca, "value", json_string(val)); free(val);
    json_t *res = ta14_call(ctx, "choice_callback", ca);
    json_t *result_text = NULL;
    if (ta14_is_raise(res)) {
        const char *k = ta14_raise_kind(res);
        if (!ta14_is_exception(k)) return res;
        json_t *arr = json_array();
        json_append(arr, json_string("Choice picker selection failed: %s"));
        json_append(arr, json_copy(res));
        json_t *le = ta14_call(ctx, "logger_error", ta14_logger_args(arr));
        if (ta14_is_raise(le)) { json_free(res); return le; }
        json_free(le);
        json_t *kw = json_object();
        json_set(kw, "error", json_string(ta14_raise_msg(res)));
        json_free(res);
        result_text = ta14_translate(ctx, "platform.telegram.picker.apply_error", kw);
        if (ta14_is_raise(result_text)) return result_text;
    } else result_text = res;
    json_t *a = json_object();
    json_set(a, "query", ta14_copyv(q)); json_set(a, "text", json_copy(result_text));
    json_free(result_text);
    r = ta14_call(ctx, "edit_result_text", a);
    if (ta14_is_raise(r)) return r;
    json_free(r);
    r = ta14_answer(ctx, q, NULL, false);
    if (ta14_is_raise(r)) return r;
    json_free(r);
    if (cps && cps->type == JSON_OBJECT && chat) json_obj_del((json_t *)cps, chat);
    return json_null();
}

/* helper: prefix match against a tuple of strings */
static bool ta14_startswith_any(const char *data, const char *const *pfx, size_t n) {
    if (!data) return false;
    for (size_t i = 0; i < n; i++) if (strncmp(data, pfx[i], strlen(pfx[i])) == 0) return true;
    return false;
}

/* PoP: ta14_handle_callback_query @ plugins/platforms/telegram/adapter.py:_handle_callback_query */
static json_t *ta14_handle_callback_query(ta14_ctx_t *ctx, const json_t *args) {
    const json_t *up = ta14_arg(args, "update");
    const json_t *q = ta14_getattr(up, "callback_query");
    if (!ta14_truthy(q)) return json_null();
    json_t *r = ta14_call(ctx, "accept_update", json_object());
    if (ta14_is_raise(r)) return r;
    json_free(r);
    json_t *qa = json_object();
    json_set(qa, "query", ta14_copyv(q));
    json_t *cb = ta14_call(ctx, "callback_ctx", qa);
    if (ta14_is_raise(cb)) return cb;
    const char *data = ta14_str(ta14_getattr(q, "data"));
    if (!data) data = "";
    char databuf[8192];
    snprintf(databuf, sizeof databuf, "%s", data);
    data = databuf;
    const json_t *msg = ta14_getattr(q, "message");
    const char *chat = NULL;
    char *chatbuf = NULL;
    if (ta14_truthy(msg)) { chatbuf = ta14_py_str(ta14_getattr(msg, "chat_id")); chat = chatbuf; }
    const json_t *fu = ta14_getattr(q, "from_user");
    const json_t *uid = ta14_getattr(fu, "id");

    static const char *const MP[] = {"mp:", "mpg:", "mpv:", "mm:", "mc:", "mb", "mx", "mg:"};
    static const char *const CP[] = {"cp:"};
    bool is_mp = ta14_startswith_any(data, MP, sizeof(MP)/sizeof(MP[0]));
    bool is_cp = ta14_startswith_any(data, CP, sizeof(CP)/sizeof(CP[0]));
    if (is_mp || is_cp) {
        const char *statekey = is_mp ? "_model_picker_state" : "_choice_picker_state";
        const char *which = is_mp ? "_handle_model_picker_callback" : "_handle_choice_picker_callback";
        const json_t *smap = ta14_state_get(ctx, statekey);
        const json_t *st = (smap && smap->type == JSON_OBJECT && chat) ? json_obj_get((json_t *)smap, chat) : NULL;
        if (!ta14_truthy(st)) st = NULL;
        if (chat) {
            json_t *un = ta14_call(ctx, "unauthorized", json_object());
            if (ta14_is_raise(un)) { free(chatbuf); json_free(cb); return un; }
            json_t *aa = json_object();
            json_set(aa, "query", ta14_copyv(q)); json_set(aa, "cb", json_copy(cb));
            json_set(aa, "unauth", json_copy(un)); json_free(un);
            json_t *auth = ta14_call(ctx, "callback_authorized", aa);
            if (ta14_is_raise(auth)) { free(chatbuf); json_free(cb); return auth; }
            bool ok = ta14_truthy(auth); json_free(auth);
            if (ok) {
                const json_t *tapped = ta14_getattr(msg, "message_id");
                const json_t *mid = st ? ta14_getattr(st, "msg_id") : NULL;
                if (st && !ta14_is_null(mid) && !ta14_is_null(tapped)) {
                    char *ms = ta14_py_str(mid), *ts = ta14_py_str(tapped);
                    if (strcmp(ms, ts) != 0) {
                        free(ms); free(ts);
                        r = ta14_toast(ctx, "platform.telegram.picker.expired_rerun");
                        if (ta14_is_raise(r)) { free(chatbuf); json_free(cb); return r; }
                        json_t *pa = json_object();
                        json_set(pa, "self", json_string("<self>"));
                        json_set(pa, "query", ta14_copyv(q)); json_set(pa, "text", json_copy(r));
                        json_free(r);
                        r = ta14_call(ctx, "answer_or_note", pa);
                        if (ta14_is_raise(r)) { free(chatbuf); json_free(cb); return r; }
                        json_free(r); free(chatbuf); json_free(cb); return json_null();
                    }
                    free(ms); free(ts);
                }
                const json_t *pr_cb = st ? ta14_getattr(st, "on_model_selected") : NULL;
                if (!ta14_truthy(pr_cb) && st) pr_cb = ta14_getattr(st, "on_choice_selected");
                json_t *pa = json_object();
                json_set(pa, "callback", ta14_copyv(pr_cb));
                json_set(pa, "user_id", ta14_copyv(uid));
                json_set(pa, "extra", json_object());
                json_t *ref = ta14_call(ctx, "picker_refusal", pa);
                if (ta14_is_raise(ref)) { free(chatbuf); json_free(cb); return ref; }
                json_t *ra = json_object();
                json_set(ra, "query", ta14_copyv(q)); json_set(ra, "refusal", json_copy(ref));
                json_free(ref);
                json_t *rt = ta14_call(ctx, "refuse_tap", ra);
                if (ta14_is_raise(rt)) { free(chatbuf); json_free(cb); return rt; }
                bool refused = ta14_truthy(rt); json_free(rt);
                if (!refused) {
                    json_t *da = json_object();
                    json_set(da, "which", json_string(which));
                    json_set(da, "query", ta14_copyv(q));
                    json_set(da, "data", json_string(data));
                    json_set(da, "third", json_string(chat));
                    r = ta14_call(ctx, "dispatch_handler", da);
                    if (ta14_is_raise(r)) { free(chatbuf); json_free(cb); return r; }
                    json_free(r);
                }
            }
        }
        free(chatbuf); json_free(cb); return json_null();
    }
    const char *verbs[] = {"gt:", "ea:", "sc:", "cl:", "update_prompt:"};
    const char *hands[] = {"_handle_gmail_triage_callback", "_handle_exec_approval_callback",
                           "_handle_slash_confirm_callback", "_handle_clarify_callback",
                           "_handle_update_prompt_callback"};
    for (size_t i = 0; i < 5; i++) {
        if (strncmp(data, verbs[i], strlen(verbs[i])) == 0) {
            json_t *da = json_object();
            json_set(da, "which", json_string(hands[i]));
            json_set(da, "query", ta14_copyv(q));
            json_set(da, "data", json_string(data));
            json_set(da, "third", json_copy(cb));
            r = ta14_call(ctx, "dispatch_handler", da);
            json_free(cb); free(chatbuf);
            if (ta14_is_raise(r)) return r;
            json_free(r); return json_null();
        }
    }
    json_t *arr = json_array();
    json_append(arr, json_string("[%s] unhandled callback query (data=%r)"));
    json_append(arr, json_string("bot"));
    json_append(arr, json_string(strlen(data) > 64 ? databuf : data));
    json_t *dbg = ta14_call(ctx, "logger_debug", ta14_logger_args(arr));
    if (ta14_is_raise(dbg)) { json_free(cb); free(chatbuf); return dbg; }
    json_free(dbg);
    r = ta14_answer(ctx, q, NULL, false);
    json_free(cb); free(chatbuf);
    if (ta14_is_raise(r)) return r;
    json_free(r); return json_null();
}

/* helper: truncate to _TOAST_LIMIT bytes */
static json_t *ta14_toast_slice(const json_t *v) {
    const char *s = ta14_str(v);
    if (!s) return ta14_copyv(v);
    if (strlen(s) <= _TOAST_LIMIT) return json_string(s);
    char *t = (char *)malloc(_TOAST_LIMIT + 1);
    memcpy(t, s, _TOAST_LIMIT); t[_TOAST_LIMIT] = '\0';
    json_t *r = json_string(t); free(t); return r;
}

/* PoP: ta14_handle_clarify_callback @ plugins/platforms/telegram/adapter.py:_handle_clarify_callback */
static json_t *ta14_handle_clarify_callback(ta14_ctx_t *ctx, const json_t *args) {
    const json_t *q = ta14_arg(args, "query");
    const json_t *cb = ta14_arg(args, "cb");
    const char *data = ta14_str(ta14_arg(args, "data"));
    char *p1 = NULL, *p2 = NULL, *p3 = NULL;
    int n = ta14_split3(data, &p1, &p2, &p3);
    if (n != 3) { free(p1); free(p2); free(p3); return json_null(); }
    const char *cid = p2, *token = p3;
    json_t *r = NULL;
    json_t *un = ta14_call(ctx, "unauthorized", json_object());
    if (ta14_is_raise(un)) { free(p1); free(p2); free(p3); return un; }
    json_t *resolved = ta14_toast(ctx, "platform.telegram.slash_confirm.already_resolved");
    if (ta14_is_raise(resolved)) { free(p1); free(p2); free(p3); json_free(un); return resolved; }
    json_t *cs = ta14_state_get(ctx, "_clarify_state");
    json_t *ca = json_object();
    json_set(ca, "query", ta14_copyv(q)); json_set(ca, "cb", ta14_copyv(cb));
    json_set(ca, "state", cs ? json_copy(cs) : json_object());
    json_set(ca, "clarify_id", json_string(cid));
    json_set(ca, "unauth", json_copy(un)); json_free(un);
    json_set(ca, "resolved", json_copy(resolved)); json_free(resolved);
    json_set(ca, "pop", json_bool(false));
    json_t *prompt = ta14_call(ctx, "claim_callback_state", ca);
    if (ta14_is_raise(prompt)) { free(p1); free(p2); free(p3); return prompt; }
    if (!ta14_truthy(prompt)) { free(p1); free(p2); free(p3); json_free(prompt); return json_null(); }
    json_t *iv = ta14_toast(ctx, "platform.telegram.prompt.invalid_choice");
    if (ta14_is_raise(iv)) { free(p1); free(p2); free(p3); json_free(prompt); return iv; }
    json_t *ta = json_object();
    json_set(ta, "query", ta14_copyv(q)); json_set(ta, "clarify_id", json_string(cid));
    json_set(ta, "prompt", json_copy(prompt)); json_set(ta, "toast", json_copy(iv)); json_free(iv);
    json_t *tm = ta14_call(ctx, "tap_matches_prompt", ta);
    if (ta14_is_raise(tm)) { free(p1); free(p2); free(p3); json_free(prompt); return tm; }
    bool matched = ta14_truthy(tm); json_free(tm);
    if (!matched) { free(p1); free(p2); free(p3); json_free(prompt); return json_null(); }
    const json_t *fu = ta14_getattr(q, "from_user");
    const json_t *uid = ta14_getattr(fu, "id");
    json_t *ra = json_object();
    json_set(ra, "clarify_id", json_string(cid)); json_set(ra, "user_id", ta14_copyv(uid));
    json_set(ra, "extra", json_object());
    json_t *ref = ta14_call(ctx, "clarify_refusal", ra);
    if (ta14_is_raise(ref)) { free(p1); free(p2); free(p3); json_free(prompt); return ref; }
    if (ta14_truthy(ref)) {
        json_t *rt = ta14_call(ctx, "refusal_text", (json_t *)({ json_t *x = json_object(); json_set(x, "requester", json_copy(ref)); x; }));
        json_free(ref);
        if (ta14_is_raise(rt)) { free(p1); free(p2); free(p3); json_free(prompt); return rt; }
        json_t *sl = ta14_toast_slice(rt); json_free(rt);
        r = ta14_answer(ctx, q, sl, true); json_free(sl);
        free(p1); free(p2); free(p3); json_free(prompt);
        if (ta14_is_raise(r)) return r;
        return r;
    }
    json_free(ref);
    const json_t *fn = ta14_getattr(fu, "first_name");
    json_t *display = NULL;
    if (ta14_truthy(fn)) display = json_copy((json_t *)fn);
    else {
        display = ta14_translate(ctx, "platform.telegram.user_fallback", NULL);
        if (ta14_is_raise(display)) { free(p1); free(p2); free(p3); json_free(prompt); return display; }
    }
    json_t *ma = json_object();
    json_set(ma, "self", json_string("<self>")); json_set(ma, "query", ta14_copyv(q));
    json_set(ma, "clarify_id", json_string(cid)); json_set(ma, "choice_token", json_string(token));
    json_set(ma, "prompt", json_copy(prompt)); json_set(ma, "user_display", json_copy(display));
    json_t *mt = ta14_call(ctx, "handle_multi_token", ma);
    if (ta14_is_raise(mt)) { free(p1); free(p2); free(p3); json_free(prompt); json_free(display); return mt; }
    bool multi = ta14_truthy(mt); json_free(mt);
    if (multi) { free(p1); free(p2); free(p3); json_free(prompt); json_free(display); return json_null(); }
    if (strcmp(token, "other") == 0) {
        const json_t *cm = ta14_state_get(ctx, "_clarify_multi");
        if (cm && cm->type == JSON_OBJECT) json_obj_del((json_t *)cm, cid);
        json_t *mark = ta14_call(ctx, "mark_awaiting_text", (json_t *)({ json_t *x = json_object(); json_set(x, "clarify_id", json_string(cid)); x; }));
        bool flipped = false;
        if (ta14_is_raise(mark)) {
            if (ta14_is_exception(ta14_raise_kind(mark))) {
                json_t *arr = json_array();
                json_append(arr, json_string("[%s] mark_awaiting_text failed: %s"));
                json_append(arr, json_string("bot"));
                json_append(arr, json_copy(mark));
                json_t *lw = ta14_call(ctx, "logger_warning", ta14_logger_args(arr));
                if (ta14_is_raise(lw)) { free(p1); free(p2); free(p3); json_free(prompt); json_free(display); json_free(mark); return lw; }
                json_free(lw);
            } else { free(p1); free(p2); free(p3); json_free(prompt); json_free(display); return mark; }
        } else { flipped = ta14_truthy(mark); json_free(mark); }
        if (!flipped) {
            const json_t *cs2 = ta14_state_get(ctx, "_clarify_state");
            if (cs2 && cs2->type == JSON_OBJECT) json_obj_del((json_t *)cs2, cid);
            json_t *na = json_object();
            json_set(na, "query", ta14_copyv(q)); json_set(na, "user_display", json_copy(display));
            json_t *ne = ta14_call(ctx, "notify_clarify_expired", na);
            free(p1); free(p2); free(p3); json_free(prompt); json_free(display);
            if (ta14_is_raise(ne)) return ne;
            json_free(ne); return json_null();
        }
        json_t *tt = ta14_toast(ctx, "platform.telegram.prompt.type_answer");
        if (ta14_is_raise(tt)) { free(p1); free(p2); free(p3); json_free(prompt); json_free(display); return tt; }
        r = ta14_answer(ctx, q, tt, false); json_free(tt);
        if (ta14_is_raise(r)) { free(p1); free(p2); free(p3); json_free(prompt); json_free(display); return r; }
        json_free(r);
        json_t *kw = json_object(); json_set(kw, "user", json_copy(display));
        json_t *tr = ta14_translate(ctx, "platform.telegram.prompt.awaiting_typed", kw);
        if (ta14_is_raise(tr)) { free(p1); free(p2); free(p3); json_free(prompt); json_free(display); return tr; }
        json_t *ea = json_object(); json_set(ea, "text", json_copy(tr)); json_free(tr);
        json_t *esc = ta14_call(ctx, "html_escape", ea);
        if (ta14_is_raise(esc)) { free(p1); free(p2); free(p3); json_free(prompt); json_free(display); return esc; }
        const char *mtxt = ta14_str(ta14_getattr(ta14_getattr(q, "message"), "text"));
        char *formatted = ta14_fmt("❓ %s\n\n<i>%s</i>", mtxt ? mtxt : "", ta14_str(esc) ? ta14_str(esc) : "");
        json_free(esc);
        json_t *ha = json_object();
        json_set(ha, "query", ta14_copyv(q)); json_set(ha, "text", json_string(formatted)); free(formatted);
        r = ta14_call(ctx, "edit_html_quiet", ha);
        free(p1); free(p2); free(p3); json_free(prompt); json_free(display);
        if (ta14_is_raise(r)) return r;
        json_free(r); return json_null();
    }
    json_t *num = json_string(token);
    double idx = 0;
    bool ok = ta14_parse_int(num, &idx); json_free(num);
    if (!ok) {
        json_t *tt = ta14_toast(ctx, "platform.telegram.prompt.invalid_choice");
        if (ta14_is_raise(tt)) { free(p1); free(p2); free(p3); json_free(prompt); json_free(display); return tt; }
        r = ta14_answer(ctx, q, tt, false); json_free(tt);
        free(p1); free(p2); free(p3); json_free(prompt); json_free(display);
        if (ta14_is_raise(r)) return r;
        json_free(r); return json_null();
    }
    json_t *gp = ta14_call(ctx, "get_pending", (json_t *)({ json_t *x = json_object(); json_set(x, "clarify_id", json_string(cid)); x; }));
    json_t *resolved_text = NULL;
    if (ta14_is_raise(gp)) {
        if (!ta14_is_exception(ta14_raise_kind(gp))) { free(p1); free(p2); free(p3); json_free(prompt); json_free(display); return gp; }
        json_free(gp);
    } else {
        const json_t *ch = ta14_getattr(gp, "choices");
        if (ta14_truthy(ch) && ch->type == JSON_ARRAY && idx >= 0 && idx < (double)ch->c.count)
            resolved_text = json_copy(ch->c.items[(size_t)idx]);
        json_free(gp);
    }
    if (!resolved_text) resolved_text = json_string(ta14_fmt("choice %.0f", idx + 1));
    json_t *ra2 = json_object();
    json_set(ra2, "query", ta14_copyv(q)); json_set(ra2, "clarify_id", json_string(cid));
    json_set(ra2, "prompt", json_copy(prompt)); json_set(ra2, "resolved_text", json_copy(resolved_text));
    json_set(ra2, "user_display", json_copy(display)); json_free(resolved_text);
    r = ta14_call(ctx, "resolve_clarify_choice", ra2);
    free(p1); free(p2); free(p3); json_free(prompt); json_free(display);
    if (ta14_is_raise(r)) return r;
    json_free(r); return json_null();
}

/* PoP: ta14_handle_exec_approval_callback @ plugins/platforms/telegram/adapter.py:_handle_exec_approval_callback */
static json_t *ta14_handle_exec_approval_callback(ta14_ctx_t *ctx, const json_t *args) {
    const json_t *q = ta14_arg(args, "query");
    const json_t *cb = ta14_arg(args, "cb");
    const char *data = ta14_str(ta14_arg(args, "data"));
    char *p1 = NULL, *p2 = NULL, *p3 = NULL;
    int n = ta14_split3(data, &p1, &p2, &p3);
    if (n != 3) { free(p1); free(p2); free(p3); return json_null(); }
    const char *choice = p2;
    json_t *num = json_string(p3);
    double aid = 0; bool ok = ta14_parse_int(num, &aid); json_free(num);
    json_t *r = NULL;
    if (!ok) {
        r = ta14_toast(ctx, "platform.telegram.approval.toast_invalid_data");
        if (ta14_is_raise(r)) { free(p1); free(p2); free(p3); return r; }
        r = ta14_answer(ctx, q, r, false);
        free(p1); free(p2); free(p3);
        if (ta14_is_raise(r)) return r;
        return r;
    }
    json_t *resolved = ta14_toast(ctx, "platform.telegram.approval.toast_already_resolved");
    if (ta14_is_raise(resolved)) { free(p1); free(p2); free(p3); return resolved; }
    json_t *un = ta14_call(ctx, "unauthorized", json_object());
    if (ta14_is_raise(un)) { free(p1); free(p2); free(p3); json_free(resolved); return un; }
    const json_t *aps = ta14_state_get(ctx, "_approval_state");
    char aidbuf[64]; snprintf(aidbuf, sizeof aidbuf, "%.0f", aid);
    json_t *ca = json_object();
    json_set(ca, "query", ta14_copyv(q));
    json_set(ca, "cb", ta14_copyv(cb));
    json_set(ca, "state", aps ? json_copy((json_t *)aps) : json_object());
    json_set(ca, "clarify_id", json_number(aid));
    json_set(ca, "unauth", json_copy(un)); json_free(un);
    json_set(ca, "resolved", json_copy(resolved)); json_free(resolved);
    json_set(ca, "pop", json_bool(false));
    json_t *sk = ta14_call(ctx, "claim_callback_state", ca);
    if (ta14_is_raise(sk)) { free(p1); free(p2); free(p3); return sk; }
    if (!ta14_truthy(sk)) { free(p1); free(p2); free(p3); json_free(sk); return json_null(); }
    const json_t *fu = ta14_getattr(q, "from_user");
    const json_t *uid = ta14_getattr(fu, "id");
    const json_t *rids = ta14_state_get(ctx, "_approval_request_ids");
    const json_t *rid = NULL;
    if (rids && rids->type == JSON_OBJECT) rid = json_obj_get((json_t *)rids, aidbuf);
    json_t *ra = json_object();
    json_set(ra, "session_key", json_copy(sk));
    json_set(ra, "user_id", ta14_copyv(uid));
    json_set(ra, "extra", json_object());
    json_set(ra, "request_id", ta14_copyv(rid));
    json_set(ra, "choice", json_string(choice));
    json_t *ref = ta14_call(ctx, "approval_refusal", ra);
    if (ta14_is_raise(ref)) { free(p1); free(p2); free(p3); json_free(sk); return ref; }
    if (ta14_truthy(ref)) {
        json_t *x = json_object(); json_set(x, "requester", json_copy(ref)); json_free(ref);
        json_t *rt = ta14_call(ctx, "refusal_text", x);
        if (ta14_is_raise(rt)) { free(p1); free(p2); free(p3); json_free(sk); return rt; }
        json_t *sl = ta14_toast_slice(rt); json_free(rt);
        r = ta14_answer(ctx, q, sl, true); json_free(sl);
        free(p1); free(p2); free(p3); json_free(sk);
        if (ta14_is_raise(r)) return r;
        return r;
    }
    json_free(ref);
    json_t *popped = NULL;
    if (aps && aps->type == JSON_OBJECT) {
        const json_t *pv = json_obj_get((json_t *)aps, aidbuf);
        if (pv) { popped = json_copy((json_t *)pv); json_obj_del((json_t *)aps, aidbuf); }
    }
    if (!ta14_truthy(popped)) {
        json_free(popped); json_free(sk);
        r = ta14_answer(ctx, q, resolved, false);
        free(p1); free(p2); free(p3);
        if (ta14_is_raise(r)) return r;
        return r;
    }
    json_free(popped);
    if (rids && rids->type == JSON_OBJECT) json_obj_del((json_t *)rids, aidbuf);
    const json_t *fn = ta14_getattr(fu, "first_name");
    json_t *display = NULL;
    if (ta14_truthy(fn)) display = json_copy((json_t *)fn);
    else {
        display = ta14_translate(ctx, "platform.telegram.user_fallback", NULL);
        if (ta14_is_raise(display)) { free(p1); free(p2); free(p3); json_free(sk); return display; }
    }
    json_t *pa = json_object();
    json_set(pa, "session_key", json_copy(sk));
    json_set(pa, "choice", json_string(choice));
    json_set(pa, "request_id", ta14_copyv(rid));
    json_set(pa, "actor_id", ta14_copyv(uid));
    json_set(pa, "extra", json_object());
    json_t *cnt = ta14_call(ctx, "resolve_approval", pa);
    double count = 0;
    bool resolve_ok = true;
    if (ta14_is_raise(cnt)) {
        resolve_ok = false;
        if (!ta14_is_exception(ta14_raise_kind(cnt))) { free(p1); free(p2); free(p3); json_free(sk); json_free(display); return cnt; }
        json_t *arr = json_array();
        json_append(arr, json_string("Failed to resolve gateway approval from Telegram button: %s"));
        json_append(arr, json_copy(cnt)); json_free(cnt);
        json_t *le = ta14_call(ctx, "logger_error", ta14_logger_args(arr));
        if (ta14_is_raise(le)) { free(p1); free(p2); free(p3); json_free(sk); json_free(display); return le; }
        json_free(le);
    } else { count = ta14_num(cnt); json_free(cnt); }
    if (resolve_ok) {
        json_t *arr = json_array();
        json_append(arr, json_string("Telegram button resolved %d approval(s) for session %s (choice=%s, user=%s)"));
        json_append(arr, json_number(count));
        json_append(arr, json_copy(sk));
        json_append(arr, json_string(choice));
        json_append(arr, ta14_copyv(uid));
        json_t *li = ta14_call(ctx, "logger_info", ta14_logger_args(arr));
        if (ta14_is_raise(li)) { free(p1); free(p2); free(p3); json_free(sk); json_free(display); return li; }
        json_free(li);
    }
    json_t *label = NULL, *et = NULL;
    if (count != 0) {
        const char *lk = "resolved_generic";
        if (strcmp(choice, "once") == 0) lk = "resolved_once";
        else if (strcmp(choice, "session") == 0) lk = "resolved_session";
        else if (strcmp(choice, "always") == 0) lk = "resolved_always";
        else if (strcmp(choice, "deny") == 0) lk = "resolved_deny";
        char key[256]; snprintf(key, sizeof key, "platform.telegram.approval.%s", lk);
        label = ta14_translate(ctx, key, NULL);
        if (ta14_is_raise(label)) { free(p1); free(p2); free(p3); json_free(sk); json_free(display); return label; }
        json_t *kw = json_object();
        json_set(kw, "label", json_copy(label)); json_set(kw, "user", json_copy(display));
        et = ta14_translate(ctx, "platform.telegram.approval.resolved_by_user", kw);
    } else {
        label = ta14_translate(ctx, "platform.telegram.approval.expired", NULL);
        if (ta14_is_raise(label)) { free(p1); free(p2); free(p3); json_free(sk); json_free(display); return label; }
        json_t *kw = json_object(); json_set(kw, "label", json_copy(label));
        et = ta14_translate(ctx, "platform.telegram.approval.expired_detail", kw);
    }
    json_t *sl = ta14_toast_slice(label); json_free(label);
    r = ta14_answer(ctx, q, sl, false); json_free(sl);
    if (ta14_is_raise(r)) { free(p1); free(p2); free(p3); json_free(sk); json_free(display); json_free(et); return r; }
    json_free(r);
    json_t *ea = json_object();
    json_set(ea, "query", ta14_copyv(q)); json_set(ea, "text", json_copy(et)); json_free(et);
    r = ta14_call(ctx, "edit_keeping_prompt", ea);
    if (ta14_is_raise(r)) { free(p1); free(p2); free(p3); json_free(sk); json_free(display); return r; }
    json_free(r);
    const json_t *chatid = ta14_getattr(cb, "chat_id");
    if (count != 0 && !ta14_is_null(chatid)) {
        char *cs = ta14_py_str(chatid);
        json_t *ta = json_object(); json_set(ta, "chat_id", json_string(cs)); free(cs);
        json_t *tr = ta14_call(ctx, "resume_typing", ta);
        if (ta14_is_raise(tr)) { free(p1); free(p2); free(p3); json_free(sk); json_free(display); return tr; }
        json_free(tr);
    }
    free(p1); free(p2); free(p3); json_free(sk); json_free(display);
    return json_null();
}

/* gmail verb dispatch table */
typedef struct { const char *verb, *script, *skey; const char *const *extra; int nextra; int state_verb; } ta14_gv_t;
static const char *const GX_NONE[] = {};
static const char *const GX_EMAIL[] = {"email"};
static const char *const GX_DOMAIN[] = {"domain"};
static const ta14_gv_t TA14_GV[] = {
    {"send","send-draft.sh","done_send",GX_NONE,0,0},
    {"archive","archive.sh","done_archive",GX_NONE,0,0},
    {"draft","draft-blank.sh","done_draft",GX_NONE,0,0},
    {"spam","spam.sh","done_spam",GX_NONE,0,0},
    {"mute","mute-add.sh","done_mute",GX_EMAIL,1,1},
    {"mute-domain","mute-add.sh","done_mute_domain",GX_DOMAIN,1,1},
    {"trust","trusted-ops-add.sh","done_trust",GX_EMAIL,1,1},
    {"trust-domain","trusted-ops-add.sh","done_trust_domain",GX_DOMAIN,1,1},
    {"vip","vip-add.sh","done_vip",GX_EMAIL,1,1},
    {"vip-domain","vip-add.sh","done_vip_domain",GX_DOMAIN,1,1},
};

static char *ta14_strip(const char *s) {
    if (!s) return ta14_strdup("");
    while (*s && isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) n--;
    char *o = (char *)malloc(n + 1);
    memcpy(o, s, n); o[n] = '\0';
    return o;
}

/* PoP: ta14_handle_gmail_triage_callback @ plugins/platforms/telegram/adapter.py:_handle_gmail_triage_callback */
static json_t *ta14_handle_gmail_triage_callback(ta14_ctx_t *ctx, const json_t *args) {
    const json_t *q = ta14_arg(args, "query");
    const json_t *cb = ta14_arg(args, "cb");
    const char *data = ta14_str(ta14_arg(args, "data"));
    char *p1 = NULL, *p2 = NULL, *p3 = NULL;
    int n = ta14_split3(data, &p1, &p2, &p3);
    json_t *r = NULL;
    if (n != 3) {
        r = ta14_toast(ctx, "platform.telegram.gmail_triage.invalid_data");
        if (ta14_is_raise(r)) { free(p1); free(p2); free(p3); return r; }
        r = ta14_answer(ctx, q, r, false);
        free(p1); free(p2); free(p3);
        if (ta14_is_raise(r)) return r;
        json_free(r); return json_null();
    }
    const char *verb = p2, *arg = p3;
    json_t *un = ta14_call(ctx, "unauthorized", json_object());
    if (ta14_is_raise(un)) { free(p1); free(p2); free(p3); return un; }
    json_t *aa = json_object();
    json_set(aa, "query", ta14_copyv(q)); json_set(aa, "cb", ta14_copyv(cb));
    json_set(aa, "unauth", json_copy(un)); json_free(un);
    json_t *auth = ta14_call(ctx, "callback_authorized", aa);
    if (ta14_is_raise(auth)) { free(p1); free(p2); free(p3); return auth; }
    bool ok = ta14_truthy(auth); json_free(auth);
    if (!ok) { free(p1); free(p2); free(p3); return json_null(); }
    const ta14_gv_t *ent = NULL;
    for (size_t i = 0; i < sizeof(TA14_GV)/sizeof(TA14_GV[0]); i++)
        if (strcmp(verb, TA14_GV[i].verb) == 0) { ent = &TA14_GV[i]; break; }
    if (!ent) {
        json_t *kw = json_object(); json_set(kw, "verb", json_string(verb));
        json_t *tt = ta14_toast_kw(ctx, "platform.telegram.gmail_triage.unknown_verb", json_copy(kw));
        json_free(kw);
        if (ta14_is_raise(tt)) { free(p1); free(p2); free(p3); return tt; }
        r = ta14_answer(ctx, q, tt, false); json_free(tt);
        free(p1); free(p2); free(p3);
        if (ta14_is_raise(r)) return r;
        json_free(r); return json_null();
    }
    json_t *home = ta14_call(ctx, "get_hermes_home", json_object());
    if (ta14_is_raise(home)) { free(p1); free(p2); free(p3); return home; }
    char *hstr = ta14_py_str(home); json_free(home);
    char *path = ta14_fmt("%s%sscripts/gmail-triage/%s",
                          hstr ? hstr : "", (hstr && hstr[strlen(hstr)-1] == '/') ? "" : "/", ent->script);
    free(hstr);
    json_t *pa = json_object(); json_set(pa, "path", json_string(path));
    json_t *exists = ta14_call(ctx, "script_exists", pa);
    if (ta14_is_raise(exists)) { free(path); free(p1); free(p2); free(p3); return exists; }
    bool has = ta14_truthy(exists); json_free(exists);
    if (!has) {
        json_t *kw = json_object(); json_set(kw, "script", json_string(ent->script));
        json_t *tt = ta14_toast_kw(ctx, "platform.telegram.gmail_triage.script_missing", json_copy(kw));
        json_free(kw);
        if (ta14_is_raise(tt)) { free(path); free(p1); free(p2); free(p3); return tt; }
        r = ta14_answer(ctx, q, tt, false); json_free(tt);
        json_t *arr = json_array();
        json_append(arr, json_string("[%s] gmail-triage script missing: %s"));
        json_append(arr, json_string("bot")); json_append(arr, json_string(path));
        json_t *le = ta14_call(ctx, "logger_error", ta14_logger_args(arr));
        if (ta14_is_raise(le)) { free(path); free(p1); free(p2); free(p3); json_free(r); return le; }
        json_free(le); free(path); free(p1); free(p2); free(p3);
        if (ta14_is_raise(r)) return r;
        json_free(r); return json_null();
    }
    json_t *argv = json_array();
    json_append(argv, json_string(path)); json_append(argv, json_string(arg));
    for (int i = 0; i < ent->nextra; i++) json_append(argv, json_string(ent->extra[i]));
    json_t *sa = json_object();
    json_set(sa, "argv", argv);
    json_set(sa, "stdin", json_string("DEVNULL")); json_set(sa, "stdout", json_string("PIPE"));
    json_set(sa, "stderr", json_string("PIPE")); json_set(sa, "env", json_string("<env>"));
    json_t *proc = ta14_call(ctx, "create_subprocess", sa);
    json_t *label = NULL; bool success = false;
    bool failed = false;
    if (ta14_is_raise(proc)) {
        if (!ta14_is_exception(ta14_raise_kind(proc))) { free(path); free(p1); free(p2); free(p3); return proc; }
        json_t *exc_copy = json_copy(proc);
        json_t *kw = json_object();
        json_set(kw, "verb", json_string(verb)); json_set(kw, "error", json_string(ta14_raise_msg(proc)));
        json_free(proc);
        label = ta14_translate(ctx, "platform.telegram.gmail_triage.error", kw);
        if (ta14_is_raise(label)) { free(path); free(p1); free(p2); free(p3); return label; }
        json_t *arr = json_array();
        json_append(arr, json_string("[%s] gmail-triage callback exception: verb=%s arg=%s err=%s"));
        json_append(arr, json_string("bot")); json_append(arr, json_string(verb));
        json_append(arr, json_string(arg)); json_append(arr, exc_copy);
        json_t *le = ta14_call(ctx, "logger_error", ta14_logger_args(arr));
        if (ta14_is_raise(le)) { free(path); free(p1); free(p2); free(p3); json_free(label); return le; }
        json_free(le);
        failed = true;
    } else {
        json_t *ca = json_object();
        json_set(ca, "proc", (proc && proc->type == JSON_OBJECT) ? json_copy(proc) : json_object());
        json_t *comm = ta14_call(ctx, "communicate", ca);
        json_t *wa = json_object();
        json_set(wa, "awaitable", json_string("<awaitable>"));
        json_set(wa, "timeout", json_number(60));
        json_t *wr = NULL;
        if (ta14_is_raise(comm)) wr = comm;
        else { json_free(comm); wr = ta14_call(ctx, "wait_for", wa); }
        if (ta14_is_raise(wr)) {
            const char *k = ta14_raise_kind(wr);
            if (strcmp(k ? k : "", "TimeoutError") == 0) {
                json_t *kw = json_object(); json_set(kw, "verb", json_string(verb)); json_free(wr);
                label = ta14_translate(ctx, "platform.telegram.gmail_triage.timed_out", kw);
                if (ta14_is_raise(label)) { free(path); free(p1); free(p2); free(p3); json_free(proc); return label; }
                json_t *arr = json_array();
                json_append(arr, json_string("[%s] gmail-triage callback timed out: verb=%s arg=%s"));
                json_append(arr, json_string("bot")); json_append(arr, json_string(verb));
                json_append(arr, json_string(arg));
                json_t *le = ta14_call(ctx, "logger_error", ta14_logger_args(arr));
                if (ta14_is_raise(le)) { free(path); free(p1); free(p2); free(p3); json_free(proc); json_free(label); return le; }
                json_free(le);
            } else if (ta14_is_exception(k)) {
                json_t *kw = json_object();
                json_set(kw, "verb", json_string(verb));
                json_set(kw, "error", json_string(ta14_raise_msg(wr)));
                json_free(wr);
                label = ta14_translate(ctx, "platform.telegram.gmail_triage.error", kw);
                if (ta14_is_raise(label)) { free(path); free(p1); free(p2); free(p3); json_free(proc); return label; }
                json_t *arr = json_array();
                json_append(arr, json_string("[%s] gmail-triage callback exception: verb=%s arg=%s err=%s"));
                json_append(arr, json_string("bot")); json_append(arr, json_string(verb));
                json_append(arr, json_string(arg)); json_append(arr, json_string(ta14_raise_msg(label)));
                json_t *le = ta14_call(ctx, "logger_error", ta14_logger_args(arr));
                if (ta14_is_raise(le)) { free(path); free(p1); free(p2); free(p3); json_free(proc); json_free(label); return le; }
                json_free(le);
            } else { free(path); free(p1); free(p2); free(p3); json_free(proc); return wr; }
            failed = true;
        } else {
            double rc = ta14_num(ta14_getattr(proc, "returncode"));
            if (rc == 0) {
                char key[256]; snprintf(key, sizeof key, "platform.telegram.gmail_triage.%s", ent->skey);
                label = ta14_translate(ctx, key, NULL);
                if (ta14_is_raise(label)) { free(path); free(p1); free(p2); free(p3); json_free(proc); json_free(wr); return label; }
                success = true;
                json_t *arr = json_array();
                json_append(arr, json_string("[%s] gmail-triage callback ok: verb=%s arg=%s"));
                json_append(arr, json_string("bot")); json_append(arr, json_string(verb));
                json_append(arr, json_string(arg));
                json_t *li = ta14_call(ctx, "logger_info", ta14_logger_args(arr));
                if (ta14_is_raise(li)) { free(path); free(p1); free(p2); free(p3); json_free(proc); json_free(wr); json_free(label); return li; }
                json_free(li);
            } else {
                const char *err_text = (wr && wr->type == JSON_ARRAY && wr->c.count > 1) ? ta14_str(wr->c.items[1]) : "";
                char *trimmed = ta14_strip(err_text);
                const char *last = trimmed;
                const char *nl = strrchr(trimmed, '\n');
                if (nl) last = nl + 1;
                if (!*trimmed) {
                    char codebuf[32]; snprintf(codebuf, sizeof codebuf, "%.0f", rc);
                    json_t *kw = json_object(); json_set(kw, "code", json_string(codebuf));
                    json_t *tr = ta14_translate(ctx, "platform.telegram.gmail_triage.exit_code", kw);
                    last = ta14_str(tr) ? ta14_str(tr) : "";
                    free(trimmed); trimmed = ta14_strdup(last);
                }
                char detail[128];
                snprintf(detail, sizeof detail, "%.80s", last ? last : "");
                json_t *kw = json_object();
                json_set(kw, "verb", json_string(verb)); json_set(kw, "detail", json_string(detail));
                label = ta14_translate(ctx, "platform.telegram.gmail_triage.failed", kw);
                if (ta14_is_raise(label)) { free(trimmed); free(path); free(p1); free(p2); free(p3); json_free(proc); json_free(wr); return label; }
                json_t *arr = json_array();
                json_append(arr, json_string("[%s] gmail-triage callback failed: verb=%s arg=%s rc=%s stderr=%s"));
                json_append(arr, json_string("bot")); json_append(arr, json_string(verb));
                json_append(arr, json_string(arg));
                char rcstr[32]; snprintf(rcstr, sizeof rcstr, "%.0f", rc);
                json_append(arr, json_number(rc)); json_append(arr, json_string(trimmed));
                json_t *le = ta14_call(ctx, "logger_error", ta14_logger_args(arr));
                if (ta14_is_raise(le)) { free(trimmed); free(path); free(p1); free(p2); free(p3); json_free(proc); json_free(wr); json_free(label); return le; }
                json_free(le); free(trimmed);
            }
            json_free(wr);
        }
        json_free(proc);
    }
    (void)failed;
    json_t *sl = ta14_toast_slice(label);
    r = ta14_answer(ctx, q, sl, false); json_free(sl);
    if (ta14_is_raise(r)) { free(path); free(p1); free(p2); free(p3); json_free(label); return r; }
    json_free(r);
    if (!success) { free(path); free(p1); free(p2); free(p3); json_free(label); return json_null(); }
    const json_t *qm = ta14_getattr(q, "message");
    const char *orig = ta14_str(ta14_getattr(qm, "text"));
    const json_t *fu = ta14_getattr(q, "from_user");
    const json_t *fn = ta14_getattr(fu, "first_name");
    json_t *display = NULL;
    if (ta14_truthy(fn)) display = json_copy((json_t *)fn);
    else {
        display = ta14_translate(ctx, "platform.telegram.user_fallback", NULL);
        if (ta14_is_raise(display)) { free(path); free(p1); free(p2); free(p3); return display; }
    }
    json_t *kw = json_object();
    json_set(kw, "label", json_copy(label)); json_set(kw, "user", json_copy(display));
    json_free(display);
    json_free(label);
    json_t *by = ta14_translate(ctx, "platform.telegram.gmail_triage.appended_by", kw);
    if (ta14_is_raise(by)) { free(path); free(p1); free(p2); free(p3); return by; }
    char *appended = ta14_fmt("%s\n%s", orig ? orig : "", ta14_str(by) ? ta14_str(by) : "");
    json_free(by);
    json_t *ea = json_object();
    json_set(ea, "text", json_string(appended)); free(appended);
    if (!ent->state_verb) { json_t *kw2 = json_object(); json_set(kw2, "reply_markup", json_null()); json_set(ea, "kwargs", kw2); }
    r = ta14_call(ctx, "edit_message_text", ea);
    free(path); free(p1); free(p2); free(p3);
    if (ta14_is_raise(r)) {
        if (!ta14_is_exception(ta14_raise_kind(r))) return r;
        json_free(r); return json_null();
    }
    json_free(r); return json_null();
}

/* PoP: ta14_handle_inline_query @ plugins/platforms/telegram/adapter.py:_handle_inline_query */
static json_t *ta14_handle_inline_query(ta14_ctx_t *ctx, const json_t *args) {
    const json_t *up = ta14_arg(args, "update");
    const json_t *iq = ta14_getattr(up, "inline_query");
    if (ta14_is_null(iq)) return json_null();
    const json_t *fu = ta14_getattr(iq, "from_user");
    const json_t *idv = ta14_getattr(fu, "id");
    char *uid = ta14_truthy(idv) ? ta14_py_str(idv) : ta14_strdup("");
    char *uids = ta14_strip(uid); free(uid); uid = uids;
    json_t *auth = NULL;
    if (*uid) {
        json_t *aa = json_object();
        json_set(aa, "user_id", json_string(uid));
        json_set(aa, "chat_id", json_string(uid));
        json_set(aa, "chat_type", json_string("private"));
        json_set(aa, "user_name", ta14_copyv(ta14_getattr(fu, "username")));
        auth = ta14_call(ctx, "is_callback_user_authorized", aa);
    } else auth = json_bool(false);
    bool authorized = false;
    if (ta14_is_raise(auth)) {
        if (!ta14_is_exception(ta14_raise_kind(auth))) { free(uid); return auth; }
        json_t *arr = json_array();
        json_append(arr, json_string("[%s] inline picker auth check failed"));
        json_append(arr, json_string("bot"));
        json_t *dbg = ta14_call(ctx, "logger_debug", ta14_logger_args(arr));
        if (ta14_is_raise(dbg)) { free(uid); json_free(auth); return dbg; }
        json_free(dbg); json_free(auth);
    } else { authorized = ta14_truthy(auth); json_free(auth); }
    json_t *r = NULL;
    if (!authorized) {
        json_t *ac = ta14_call(ctx, "accept_update", json_object());
        json_t *cache = ta14_call(ctx, "inline_cache_time", json_object());
        json_t *aa = json_object();
        json_set(aa, "query", ta14_copyv(iq));
        json_set(aa, "results", json_array());
        json_set(aa, "cache_time", ta14_copyv(cache)); json_free(cache);
        json_set(aa, "is_personal", json_bool(true));
        r = ta14_call(ctx, "inline_answer", aa);
        if (ta14_is_raise(ac)) { free(uid); json_free(r); return ac; }
        json_free(ac);
        if (ta14_is_raise(r)) {
            if (!ta14_is_exception(ta14_raise_kind(r))) { free(uid); return r; }
            json_free(r);
            json_t *fp = ta14_call(ctx, "fail_update_preparation", json_object());
            if (ta14_is_raise(fp)) { free(uid); return fp; }
            json_free(fp);
            json_t *arr = json_array();
            json_append(arr, json_string("[%s] inline picker empty answer failed"));
            json_append(arr, json_string("bot"));
            json_t *dbg = ta14_call(ctx, "logger_debug", ta14_logger_args(arr));
            if (ta14_is_raise(dbg)) { free(uid); return dbg; }
            json_free(dbg);
        }
        free(uid); return json_null();
    }
    json_t *ta = json_object();
    const json_t *qv = ta14_getattr(iq, "query");
    const json_t *off = ta14_getattr(iq, "offset");
    json_set(ta, "query", ta14_copyv(qv)); json_set(ta, "offset", ta14_copyv(off));
    json_t *thread = ta14_call(ctx, "to_thread_inline", ta);
    if (ta14_is_raise(thread)) {
        if (!ta14_is_exception(ta14_raise_kind(thread))) { free(uid); return thread; }
        json_free(thread);
        json_t *fp = ta14_call(ctx, "fail_update_preparation", json_object());
        if (ta14_is_raise(fp)) { free(uid); return fp; }
        json_free(fp);
        json_t *arr = json_array();
        json_append(arr, json_string("[%s] inline picker answer failed"));
        json_append(arr, json_string("bot"));
        json_t *dbg = ta14_call(ctx, "logger_debug", ta14_logger_args(arr));
        if (ta14_is_raise(dbg)) { free(uid); return dbg; }
        json_free(dbg); free(uid); return json_null();
    }
    json_t *articles = json_array();
    json_t *results = (thread && thread->type == JSON_ARRAY && thread->c.count > 0) ? thread->c.items[0] : NULL;
    json_t *next_offset = (thread && thread->type == JSON_ARRAY && thread->c.count > 1) ? thread->c.items[1] : NULL;
    bool bad = false;
    if (results && results->type == JSON_ARRAY) {
        for (size_t i = 0; i < results->c.count; i++) {
            const json_t *row = results->c.items[i];
            json_t *ca = json_object();
            json_set(ca, "message_text", ta14_copyv(ta14_getattr(row, "message_text")));
            json_t *content = ta14_call(ctx, "make_content", ca);
            if (ta14_is_raise(content)) { bad = true; r = content; break; }
            json_t *ara = json_object();
            json_set(ara, "id", ta14_copyv(ta14_getattr(row, "id")));
            json_set(ara, "title", ta14_copyv(ta14_getattr(row, "title")));
            json_set(ara, "description", ta14_copyv(ta14_getattr(row, "description")));
            json_set(ara, "input_message_content", json_copy(content));
            json_free(content);
            json_t *art = ta14_call(ctx, "make_article", ara);
            if (ta14_is_raise(art)) { bad = true; r = art; break; }
            json_append(articles, json_copy(art)); json_free(art);
        }
    }
    if (!bad) {
        json_t *ac = ta14_call(ctx, "accept_update", json_object());
        json_t *cache = ta14_call(ctx, "inline_cache_time", json_object());
        json_t *aa = json_object();
        json_set(aa, "query", ta14_copyv(iq));
        json_set(aa, "results", json_copy(articles));
        json_set(aa, "cache_time", ta14_copyv(cache)); json_free(cache);
        json_set(aa, "is_personal", json_bool(true));
        json_set(aa, "next_offset", ta14_copyv(next_offset));
        r = ta14_call(ctx, "inline_answer", aa);
        if (ta14_is_raise(ac)) { json_free(articles); json_free(r); free(uid); return ac; }
        json_free(ac);
    }
    json_free(articles);
    if (ta14_is_raise(r)) {
        if (!ta14_is_exception(ta14_raise_kind(r))) { free(uid); return r; }
        json_free(r);
        json_t *fp = ta14_call(ctx, "fail_update_preparation", json_object());
        if (ta14_is_raise(fp)) { free(uid); return fp; }
        json_free(fp);
        json_t *arr = json_array();
        json_append(arr, json_string("[%s] inline picker answer failed"));
        json_append(arr, json_string("bot"));
        json_t *dbg = ta14_call(ctx, "logger_debug", ta14_logger_args(arr));
        if (ta14_is_raise(dbg)) { free(uid); return dbg; }
        json_free(dbg);
    } else json_free(r);
    free(uid); return json_null();
}


