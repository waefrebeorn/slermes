/*
 * discord_adapter_flow13.c - Discord adapter FLOW part 13 of
 * plugins/platforms/discord/adapter.py (hermes-agent): the remainder batch
 * left after discord_adapter_flow{,2,5,6,7,8,9,10,11,12}. Every target below
 * was confirmed UNCLAIMED; no flow 1-12 target is re-ported.
 *
 * Themes: platform-event normalization, websocket health, command-sync state,
 * audio probing, voice-input processing, attachment reads, SSRF-guarded image
 * downloads, exec-approval settlement and the voice RTP packet pipeline.
 * All live I/O and injected callables enter through ONE universal seam:
 * da13_call(ctx, name, args).
 *
 * Self-contained: harness #includes this .c directly; libjson only.
 * Every symbol is static and da13_-prefixed.
 *
 * Representation contract (shared with tests/oracle_discord_adapter_flow13.py):
 * - NULL json_t* == Python None / absent; JSON_NULL is a real null value.
 * - bytes/bytearray payloads travel as ASCII-safe hex strings.
 * - Seam scripts: a plain value repeats; a LIST is consumed per call (last
 *   repeats); {"__script__":true,"ret":v,"raise":{type,message}} forces a
 *   result or raise. Missing/exhausted script => fail-closed RuntimeError
 *   recorded in the trace before propagation (BaseException passthrough).
 * - Except sites swallow ONLY Exception-family kinds (DA13_EXC_HIER);
 *   BaseException-only kinds pass through every except-Exception site.
 * - Integral doubles render as ints in py_str (and canon folds them too);
 *   keep str()-position fixtures non-integral or integral-int.
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

#define DA13_MAX_REDIRECTS 10
#define DA13_REKEY_FAILURE_STREAK 25

typedef struct da13_ctx {
    json_t *state;        /* owned; carries the attribute-double fields */
    const json_t *io;     /* borrowed scripted seam results */
    json_t *cursors;      /* owned; per-name array cursors */
    json_t *trace;        /* owned; ordered seam events */
} da13_ctx_t;

/* ── basic helpers ── */
static bool da13_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *da13_raise_kind(const json_t *v) {
    return da13_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static bool da13_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static bool da13_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static const char *da13_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static json_t *da13_copyv(const json_t *v) { return v ? json_copy((json_t *)v) : json_null(); }
static json_t *da13_err(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}
static const json_t *da13_arg(const json_t *args, const char *key) {
    return args ? json_obj_get(args, key) : NULL;
}
static const json_t *da13_getattr(const json_t *obj, const char *key) {
    return (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, key) : NULL;
}
static const json_t *da13_state_get(const da13_ctx_t *ctx, const char *key) {
    return (ctx && ctx->state && ctx->state->type == JSON_OBJECT)
        ? json_obj_get(ctx->state, key) : NULL;
}
static void da13_state_set(da13_ctx_t *ctx, const char *key, json_t *v) {
    if (ctx && ctx->state && ctx->state->type == JSON_OBJECT) json_set(ctx->state, key, v);
}
static double da13_num(const json_t *v) {
    return (v && v->type == JSON_NUMBER) ? v->num_val : 0.0;
}
static bool da13_has(const json_t *obj, const char *key) {
    return obj && obj->type == JSON_OBJECT && json_obj_get(obj, key) != NULL;
}
static bool da13_json_eq(const json_t *a, const json_t *b) {
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
                if (!da13_json_eq(a->c.items[i], b->c.items[i])) return false;
            return true;
        case JSON_OBJECT:
            if (a->c.count != b->c.count) return false;
            for (size_t i = 0; i < a->c.count; i++) {
                if (!a->c.keys[i] || !b->c.keys[i] || strcmp(a->c.keys[i], b->c.keys[i]) != 0)
                    return false;
                if (!da13_json_eq(a->c.items[i], b->c.items[i])) return false;
            }
            return true;
        default: return false;
    }
}

/* ── string helpers ── */
static char *da13_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static char *da13_lower(const char *s) {
    char *o = da13_strdup(s);
    if (!o) return NULL;
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static char *da13_fmt(const char *fmt, ...) {
    char tmp[2400]; va_list ap;
    va_start(ap, fmt); int n = vsnprintf(tmp, sizeof tmp, fmt, ap); va_end(ap);
    if (n < 0) return da13_strdup("");
    char *o = malloc((size_t)n + 1);
    if (!o) return da13_strdup("");
    va_start(ap, fmt); vsnprintf(o, (size_t)n + 1, fmt, ap); va_end(ap);
    return o;
}
static char *da13_trim(const char *s) {
    if (!s) return da13_strdup("");
    const char *a = s;
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    char *o = malloc((size_t)(b - a) + 1);
    if (!o) return da13_strdup("");
    memcpy(o, a, (size_t)(b - a)); o[b - a] = '\0';
    return o;
}
/* Python str() of a JSON scalar; integral doubles render as ints. */
static char *da13_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return da13_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return da13_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 1e16) return da13_fmt("%.0f", d);
            return da13_fmt("%.17g", d);
        }
        case JSON_STRING: return da13_strdup(v->str_val ? v->str_val : "");
        default: { char *s = json_serialize((json_t *)v); return s ? s : da13_strdup(""); }
    }
}
/* py_str truncated to N codepoints (fixtures ASCII in slice positions). */
static json_t *da13_slice_str(const json_t *v, size_t n, bool only_string) {
    if (!v || v->type == JSON_NULL || (only_string && v->type != JSON_STRING))
        return json_null();
    char *s = da13_py_str(v);
    if (!s) return json_null();
    size_t len = strlen(s);
    if (len > n) s[n] = '\0';
    json_t *r = json_string(s);
    free(s);
    return r;
}

/* hex <-> bytes */
static unsigned char *da13_hex_bytes(const char *hex, size_t *out_n) {
    size_t n = hex ? strlen(hex) / 2 : 0;
    unsigned char *b = malloc(n ? n : 1);
    for (size_t i = 0; i < n; i++) {
        unsigned int v = 0;
        sscanf(hex + 2 * i, "%2x", &v);
        b[i] = (unsigned char)v;
    }
    *out_n = n;
    return b;
}
/* bytes.fromhex semantics for untrusted caller input. ASCII whitespace is
 * skipped before each byte; the two nibbles of a byte must be adjacent.
 * On invalid input *err receives the exact CPython message and NULL returns. */
static unsigned char *da13_hex_checked(const char *hex, size_t *out_n, const char **err) {
    *out_n = 0; *err = NULL;
    size_t L = hex ? strlen(hex) : 0;
    unsigned char *b = malloc(L / 2 + 1);
    if (!b) return NULL;
    size_t k = 0, i = 0;
    for (;;) {
        while (i < L && (hex[i] == ' ' || hex[i] == '\t' || hex[i] == '\n' ||
                         hex[i] == '\r' || hex[i] == '\x0b' || hex[i] == '\x0c')) i++;
        if (i >= L) break;
        unsigned char hi = (unsigned char)hex[i];
        bool hi_ok = (hi >= '0' && hi <= '9') || (hi >= 'a' && hi <= 'f') || (hi >= 'A' && hi <= 'F');
        if (!hi_ok) goto nonhex;
        i++;
        if (i >= L) goto odd;
        unsigned char lo = (unsigned char)hex[i];
        bool lo_ok = (lo >= '0' && lo <= '9') || (lo >= 'a' && lo <= 'f') || (lo >= 'A' && lo <= 'F');
        if (!lo_ok) goto nonhex;
        i++;
        int hv = (hi <= '9') ? hi - '0' : ((hi >= 'a') ? hi - 'a' + 10 : hi - 'A' + 10);
        int lv = (lo <= '9') ? lo - '0' : ((lo >= 'a') ? lo - 'a' + 10 : lo - 'A' + 10);
        b[k++] = (unsigned char)((hv << 4) | lv);
    }
    *out_n = k;
    return b;
nonhex: {
        char *msg = malloc(L + 96);
        if (msg) snprintf(msg, L + 96,
                          "non-hexadecimal number found in fromhex() arg at position %zu", i);
        *err = msg;
        free(b);
        return NULL;
    }
odd:
    *err = da13_strdup("fromhex() arg must contain an even number of hexadecimal digits");
    free(b);
    return NULL;
}
static char *da13_bytes_hex(const unsigned char *b, size_t n) {
    char *o = malloc(n * 2 + 1);
    for (size_t i = 0; i < n; i++) sprintf(o + 2 * i, "%02x", b[i]);
    o[2 * n] = '\0';
    return o;
}
static char *da13_hex_slice(const char *hex, size_t start_byte, size_t len_byte) {
    size_t total = hex ? strlen(hex) / 2 : 0;
    if (start_byte >= total) return da13_strdup("");
    size_t n = len_byte;
    if (start_byte + n > total) n = total - start_byte;
    return da13_strdup(hex + 2 * start_byte); /* caller truncates via nul below */
}

/* ── FULL builtin exception hierarchy (CPython 3.14) ── */
typedef struct { const char *name; const char *parent; } da13_exc_row_t;
static const da13_exc_row_t DA13_EXC_HIER[] = {
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
static const char *da13_builtin_parent(const char *name) {
    for (size_t i = 0; i < sizeof(DA13_EXC_HIER) / sizeof(DA13_EXC_HIER[0]); i++)
        if (strcmp(name, DA13_EXC_HIER[i].name) == 0) return DA13_EXC_HIER[i].parent;
    return NULL;
}
static bool da13_chain_has(const char *start, const char *target) {
    const char *cur = start;
    for (int depth = 0; cur && depth < 24; depth++) {
        if (strcmp(cur, target) == 0) return true;
        cur = da13_builtin_parent(cur);
    }
    return false;
}
static bool da13_is_exception(const char *kind) {
    if (!kind) return false;
    if (da13_builtin_parent(kind) == NULL) return true; /* custom = Exception subclass */
    return da13_chain_has(kind, "Exception");
}

/* logger helpers */
static json_t *da13_log_args(json_t *arr) {
    json_t *a = json_object();
    json_set(a, "args", arr ? arr : json_array());
    return a;
}
static json_t *da13_log_args_kw(json_t *arr, bool exc_info) {
    json_t *a = json_object();
    json_set(a, "args", arr ? arr : json_array());
    json_t *kw = json_object();
    if (exc_info) json_set(kw, "exc_info", json_bool(true));
    json_set(a, "kwargs", kw);
    return a;
}

/* ── the one universal seam ──
 * args is ADOPTED into the trace event; return is a fresh owned node. */
static json_t *da13_call(da13_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = da13_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = da13_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                ret = da13_err(json_get_str(raise, "type", "RuntimeError"),
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

/* PoP: da13_on_platform_message_edit @ plugins/platforms/discord/adapter.py:_on_platform_message_edit */
static json_t *da13_on_platform_message_edit(da13_ctx_t *ctx, const json_t *args) {
    const json_t *msg = da13_arg(args, "after");
    if (da13_is_null(msg)) msg = da13_arg(args, "before");
    json_t *extra = json_object();
    json_set(extra, "text", da13_slice_str(da13_getattr(msg, "content"), 8192, true));
    const json_t *ed = da13_getattr(msg, "edited_at");
    if (ed && ed->type == JSON_OBJECT && da13_has(ed, "__stamp__"))
        json_set(extra, "edited_at", da13_slice_str(json_obj_get(ed, "__stamp__"), 64, false));
    else
        json_set(extra, "edited_at", json_null());
    json_t *a = json_object();
    json_set(a, "message", da13_copyv(msg));
    json_set(a, "extra", extra);
    json_t *parts = da13_call(ctx, "message_event_parts", a);
    if (da13_is_raise(parts)) return parts;
    json_t *b = json_object();
    json_set(b, "event_type", json_string("message_edited"));
    json_set(b, "built", json_copy(parts));
    json_free(parts);
    json_t *r = da13_call(ctx, "emit_platform_event", b);
    if (da13_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: da13_on_platform_message_delete @ plugins/platforms/discord/adapter.py:_on_platform_message_delete */
static json_t *da13_on_platform_message_delete(da13_ctx_t *ctx, const json_t *args) {
    const json_t *msg = da13_arg(args, "message");
    const json_t *author = da13_getattr(msg, "author");
    const json_t *aid = da13_getattr(author, "id");
    json_t *extra = json_object();
    if (!da13_is_null(aid) && da13_truthy(aid))
        json_set(extra, "author_id", da13_slice_str(aid, 128, false));
    else
        json_set(extra, "author_id", json_null());
    json_t *a = json_object();
    json_set(a, "message", da13_copyv(msg));
    json_set(a, "extra", extra);
    json_t *parts = da13_call(ctx, "message_event_parts", a);
    if (da13_is_raise(parts)) return parts;
    json_t *b = json_object();
    json_set(b, "event_type", json_string("message_deleted"));
    json_set(b, "built", json_copy(parts));
    json_free(parts);
    json_t *r = da13_call(ctx, "emit_platform_event", b);
    if (da13_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: da13_on_platform_thread_create @ plugins/platforms/discord/adapter.py:_on_platform_thread_create */
static json_t *da13_on_platform_thread_create(da13_ctx_t *ctx, const json_t *args) {
    const json_t *th = da13_arg(args, "thread");
    json_t *extra = json_object();
    json_set(extra, "name", da13_slice_str(da13_getattr(th, "name"), 256, true));
    const json_t *oid = da13_getattr(th, "owner_id");
    json_set(extra, "owner_id", da13_is_null(oid) ? json_null() : da13_slice_str(oid, 128, false));
    json_t *a = json_object();
    json_set(a, "thread", da13_copyv(th));
    json_set(a, "extra", extra);
    json_t *parts = da13_call(ctx, "thread_event_parts", a);
    if (da13_is_raise(parts)) return parts;
    json_t *b = json_object();
    json_set(b, "event_type", json_string("thread_created"));
    json_set(b, "built", json_copy(parts));
    json_free(parts);
    json_t *r = da13_call(ctx, "emit_platform_event", b);
    if (da13_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: da13_on_platform_thread_update @ plugins/platforms/discord/adapter.py:_on_platform_thread_update */
static json_t *da13_on_platform_thread_update(da13_ctx_t *ctx, const json_t *args) {
    const json_t *before = da13_arg(args, "before");
    const json_t *after = da13_arg(args, "after");
    const json_t *oldv = da13_getattr(before, "name");
    const json_t *newv = da13_getattr(after, "name");
    json_t *built = NULL;
    if ((!da13_is_null(oldv) && !da13_is_null(newv) && da13_json_eq(oldv, newv)) ||
        !newv || newv->type != JSON_STRING) {
        built = json_null();
    } else {
        json_t *extra = json_object();
        json_set(extra, "old_name", da13_slice_str(oldv, 256, true));
        json_set(extra, "new_name", da13_slice_str(newv, 256, true));
        json_t *a = json_object();
        json_set(a, "thread", da13_copyv(after));
        json_set(a, "extra", extra);
        built = da13_call(ctx, "thread_event_parts", a);
        if (da13_is_raise(built)) return built;
    }
    json_t *b = json_object();
    json_set(b, "event_type", json_string("thread_renamed"));
    json_set(b, "built", json_copy(built));
    json_free(built);
    json_t *r = da13_call(ctx, "emit_platform_event", b);
    if (da13_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* helper: pack a (bool, string) tuple */
static json_t *da13_pair(bool b, const char *s) {
    json_t *a = json_array();
    json_append(a, json_bool(b));
    json_append(a, json_string(s ? s : ""));
    return a;
}

/* PoP: da13_read_websocket_health @ plugins/platforms/discord/adapter.py:_read_websocket_health */
static json_t *da13_read_websocket_health(da13_ctx_t *ctx, const json_t *args) {
    const json_t *client = da13_arg(args, "client");
    json_t *r = da13_call(ctx, "ws_is_ready", json_object());
    if (da13_is_raise(r)) {
        if (!da13_is_exception(da13_raise_kind(r))) return r;
        json_free(r); return da13_pair(false, "not_ready");
    }
    bool ready = da13_truthy(r); json_free(r);
    if (!ready) return da13_pair(false, "not_ready");
    r = da13_call(ctx, "ws_is_closed", json_object());
    if (da13_is_raise(r)) {
        if (!da13_is_exception(da13_raise_kind(r))) return r;
        json_free(r); return da13_pair(false, "client_closed");
    }
    bool closed = da13_truthy(r); json_free(r);
    if (closed) return da13_pair(false, "client_closed");
    const json_t *ws = da13_getattr(client, "ws");
    bool socket_open = false;
    if (!da13_is_null(ws) && ws && ws->type == JSON_OBJECT)
        socket_open = da13_truthy(json_obj_get(ws, "open"));
    if (!socket_open) return da13_pair(false, "socket_closed");
    const json_t *ka = da13_getattr(ws, "_keep_alive");
    const json_t *last_ack = da13_getattr(ka, "_last_ack");
    if (!last_ack || (last_ack->type != JSON_NUMBER && last_ack->type != JSON_BOOL))
        return da13_pair(false, "ack_unavailable");
    json_t *pc = da13_call(ctx, "perf_counter", json_object());
    if (da13_is_raise(pc)) return pc;
    double now = da13_num(pc); json_free(pc);
    double ack_age = now - (last_ack->type == JSON_BOOL ? (last_ack->bool_val ? 1.0 : 0.0) : last_ack->num_val);
    double max_age = da13_num(da13_state_get(ctx, "_heartbeat_ack_max_age_seconds"));
    if (!isfinite(ack_age) || ack_age > max_age) return da13_pair(false, "ack_stale");
    const json_t *lat = da13_getattr(client, "latency");
    if (!lat || (lat->type != JSON_NUMBER && lat->type != JSON_BOOL))
        return da13_pair(false, "latency_non_finite");
    double latency = (lat->type == JSON_BOOL) ? (lat->bool_val ? 1.0 : 0.0) : lat->num_val;
    if (!isfinite(latency)) return da13_pair(false, "latency_non_finite");
    double max_lat = da13_num(da13_state_get(ctx, "_max_latency_seconds"));
    if (latency > max_lat) return da13_pair(false, "latency_exceeded");
    double max_silence = da13_num(da13_state_get(ctx, "_event_max_silence_seconds"));
    if (max_silence > 0) {
        const json_t *last_ev = da13_state_get(ctx, "_last_dispatched_event_monotonic");
        if (!da13_is_null(last_ev)) {
            pc = da13_call(ctx, "perf_counter", json_object());
            if (da13_is_raise(pc)) return pc;
            double now2 = da13_num(pc); json_free(pc);
            double ev_silence = now2 - da13_num(last_ev);
            if (ev_silence > max_silence) return da13_pair(false, "event_silence");
        }
    }
    return da13_pair(true, "healthy");
}

/* PoP: da13_read_command_sync_state @ plugins/platforms/discord/adapter.py:_read_command_sync_state */
static json_t *da13_read_command_sync_state(da13_ctx_t *ctx, const json_t *args) {
    (void)args;
    json_t *path = da13_call(ctx, "command_sync_state_path", json_object());
    if (da13_is_raise(path)) {
        if (!da13_is_exception(da13_raise_kind(path))) return path;
        json_free(path); return json_object();
    }
    json_t *a = json_object(); json_set(a, "path", json_copy(path));
    json_t *ex = da13_call(ctx, "path_exists", a);
    if (da13_is_raise(ex)) {
        if (path) json_free(path);
        if (!da13_is_exception(da13_raise_kind(ex))) return ex;
        json_free(ex); return json_object();
    }
    bool exists = da13_truthy(ex); json_free(ex);
    if (!exists) { if (path) json_free(path); return json_object(); }
    a = json_object(); json_set(a, "path", json_copy(path));
    json_set(a, "encoding", json_string("utf-8-sig"));
    if (path) json_free(path);
    json_t *txt = da13_call(ctx, "path_read_text", a);
    if (da13_is_raise(txt)) {
        if (!da13_is_exception(da13_raise_kind(txt))) return txt;
        json_free(txt); return json_object();
    }
    a = json_object(); json_set(a, "text", json_copy(txt)); json_free(txt);
    json_t *data = da13_call(ctx, "json_loads", a);
    if (da13_is_raise(data)) {
        if (!da13_is_exception(da13_raise_kind(data))) return data;
        json_free(data); return json_object();
    }
    if (data && data->type == JSON_OBJECT) return data;
    json_free(data);
    return json_object();
}

/* float() subset: numbers, bools and numeric strings; returns false on bad token */
static bool da13_float(const json_t *v, double *out) {
    if (!v) return false;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return true; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return true; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        while (*s && isspace((unsigned char)*s)) s++;
        if (!*s) return false;
        char *end = NULL;
        double d = strtod(s, &end);
        if (!end) return false;
        while (*end && isspace((unsigned char)*end)) end++;
        if (*end) return false;
        *out = d; return true;
    }
    return false;
}

/* PoP: da13_record_command_sync_rate_limit @ plugins/platforms/discord/adapter.py:_record_command_sync_rate_limit */
static json_t *da13_record_command_sync_rate_limit(da13_ctx_t *ctx, const json_t *args) {
    double ra = 0.0;
    if (!da13_float(da13_arg(args, "retry_after"), &ra))
        return da13_err("ValueError", "could not convert string to float");
    if (ra < 1.0) ra = 1.0; /* max(1.0, nan) -> 1.0 (nan comparison false) */
    json_t *t = da13_call(ctx, "time_time", json_object());
    if (da13_is_raise(t)) return t;
    double now = da13_num(t); json_free(t);
    json_t *fields = json_object();
    json_set(fields, "retry_after_until", json_number(now + ra));
    json_set(fields, "retry_after", json_number(ra));
    json_t *a = json_object();
    json_set(a, "app_id", da13_copyv(da13_arg(args, "app_id")));
    json_set(a, "fingerprint", da13_copyv(da13_arg(args, "fingerprint")));
    json_set(a, "keep_existing", json_bool(true));
    json_set(a, "fields", fields);
    json_t *r = da13_call(ctx, "update_command_sync_entry", a);
    if (da13_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* PoP: da13_probe_audio_duration_seconds @ plugins/platforms/discord/adapter.py:_probe_audio_duration_seconds */
static json_t *da13_probe_audio_duration_seconds(da13_ctx_t *ctx, const json_t *args) {
    const char *path = da13_str(da13_arg(args, "audio_path"));
    bool ffprobe = true;
    json_t *a = json_object();
    json_set(a, "name", json_string("mutagen"));
    json_t *m = da13_call(ctx, "import_module", a);
    if (da13_is_raise(m)) {
        if (!da13_is_exception(da13_raise_kind(m))) return m;
        json_free(m);
    } else {
        json_free(m);
        a = json_object(); json_set(a, "path", json_string(path ? path : ""));
        json_t *audio = da13_call(ctx, "mutagen_file", a);
        if (da13_is_raise(audio)) {
            if (!da13_is_exception(da13_raise_kind(audio))) return audio;
            json_free(audio);
        } else {
            const json_t *info = da13_getattr(audio, "info");
            const json_t *len = da13_getattr(info, "length");
            if (da13_truthy(len)) {
                double d = 0.0;
                if (da13_float(len, &d)) { json_free(audio); return json_number(d); }
                /* float() failed inside the try -> swallowed -> ffprobe */
            }
            json_free(audio);
        }
    }
    /* ffprobe fallback */
    json_t *cmd = json_array();
    const char *const parts[] = {"ffprobe", "-v", "error", "-show_entries",
        "format=duration", "-of", "default=noprint_wrappers=1:nokey=1"};
    for (size_t i = 0; i < sizeof(parts)/sizeof(parts[0]); i++)
        json_append(cmd, json_string(parts[i]));
    json_append(cmd, json_string(path ? path : ""));
    a = json_object();
    json_set(a, "cmd", cmd);
    json_set(a, "check", json_bool(false));
    json_set(a, "capture_output", json_bool(true));
    json_set(a, "text", json_bool(true));
    json_set(a, "timeout", json_number(5));
    json_set(a, "stdin", json_number(-3));
    json_t *proc = da13_call(ctx, "subprocess_run", a);
    if (da13_is_raise(proc)) {
        if (!da13_is_exception(da13_raise_kind(proc))) return proc;
        json_free(proc); return json_null();
    }
    double rc = da13_num(da13_getattr(proc, "returncode"));
    if (rc == 0.0) {
        const json_t *so = da13_getattr(proc, "stdout");
        char *raw = da13_trim(da13_str(so));
        if (raw && raw[0]) {
            double d = 0.0;
            bool ok = false;
            if (so && so->type == JSON_STRING) ok = da13_float(so, &d);
            /* after strip: parse the trimmed string */
            if (!ok) ok = da13_float(json_string(raw), &d);
            free(raw);
            json_free(proc);
            return ok ? json_number(d) : json_null();
        }
        free(raw);
    }
    json_free(proc);
    return json_null();
}

/* PoP: da13_process_voice_input @ plugins/platforms/discord/adapter.py:_process_voice_input */
static json_t *da13_process_voice_input(da13_ctx_t *ctx, const json_t *args) {
    json_t *a = json_object();
    json_set(a, "suffix", json_string(".wav"));
    json_set(a, "prefix", json_string("vc_listen_"));
    json_set(a, "delete", json_bool(false));
    json_t *tmp = da13_call(ctx, "named_tempfile", a);
    if (da13_is_raise(tmp)) return tmp;
    const char *wav = da13_str(da13_getattr(tmp, "name"));
    char *wavp = da13_strdup(wav ? wav : "");
    json_free(tmp);
    json_t *pending = NULL;      /* raise spec captured by the try body */
    json_t *result = NULL;
    goto try_start;

try_start:
    /* await asyncio.to_thread(VoiceReceiver.pcm_to_wav, pcm_data, wav_path) */
    a = json_object();
    json_set(a, "func", json_string("pcm_to_wav"));
    json_t *pa = json_array();
    json_append(pa, da13_copyv(da13_arg(args, "pcm_data")));
    json_append(pa, json_string(wavp));
    json_set(a, "args", pa);
    {
        json_t *t = da13_call(ctx, "to_thread", a);
        if (da13_is_raise(t)) { pending = t; goto except_handler; }
        json_free(t);
    }
    /* result = await asyncio.to_thread(transcribe_audio, wav_path) */
    a = json_object();
    json_set(a, "func", json_string("transcribe_audio"));
    pa = json_array(); json_append(pa, json_string(wavp));
    json_set(a, "args", pa);
    result = da13_call(ctx, "to_thread", a);
    if (da13_is_raise(result)) { pending = result; result = NULL; goto except_handler; }
    if (!da13_truthy(json_obj_get(result, "success"))) goto fin;
    {
        const json_t *tr = json_obj_get(result, "transcript");
        char *str = da13_trim(da13_str(tr));
        if (!str || !str[0]) { free(str); goto fin; }
        json_t *ha = json_object();
        json_set(ha, "transcript", json_copy(json_string(str)));
        free(str);
        json_t *h = da13_call(ctx, "is_whisper_hallucination", ha);
        if (da13_is_raise(h)) { pending = h; goto except_handler; }
        bool halluc = da13_truthy(h); json_free(h);
        if (halluc) goto fin;
        /* logger.info("Voice input from user %d (%d chars)", user_id, len(transcript)) */
        json_t *la = json_array();
        json_append(la, json_string("Voice input from user %d (%d chars)"));
        json_append(la, da13_copyv(da13_arg(args, "user_id")));
        const char *ts = da13_str(json_obj_get(result, "transcript"));
        char *trimmed = da13_trim(ts);
        json_append(la, json_number((double)strlen(trimmed ? trimmed : "")));
        free(trimmed);
        json_t *li = da13_call(ctx, "logger_info", da13_log_args(la));
        if (da13_is_raise(li)) { pending = li; goto except_handler; }
        json_free(li);
        /* binding moved? */
        const json_t *vt = da13_arg(args, "voice_text_channels");
        char key[64];
        snprintf(key, sizeof key, "%.0f", da13_num(da13_arg(args, "guild_id")));
        const json_t *bound = da13_getattr(vt, key);
        if (!da13_json_eq(bound, da13_arg(args, "captured_for"))) {
            json_t *la2 = json_array();
            json_append(la2, json_string("Dropping voice input from user %d: binding moved during transcription"));
            json_append(la2, da13_copyv(da13_arg(args, "user_id")));
            json_t *li2 = da13_call(ctx, "logger_info", da13_log_args(la2));
            if (da13_is_raise(li2)) { pending = li2; goto except_handler; }
            json_free(li2);
            goto fin;
        }
        if (!da13_truthy(da13_arg(args, "voice_cb")) &&
            !da13_has(ctx->state, "_voice_input_callback")) {
            pending = da13_err("AttributeError",
                "'DA13Double' object has no attribute '_voice_input_callback'");
            goto except_handler;
        }
        if (da13_truthy(da13_arg(args, "voice_cb")) || da13_has(ctx->state, "_voice_input_callback")) {
            json_t *ca = json_object();
            json_set(ca, "guild_id", da13_copyv(da13_arg(args, "guild_id")));
            json_set(ca, "user_id", da13_copyv(da13_arg(args, "user_id")));
            char *ttrim = da13_trim(da13_str(json_obj_get(result, "transcript")));
            json_set(ca, "transcript", json_string(ttrim ? ttrim : ""));
            free(ttrim);
            json_t *cb = da13_call(ctx, "voice_input_callback", ca);
            if (da13_is_raise(cb)) { pending = cb; goto except_handler; }
            json_free(cb);
        }
    }
    goto fin;

except_handler:
    /* except Exception: log; BaseException passes straight to finally */
    if (!da13_is_exception(da13_raise_kind(pending))) goto fin;
    {
        json_t *la = json_array();
        json_append(la, json_string("Voice input processing failed: %s"));
        json_append(la, json_copy(pending));
        json_t *lw = da13_call(ctx, "logger_warning", da13_log_args_kw(la, true));
        if (da13_is_raise(lw)) { json_free(pending); pending = lw; goto fin; }
        json_free(lw);
        json_free(pending); pending = NULL;
    }
fin:
    /* finally: os.unlink(wav_path) with except OSError: pass */
    {
        json_t *ua = json_object(); json_set(ua, "path", json_string(wavp));
        json_t *u = da13_call(ctx, "os_unlink", ua);
        if (da13_is_raise(u)) {
            const char *k = da13_raise_kind(u);
            if (k && da13_chain_has(k, "OSError")) { /* swallowed */ }
            else if (!pending) pending = u;
            else json_free(u);
        }
    }
    free(wavp);
    if (result) json_free(result);
    return pending ? pending : json_null();
}

/* PoP: da13_read_attachment_bytes @ plugins/platforms/discord/adapter.py:_read_attachment_bytes */
static json_t *da13_read_attachment_bytes(da13_ctx_t *ctx, const json_t *args) {
    const json_t *att = da13_arg(args, "att");
    const char *mt = da13_str(da13_arg(args, "media_type"));
    if (!mt) mt = "media";
    const json_t *sz = da13_getattr(att, "size");
    if (da13_truthy(sz)) {
        double n = 0.0;
        if (!da13_float(sz, &n)) return da13_err("ValueError", "invalid literal for int()");
        json_t *a = json_object();
        json_set(a, "size", json_number(n));
        json_set(a, "media_type", json_string(mt));
        json_t *v = da13_call(ctx, "validate_media_size", a);
        if (da13_is_raise(v)) return v;
        json_free(v);
    }
    const json_t *reader = da13_getattr(att, "read");
    if (!reader || reader->type != JSON_STRING || strcmp(reader->str_val, "callable") != 0)
        return json_null();
    json_t *raw = da13_call(ctx, "attachment_read", json_object());
    if (da13_is_raise(raw)) {
        json_t *e = raw;
        if (!da13_is_exception(da13_raise_kind(e))) return e;
        const json_t *fn = da13_getattr(att, "filename");
        const char *name = da13_str(fn);
        if (!name || !name[0]) name = da13_str(da13_getattr(att, "url"));
        if (!name) name = "<unknown>";
        json_t *la = json_array();
        json_append(la, json_string("[Discord] Authenticated attachment read failed for %s: %s"));
        json_append(la, json_string(name));
        json_append(la, json_copy(e));
        json_t *w = da13_call(ctx, "logger_warning", da13_log_args_kw(la, false));
        if (da13_is_raise(w)) return w;
        json_free(w);
        json_free(e);
        return json_null();
    }
    size_t n = raw && raw->type == JSON_STRING ? strlen(raw->str_val) : 0;
    json_t *a = json_object();
    json_set(a, "size", json_number((double)n));
    json_set(a, "media_type", json_string(mt));
    json_t *v = da13_call(ctx, "validate_media_size", a);
    if (da13_is_raise(v)) { json_free(raw); return v; }
    json_free(v);
    return raw;
}

/* PoP: da13_read_url_image_with_redirect_guard @ plugins/platforms/discord/adapter.py:_read_url_image_with_redirect_guard */
static json_t *da13_read_url_image_with_redirect_guard(da13_ctx_t *ctx, const json_t *args) {
    const char *url = da13_str(da13_arg(args, "url"));
    char *cur = da13_strdup(url ? url : "");
    const json_t *rkw = da13_arg(args, "request_kwargs");
    for (int iter = 0; iter <= DA13_MAX_REDIRECTS; iter++) {
        json_t *a = json_object(); json_set(a, "url", json_string(cur));
        json_t *safe = da13_call(ctx, "is_safe_url", a);
        if (da13_is_raise(safe)) { free(cur); return safe; }
        bool ok = da13_truthy(safe); json_free(safe);
        if (!ok) { free(cur); return da13_err("ValueError", "Blocked unsafe image URL redirect"); }
        a = json_object();
        json_set(a, "url", json_string(cur));
        json_set(a, "timeout", da13_copyv(da13_arg(args, "timeout")));
        json_set(a, "allow_redirects", json_bool(false));
        if (rkw && rkw->type == JSON_OBJECT)
            for (size_t i = 0; i < rkw->c.count; i++)
                json_set(a, rkw->c.keys[i], json_copy(rkw->c.items[i]));
        json_t *resp = da13_call(ctx, "session_get", a);
        if (da13_is_raise(resp)) { free(cur); return resp; }
        const json_t *rh = da13_getattr(resp, "headers");
        json_t *headers = json_object();
        if (rh && rh->type == JSON_OBJECT)
            for (size_t i = 0; i < rh->c.count; i++) {
                char *low = da13_lower(rh->c.keys[i]);
                json_set(headers, low, json_copy(rh->c.items[i]));
                free(low);
            }
        double status = da13_num(da13_getattr(resp, "status"));
        bool redirect = (status == 301 || status == 302 || status == 303 ||
                         status == 307 || status == 308);
        if (redirect) {
            const json_t *loc = json_obj_get(headers, "location");
            if (da13_is_null(loc) || !da13_str(loc) || !da13_str(loc)[0]) {
                json_t *out = json_array();
                json_append(out, json_number(status));
                json_append(out, json_string(""));
                json_append(out, headers);
                json_free(resp); free(cur);
                return out;
            }
            json_t *ja = json_object();
            json_set(ja, "base", json_string(cur));
            json_set(ja, "rel", json_copy(loc));
            json_t *nextu = da13_call(ctx, "urljoin", ja);
            if (da13_is_raise(nextu)) { json_free(headers); json_free(resp); free(cur); return nextu; }
            const char *nu = da13_str(nextu);
            json_t *sa = json_object(); json_set(sa, "url", json_copy(nextu));
            json_t *safe2 = da13_call(ctx, "is_safe_url", sa);
            if (da13_is_raise(safe2)) { json_free(nextu); json_free(headers); json_free(resp); free(cur); return safe2; }
            bool ok2 = da13_truthy(safe2); json_free(safe2);
            if (!ok2) { json_free(nextu); json_free(headers); json_free(resp); free(cur);
                        return da13_err("ValueError", "Blocked redirect to private/internal address"); }
            free(cur); cur = da13_strdup(nu ? nu : "");
            json_free(nextu); json_free(headers); json_free(resp);
            continue;
        }
        json_t *body = da13_call(ctx, "resp_read", json_object());
        if (da13_is_raise(body)) { json_free(headers); json_free(resp); free(cur); return body; }
        json_t *out = json_array();
        json_append(out, json_number(status));
        json_append(out, json_copy(body));
        json_append(out, headers);
        json_free(body); json_free(resp); free(cur);
        return out;
    }
    free(cur);
    return da13_err("ValueError", "Too many image URL redirects");
}

/* PoP: da13_observe_exec_approval_settle @ plugins/platforms/discord/adapter.py:_observe_exec_approval_settle */
static json_t *da13_observe_exec_approval_settle(da13_ctx_t *ctx, const json_t *args) {
    json_t *loop = da13_call(ctx, "get_running_loop", json_object());
    if (da13_is_raise(loop)) return loop;
    json_free(loop);
    const json_t *view = da13_arg(args, "view");
    json_t *a = json_object();
    json_set(a, "session_key", da13_copyv(da13_arg(args, "session_key")));
    json_set(a, "request_id", da13_copyv(da13_getattr(view, "request_id")));
    json_t *res = da13_call(ctx, "observe_gateway_settle", a);
    if (da13_is_raise(res)) return res;
    json_t *rsn = da13_call(ctx, "settle_reason", json_object());
    if (da13_is_raise(rsn)) { json_free(res); return rsn; }
    /* loop.call_soon_threadsafe(schedule) -- recorded before the body runs */
    json_t *cs = da13_call(ctx, "call_soon_threadsafe", json_object());
    if (da13_is_raise(cs)) { json_free(rsn); json_free(res); return cs; }
    json_free(cs);
    const json_t *vm = da13_getattr(view, "_message");
    if (da13_is_null(vm)) {
        /* view.pending_retire = reason -> state echo */
        json_t *sv = (json_t *)da13_state_get(ctx, "view");
        if (sv && sv->type == JSON_OBJECT) json_set(sv, "pending_retire", json_copy(rsn));
        json_free(rsn);
        return res;
    }
    json_t *ra = json_object(); json_set(ra, "reason", json_copy(rsn));
    json_t *vr = da13_call(ctx, "view_retire", ra);
    if (da13_is_raise(vr)) { json_free(rsn); json_free(res); return vr; }
    json_free(vr);
    json_t *ca = json_object(); json_set(ca, "coro", json_null());
    json_t *task = da13_call(ctx, "loop_create_task", ca);
    if (da13_is_raise(task)) { json_free(rsn); json_free(res); return task; }
    json_t *ba = json_object(); json_set(ba, "task", json_copy(task));
    json_t *bg = da13_call(ctx, "bg_add", ba);
    if (da13_is_raise(bg)) { json_free(task); json_free(rsn); json_free(res); return bg; }
    json_free(bg);
    json_t *d1 = json_object(); json_set(d1, "cb", json_string("discard"));
    json_t *c1 = da13_call(ctx, "add_done_callback", d1);
    if (da13_is_raise(c1)) { json_free(task); json_free(rsn); json_free(res); return c1; }
    json_free(c1);
    json_t *d2 = json_object(); json_set(d2, "cb", json_string("_consume_background_task_result"));
    json_t *c2 = da13_call(ctx, "add_done_callback", d2);
    if (da13_is_raise(c2)) { json_free(task); json_free(rsn); json_free(res); return c2; }
    json_free(c2);
    json_free(task); json_free(rsn);
    return res;
}

/* PoP: da13_on_packet @ plugins/platforms/discord/adapter.py:_on_packet */
static json_t *da13_on_packet(da13_ctx_t *ctx, const json_t *args) {
    if (!da13_truthy(da13_state_get(ctx, "_running")) ||
        da13_truthy(da13_state_get(ctx, "_paused")))
        return json_null();
    /* LIVE computes bytes.fromhex(data) BEFORE calling _on_packet, so an
       invalid hex string raises ValueError before any state mutation or seam. */
    const char *hex = da13_str(da13_arg(args, "data"));
    size_t n = 0;
    const char *hexerr = NULL;
    unsigned char *data = da13_hex_checked(hex ? hex : "", &n, &hexerr);
    if (hexerr) {
        json_t *e = da13_err("ValueError", hexerr);
        free((void *)hexerr);
        return e;
    }
    const json_t *creds = da13_state_get(ctx, "_creds");
    const json_t *secret = json_get(creds, 0);
    const json_t *dave = json_get(creds, 1);
    double dave_pver = da13_num(json_get(creds, 2));

    double cnt = da13_num(da13_state_get(ctx, "_packet_debug_count")) + 1.0;
    da13_state_set(ctx, "_packet_debug_count", json_number(cnt));

    bool show = cnt <= 5.0 || cnt <= 10.0;
    if (cnt <= 5.0) {
        char *first = n >= 4 ? da13_bytes_hex(data, 4) : da13_strdup("short");
        json_t *la = json_array();
        json_append(la, json_string("Raw UDP packet: len=%d, first_bytes=%s"));
        json_append(la, json_number((double)n));
        json_append(la, json_string(first));
        free(first);
        json_t *r = da13_call(ctx, "logger_debug", da13_log_args(la));
        if (da13_is_raise(r)) { free(data); return r; }
        json_free(r);
    }
    if (n < 16) { free(data); return json_null(); }
    unsigned int b0 = data[0], b1 = data[1];
    if ((b0 >> 6) != 2 || (b1 & 0x7f) != 0x78) {
        if (cnt <= 5.0) {
            json_t *la = json_array();
            json_append(la, json_string("Skipped non-RTP: byte0=0x%02x byte1=0x%02x"));
            json_append(la, json_number((double)b0));
            json_append(la, json_number((double)b1));
            json_t *r = da13_call(ctx, "logger_debug", da13_log_args(la));
            if (da13_is_raise(r)) { free(data); return r; }
            json_free(r);
        }
        free(data); return json_null();
    }
    unsigned int seq = ((unsigned int)data[2] << 8) | data[3];
    unsigned int ssrc = ((unsigned int)data[8] << 24) | ((unsigned int)data[9] << 16) |
                        ((unsigned int)data[10] << 8) | data[11];
    if ((double)ssrc == da13_num(da13_state_get(ctx, "_bot_ssrc"))) { free(data); return json_null(); }
    unsigned int cc = b0 & 0x0f;
    bool has_ext = (b0 & 0x10) != 0;
    bool has_pad = (b0 & 0x20) != 0;
    size_t header_size = 12 + 4 * cc + (has_ext ? 4 : 0);
    if (n < header_size + 4) { free(data); return json_null(); }
    size_t ext_data_len = 0;
    if (has_ext) {
        size_t off = 12 + 4 * cc;
        unsigned int words = ((unsigned int)data[off + 2] << 8) | data[off + 3];
        ext_data_len = words * 4;
    }
    char *ssrc_key = da13_fmt("%.0f", (double)ssrc);
    if (cnt <= 10.0) {
        const json_t *map = da13_state_get(ctx, "_ssrc_to_user");
        const json_t *known = da13_getattr(map, ssrc_key);
        const char *user = da13_str(known);
        if (!user) user = "unknown";
        json_t *la = json_array();
        json_append(la, json_string("RTP packet: ssrc=%d, seq=%d, user=%s, hdr=%d, ext_data=%d"));
        json_append(la, json_number((double)ssrc));
        json_append(la, json_number((double)seq));
        json_append(la, json_string(user));
        json_append(la, json_number((double)header_size));
        json_append(la, json_number((double)ext_data_len));
        json_t *r = da13_call(ctx, "logger_debug", da13_log_args(la));
        free(ssrc_key); ssrc_key = da13_fmt("%.0f", (double)ssrc);
        if (da13_is_raise(r)) { free(data); return r; }
        json_free(r);
    }
    char *header_hex = da13_bytes_hex(data, header_size);
    size_t pwn_len = n - header_size;
    unsigned char *pwn = data + header_size;
    if (pwn_len < 4) { free(header_hex); free(ssrc_key); free(data); return json_null(); }
    unsigned char nonce[24] = {0};
    memcpy(nonce, pwn + pwn_len - 4, 4);
    size_t enc_len = pwn_len - 4;
    char *nonce_hex = da13_bytes_hex(nonce, sizeof nonce);
    char *enc_hex = da13_bytes_hex(pwn, enc_len);
    json_t *a = json_object();
    json_set(a, "secret_key", da13_copyv(secret));
    json_set(a, "encrypted", json_string(enc_hex));
    json_set(a, "header", json_string(header_hex));
    json_set(a, "nonce", json_string(nonce_hex));
    free(nonce_hex); free(enc_hex); free(header_hex);
    json_t *dec = da13_call(ctx, "nacl_decrypt", a);
    char *dec_bytes = NULL; size_t dec_n = 0;
    json_t *final = NULL;
    json_t *dec_fail = NULL;   /* exception driving the decrypt-failure branch */
    if (da13_is_raise(dec)) {
        dec_fail = dec;
        if (!da13_is_exception(da13_raise_kind(dec_fail))) {
            free(ssrc_key); free(data); return dec_fail;
        }
    } else {
        /* LIVE converts the seam string with bytes.fromhex() inside the try;
         * invalid hex raises ValueError and takes the same failure path. */
        const char *hexerr = NULL;
        dec_bytes = da13_hex_checked(da13_str(dec), &dec_n, &hexerr);
        json_free(dec);
        if (!dec_bytes) {
            dec_fail = da13_err("ValueError", hexerr ? hexerr : "");
            free((void *)hexerr);
        }
    }
    if (dec_fail) {
        json_t *e = dec_fail;
        double failed = da13_num(da13_state_get(ctx, "_decode_failed")) + 1.0;
        da13_state_set(ctx, "_decode_failed", json_number(failed));
        double streak = da13_num(da13_state_get(ctx, "_nacl_fail_streak")) + 1.0;
        da13_state_set(ctx, "_nacl_fail_streak", json_number(streak));
        if (cnt <= 10.0 || ((long)streak % 250) == 0) {
            json_t *la = json_array();
            json_append(la, json_string("NaCl decrypt failed: %s (hdr=%d, enc=%d, streak=%d)"));
            json_append(la, json_copy(e));
            json_append(la, json_number((double)header_size));
            json_append(la, json_number((double)enc_len));
            json_append(la, json_number(streak));
            json_t *w = da13_call(ctx, "logger_warning", da13_log_args_kw(la, false));
            if (da13_is_raise(w)) { json_free(e); free(ssrc_key); free(data); return w; }
            json_free(w);
        }
        if (streak >= DA13_REKEY_FAILURE_STREAK) {
            json_t *ra = json_object(); json_set(ra, "reason", json_string("decrypt-failure streak"));
            json_t *rf = da13_call(ctx, "refresh_credentials", ra);
            if (da13_is_raise(rf)) { json_free(e); free(ssrc_key); free(data); return rf; }
            json_free(rf);
        }
        json_free(e); free(ssrc_key); free(data);
        return json_null();
    }
    da13_state_set(ctx, "_nacl_fail_streak", json_number(0));
    size_t dstart = 0;
    if (ext_data_len > 0 && dec_n > ext_data_len) dstart = ext_data_len;
    size_t dlen = dec_n - dstart;
    if (has_pad) {
        if (dlen == 0) {
            if (cnt <= 10.0) {
                json_t *la = json_array();
                json_append(la, json_string("RTP padding bit set but no payload (ssrc=%d)"));
                json_append(la, json_number((double)ssrc));
                json_t *w = da13_call(ctx, "logger_warning", da13_log_args_kw(la, false));
                if (da13_is_raise(w)) { free(dec_bytes); free(ssrc_key); free(data); return w; }
                json_free(w);
            }
            free(dec_bytes); free(ssrc_key); free(data); return json_null();
        }
        unsigned int pad = dec_bytes[dstart + dlen - 1];
        if (pad == 0 || pad > dlen) {
            if (cnt <= 10.0) {
                json_t *la = json_array();
                json_append(la, json_string("Invalid RTP padding length %d for payload size %d (ssrc=%d)"));
                json_append(la, json_number((double)pad));
                json_append(la, json_number((double)dlen));
                json_append(la, json_number((double)ssrc));
                json_t *w = da13_call(ctx, "logger_warning", da13_log_args_kw(la, false));
                if (da13_is_raise(w)) { free(dec_bytes); free(ssrc_key); free(data); return w; }
                json_free(w);
            }
            free(dec_bytes); free(ssrc_key); free(data); return json_null();
        }
        dlen -= pad;
        if (dlen == 0) { free(dec_bytes); free(ssrc_key); free(data); return json_null(); }
    }
    char *payload_hex = da13_bytes_hex(dec_bytes + dstart, dlen);
    free(dec_bytes);
    /* DAVE */
    if (!da13_is_null(dave) && dave && dave->type == JSON_OBJECT) {
        const json_t *map = da13_state_get(ctx, "_ssrc_to_user");
        const json_t *uidv = da13_getattr(map, ssrc_key);
        double user_id = da13_truthy(uidv) ? da13_num(uidv) : 0.0;
        if (!da13_truthy(uidv)) {
            json_t *ia = json_object(); json_set(ia, "ssrc", json_number((double)ssrc));
            json_t *iu = da13_call(ctx, "infer_user_for_ssrc", ia);
            if (da13_is_raise(iu)) { free(payload_hex); free(ssrc_key); free(data); return iu; }
            user_id = da13_num(iu); json_free(iu);
        }
        if (da13_truthy(uidv) || user_id != 0.0) {
            json_t *da = json_object();
            json_set(da, "user_id", json_number(user_id));
            json_set(da, "media_type", json_string("audio"));
            json_set(da, "data", json_string(payload_hex));
            json_t *dd = da13_call(ctx, "dave_decrypt", da);
            if (da13_is_raise(dd)) {
                json_t *e = dd;
                if (!da13_is_exception(da13_raise_kind(e))) {
                    free(payload_hex); free(ssrc_key); free(data); return e;
                }
                const char *msg = json_get_str(e, "message", "");
                bool unencrypted = msg && strstr(msg, "Unencrypted");
                if (!unencrypted) {
                    if (cnt <= 10.0) {
                        json_t *la = json_array();
                        json_append(la, json_string("DAVE decrypt failed for ssrc=%d: %s"));
                        json_append(la, json_number((double)ssrc));
                        json_append(la, json_copy(e));
                        json_t *w = da13_call(ctx, "logger_warning", da13_log_args_kw(la, false));
                        if (da13_is_raise(w)) { json_free(e); free(payload_hex); free(ssrc_key); free(data); return w; }
                        json_free(w);
                    }
                    json_free(e); free(payload_hex); free(ssrc_key); free(data);
                    return json_null();
                }
                json_free(e);
            } else {
                free(payload_hex);
                {
                    char *low = da13_lower(da13_str(dd));
                    payload_hex = low ? low : da13_strdup("");
                }
                json_free(dd);
            }
        } else if (dave_pver > 0) {
            json_t *mn = da13_call(ctx, "monotonic", json_object());
            if (da13_is_raise(mn)) { free(payload_hex); free(ssrc_key); free(data); return mn; }
            double now = da13_num(mn); json_free(mn);
            if (now >= da13_num(da13_state_get(ctx, "_dave_passthrough_until"))) {
                double dropped = da13_num(da13_state_get(ctx, "_dave_unmapped_dropped")) + 1.0;
                da13_state_set(ctx, "_dave_unmapped_dropped", json_number(dropped));
                if (cnt <= 10.0 || ((long)dropped % 250) == 1) {
                    json_t *la = json_array();
                    json_append(la, json_string("Dropping DAVE frame for unmapped ssrc=%d (dropped=%d)"));
                    json_append(la, json_number((double)ssrc));
                    json_append(la, json_number(dropped));
                    json_t *r = da13_call(ctx, "logger_debug", da13_log_args(la));
                    if (da13_is_raise(r)) { free(payload_hex); free(ssrc_key); free(data); return r; }
                    json_free(r);
                }
                free(payload_hex); free(ssrc_key); free(data);
                return json_null();
            }
        }
    }
    /* Opus decode */
    {
        const json_t *decs = da13_state_get(ctx, "_decoders");
        json_t *newdecs = decs && decs->type == JSON_OBJECT ? json_copy((json_t *)decs) : json_object();
        json_set(newdecs, ssrc_key, json_string("<decoder>"));
        da13_state_set(ctx, "_decoders", newdecs);
        json_t *oa = json_object();
        json_set(oa, "ssrc", json_number((double)ssrc));
        json_set(oa, "data", json_string(payload_hex));
        json_t *op = da13_call(ctx, "opus_decode", oa);
        if (da13_is_raise(op)) {
            json_t *e = op;
            json_t *curdecs = (json_t *)da13_state_get(ctx, "_decoders");
            if (curdecs && curdecs->type == JSON_OBJECT) json_obj_del(curdecs, ssrc_key);
            if (!da13_is_exception(da13_raise_kind(e))) {
                free(payload_hex); free(ssrc_key); free(data); return e;
            }
            json_t *la = json_array();
            json_append(la, json_string("Opus decode error for SSRC %s; reset decoder: %s"));
            json_append(la, json_number((double)ssrc));
            json_append(la, json_copy(e));
            json_t *r = da13_call(ctx, "logger_debug", da13_log_args(la));
            if (da13_is_raise(r)) { json_free(e); free(payload_hex); free(ssrc_key); free(data); return r; }
            json_free(r); json_free(e);
            free(payload_hex); free(ssrc_key); free(data);
            return json_null();
        }
        double ok = da13_num(da13_state_get(ctx, "_decode_ok")) + 1.0;
        da13_state_set(ctx, "_decode_ok", json_number(ok));
        /* buffers[ssrc].extend(pcm) */
        const json_t *bufs = da13_state_get(ctx, "_buffers");
        json_t *newbufs = bufs && bufs->type == JSON_OBJECT ? json_copy((json_t *)bufs) : json_object();
        json_t *buf = json_obj_get(newbufs, ssrc_key);
        json_t *list = buf && buf->type == JSON_ARRAY ? json_copy(buf) : json_array();
        if (op && op->type == JSON_ARRAY)
            for (size_t i = 0; i < op->c.count; i++) json_append(list, json_copy(op->c.items[i]));
        json_free(op);
        json_set(newbufs, ssrc_key, list);
        da13_state_set(ctx, "_buffers", newbufs);
        json_t *mn = da13_call(ctx, "monotonic", json_object());
        if (da13_is_raise(mn)) {
            final = mn;
        } else {
            const json_t *lpt = da13_state_get(ctx, "_last_packet_time");
            json_t *newlpt = lpt && lpt->type == JSON_OBJECT ? json_copy((json_t *)lpt) : json_object();
            json_set(newlpt, ssrc_key, json_copy(mn));
            json_free(mn);
            da13_state_set(ctx, "_last_packet_time", newlpt);
        }
    }
    free(payload_hex); free(ssrc_key); free(data);
    return final ? final : json_null();
}
