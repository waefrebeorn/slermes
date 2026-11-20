/*
 * tui_methods_session_flow2.c -- FLOW port of the UNCLAIMED remainder of
 * tui_gateway/methods_session.py (hermes-agent, SINGULAR -- there is no
 * methods_sessions.py).  Companion to tui_methods_sessions_flow.c (flow1,
 * which claimed 31 helper/handler funcs) and src/cli/port_methods_session.c
 * (5 leaf params).  Every function below was verified unclaimed by a CRLF-aware
 * PoP grep over src/ before this file was written (flow2-remainder-scoping.md).
 *
 * Ports the DECISION SEQUENCES of the remaining TUI session handlers/helpers:
 * branch order, int()/str() coercion, tuple unpack, caller-dict mutation,
 * exception classification, error-code/message composition and seam call
 * order -- not just leaf outputs.  Each live external call enters through ONE
 * universal seam tms2_call(ctx, name, args); the harness wires it to a
 recording stub fed the case's scripts (in.io); the oracle installs the
 * identical recorder.  _ok/_err are real deterministic envelope constructors
 * (server.py:747/751) present on both sides, so they are NOT seams.
 *
 * Self-contained: the harness #includes this .c directly; libjson only.
 * All symbols are static and tms2_-prefixed.
 */
#include "hermes_json.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -- universal seam context -- */
typedef struct tms2_ctx {
    json_t *state;      /* owned; mutated in place, emitted as out.state */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} tms2_ctx_t;

/* -- basic helpers -- */
static bool tms2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *tms2_raise_kind(const json_t *v) {
    return tms2_is_raise(v) ? json_get_str(v, "__raise__", "") : NULL;
}
static const char *tms2_raise_msg(const json_t *v) {
    return tms2_is_raise(v) ? json_get_str(v, "message", "") : "";
}
static json_t *tms2_err_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type ? type : "RuntimeError");
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

/* Python truthiness: None/False/0/""/empty containers are falsy. */
static bool tms2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *tms2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static const char *tms2_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return (v->num_val == (double)(long long)v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}
static char *tms2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
/* Python str() for scalars (container str() is outside the fixture domain). */
static char *tms2_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return tms2_strdup("None");
    switch (v->type) {
        case JSON_BOOL: return tms2_strdup(v->bool_val ? "True" : "False");
        case JSON_STRING: return tms2_strdup(v->str_val ? v->str_val : "");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == (double)(long long)d && d > -9.2e18 && d < 9.2e18)
                snprintf(buf, sizeof buf, "%lld", (long long)d);
            else snprintf(buf, sizeof buf, "%.17g", d);
            return tms2_strdup(buf);
        }
        default: return json_serialize(v);
    }
}
/* Python str.strip() whitespace: ASCII White_Space INCLUDING \x1c-\x1f. */
static bool tms2_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f'
        || c == '\v' || (c >= 0x1c && c <= 0x1f);
}
static char *tms2_strip(const char *s) {
    if (!s) s = "";
    while (*s && tms2_is_ws((unsigned char)*s)) s++;
    const char *end = s + strlen(s);
    while (end > s && tms2_is_ws((unsigned char)end[-1])) end--;
    size_t n = (size_t)(end - s);
    char *o = malloc(n + 1);
    if (o) { memcpy(o, s, n); o[n] = '\0'; }
    return o;
}

/* -- full builtin exception hierarchy (parent map) -- */
/* Returns the PARENT class name of `kind` in Python's builtins hierarchy.
 * Target-interpreter audit (py3.14): CancelledError -> BaseException;
 * OSError aliases fold to OSError; ExceptionGroup -> Exception. */
static const char *tms2_exc_parent(const char *k) {
    if (!k) return NULL;
    /* BaseException roots: no Python parent (return NULL). */
    if (!strcmp(k, "BaseException")) return NULL;
    if (!strcmp(k, "SystemExit") || !strcmp(k, "KeyboardInterrupt")
        || !strcmp(k, "GeneratorExit") || !strcmp(k, "CancelledError")) return "BaseException";
    if (!strcmp(k, "Exception") || !strcmp(k, "ExceptionGroup")) return "BaseException";
    /* ArithmeticError family */
    if (!strcmp(k, "OverflowError") || !strcmp(k, "ZeroDivisionError") || !strcmp(k, "FloatingPointError")) return "ArithmeticError";
    if (!strcmp(k, "ArithmeticError")) return "Exception";
    /* LookupError family */
    if (!strcmp(k, "IndexError") || !strcmp(k, "KeyError")) return "LookupError";
    if (!strcmp(k, "LookupError")) return "Exception";
    /* ImportError family */
    if (!strcmp(k, "ModuleNotFoundError")) return "ImportError";
    if (!strcmp(k, "ImportError")) return "Exception";
    /* NameError family */
    if (!strcmp(k, "UnboundLocalError")) return "NameError";
    if (!strcmp(k, "NameError")) return "Exception";
    /* RuntimeError family */
    if (!strcmp(k, "NotImplementedError") || !strcmp(k, "RecursionError")) return "RuntimeError";
    if (!strcmp(k, "RuntimeError")) return "Exception";
    /* SyntaxError family */
    if (!strcmp(k, "IndentationError")) return "SyntaxError";
    if (!strcmp(k, "TabError")) return "IndentationError";
    if (!strcmp(k, "SyntaxError")) return "Exception";
    /* OSError family (aliases fold to OSError) */
    if (!strcmp(k, "FileNotFoundError") || !strcmp(k, "FileExistsError")
        || !strcmp(k, "IsADirectoryError") || !strcmp(k, "NotADirectoryError")
        || !strcmp(k, "PermissionError") || !strcmp(k, "ProcessLookupError")
        || !strcmp(k, "TimeoutError") || !strcmp(k, "InterruptedError")
        || !strcmp(k, "BlockingIOError") || !strcmp(k, "ChildProcessError")
        || !strcmp(k, "BrokenPipeError") || !strcmp(k, "ConnectionAbortedError")
        || !strcmp(k, "ConnectionRefusedError") || !strcmp(k, "ConnectionResetError")
        || !strcmp(k, "EnvironmentError") || !strcmp(k, "IOError")
        || !strcmp(k, "WindowsError")) return "OSError";
    if (!strcmp(k, "ConnectionError")) return "OSError";
    if (!strcmp(k, "OSError")) return "Exception";
    /* UnicodeError family */
    if (!strcmp(k, "UnicodeDecodeError") || !strcmp(k, "UnicodeEncodeError")
        || !strcmp(k, "UnicodeTranslateError")) return "UnicodeError";
    if (!strcmp(k, "UnicodeError")) return "ValueError";
    /* ValueError family */
    if (!strcmp(k, "ValueError")) return "Exception";
    /* Warning family */
    if (!strcmp(k, "BytesWarning") || !strcmp(k, "DeprecationWarning")
        || !strcmp(k, "EncodingWarning") || !strcmp(k, "FutureWarning")
        || !strcmp(k, "ImportWarning") || !strcmp(k, "PendingDeprecationWarning")
        || !strcmp(k, "ResourceWarning") || !strcmp(k, "RuntimeWarning")
        || !strcmp(k, "SyntaxWarning") || !strcmp(k, "UnicodeWarning")
        || !strcmp(k, "UserWarning")) return "Warning";
    if (!strcmp(k, "Warning")) return "Exception";
    /* StopIteration / StopAsyncIteration and the remaining direct Exception children */
    if (!strcmp(k, "StopIteration") || !strcmp(k, "StopAsyncIteration")
        || !strcmp(k, "AssertionError") || !strcmp(k, "AttributeError")
        || !strcmp(k, "BufferError") || !strcmp(k, "EOFError")
        || !strcmp(k, "MemoryError") || !strcmp(k, "ReferenceError")
        || !strcmp(k, "SystemError") || !strcmp(k, "TypeError")
        || !strcmp(k, "GenerationError") || !strcmp(k, "ManifestError")
        || !strcmp(k, "SessionNotOwned") || !strcmp(k, "CompressionBusy")
        || !strcmp(k, "CompressionLockHeld") || !strcmp(k, "PetStoreError")
        || !strcmp(k, "ExportTooLarge") || !strcmp(k, "ResumeTooLarge")
        || !strcmp(k, "BillingError") || !strcmp(k, "JSONDecodeError")) return "Exception";
    /* Unknown/custom kinds route through Exception (matches materialization). */
    return "Exception";
}
/* is `kind` a subclass of (or equal to) `target`? */
static bool tms2_exc_is(const char *kind, const char *target) {
    if (!kind || !target) return false;
    const char *cur = kind;
    for (int i = 0; i < 32 && cur; i++) {
        if (!strcmp(cur, target)) return true;
        cur = tms2_exc_parent(cur);
    }
    return false;
}
/* Does a scripted raise match an `except <target>` that derives from Exception?
 * BaseException-only kinds pass through every except-Exception site. */
static bool tms2_catches(const json_t *raise, const char *target) {
    const char *kind = tms2_raise_kind(raise);
    if (!kind) return false;
    if (tms2_exc_is(kind, "BaseException") && !tms2_exc_is(kind, "Exception")) {
        /* BaseException-only: SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError */
        if (strcmp(target, "BaseException") != 0) return false;
    }
    return tms2_exc_is(kind, target);
}

/* -- the one universal seam (fail-closed) -- */
static json_t *tms2_call(tms2_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_t *clean = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (v && v->type == JSON_NULL) continue;   /* null-arg drop */
            json_set(clean, args->c.keys[i], json_copy(v));
        }
    }
    json_free(args);
    json_set(ev, "args", clean);
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = tms2_err_spec("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = tms2_err_spec("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) {
                const char *ty = json_get_str(raise, "type", "RuntimeError");
                const char *msg = json_get_str(raise, "message", "");
                /* KeyError str() == repr(arg); scripted message is str(exc) already.
                 * For KeyError we repr the message to match materialization. */
                if (!strcmp(ty, "KeyError")) {
                    /* ASCII single-arg repr: quote the message. */
                    char q = strchr(msg, '\'') && !strchr(msg, '"') ? '"' : '\'';
                    char rep[512]; size_t k = 0;
                    rep[k++] = q;
                    for (const char *p = msg; *p && k < 500; p++) {
                        if (*p == q || *p == '\\') rep[k++] = '\\', rep[k++] = *p;
                        else rep[k++] = *p;
                    }
                    rep[k++] = q; rep[k] = 0;
                    ret = tms2_err_spec(ty, rep);
                } else {
                    ret = tms2_err_spec(ty, msg);
                }
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

/* -- arg builders -- */
static json_t *tms2_args0(void) { return json_object(); }
static void tms2_aset(json_t *a, const char *k, const json_t *v) { json_set(a, k, v ? json_copy(v) : json_null()); }
static void tms2_asets(json_t *a, const char *k, const char *v) { json_set(a, k, v ? json_string(v) : json_null()); }
static void tms2_asetb(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void tms2_asetn(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }

/* -- real envelope constructors (server.py) -- */
static json_t *tms2_ok(const json_t *rid, json_t *result) {
    json_t *o = json_object();
    json_set_hdr(o, "jsonrpc", "2.0");
    json_set(o, "id", rid ? json_copy(rid) : json_null());
    json_set(o, "result", result ? result : json_null());
    return o;
}
static json_t *tms2_err(const json_t *rid, int code, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "jsonrpc", "2.0");
    json_set(e, "id", rid ? json_copy(rid) : json_null());
    json_t *er = json_object();
    json_set(er, "code", json_number(code));
    json_set_hdr(er, "message", msg ? msg : "");
    json_set(e, "error", er);
    return e;
}

/* -- Python int() for the fixture domain (ints, numeric strings, bools) -- */
/* On success writes *out and returns NULL; on failure returns a ValueError spec. */
static json_t *tms2_pyint(const json_t *v, long long *out) {
    if (!v || v->type == JSON_NULL) return tms2_err_spec("TypeError", "int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return NULL; }
    if (v->type == JSON_NUMBER) { *out = (long long)v->num_val; return NULL; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\v' || *s == '\f' || *s == '\r') s++;
        if (!*s) return tms2_err_spec("ValueError", "invalid literal for int() with base 10: ''");
        char *end = NULL;
        long long n = strtoll(s, &end, 10);
        if (end && *end == '\0') { *out = n; return NULL; }
        char rep[256];
        snprintf(rep, sizeof rep, "invalid literal for int() with base 10: '%s'", s);
        return tms2_err_spec("ValueError", rep);
    }
    char msg[160];
    snprintf(msg, sizeof msg, "int() argument must be a string, a bytes-like object or a real number, not '%s'", tms2_type_name(v));
    return tms2_err_spec("TypeError", msg);
}

/* str(params.get(key) or "") -- NO strip (used by _save_via_compute_host). */
static char *tms2_str_or_empty(const json_t *params, const char *key) {
    const json_t *v = params ? json_obj_get(params, key) : NULL;
    if (!v || !tms2_truthy(v)) return tms2_strdup("");
    return tms2_pystr(v);
}

/* _str_param(params, key, default="") = str(params.get(key) or "").strip() or default */
static char *tms2_str_param(const json_t *params, const char *key) {
    const json_t *v = params ? json_obj_get(params, key) : NULL;
    if (!v || !tms2_truthy(v)) return tms2_strdup("");
    char *s = tms2_pystr(v);
    char *t = tms2_strip(s ? s : "");
    free(s);
    return t ? t : tms2_strdup("");
}

/* -- standard base64 (ASCII text -> bytes -> standard_b64encode) -- */
static char *tms2_b64(const char *data) {
    static const char *T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t n = data ? strlen(data) : 0;
    size_t cap = ((n + 2) / 3) * 4 + 1;
    char *o = malloc(cap);
    if (!o) return NULL;
    size_t k = 0;
    size_t i = 0;
    while (i + 3 <= n) {
        unsigned a = (unsigned char)data[i], b = (unsigned char)data[i+1], c = (unsigned char)data[i+2];
        o[k++] = T[a >> 2];
        o[k++] = T[((a & 3) << 4) | (b >> 4)];
        o[k++] = T[((b & 15) << 2) | (c >> 6)];
        o[k++] = T[c & 63];
        i += 3;
    }
    size_t rem = n - i;
    if (rem == 1) {
        unsigned a = (unsigned char)data[i];
        o[k++] = T[a >> 2]; o[k++] = T[(a & 3) << 4]; o[k++] = '='; o[k++] = '=';
    } else if (rem == 2) {
        unsigned a = (unsigned char)data[i], b = (unsigned char)data[i+1];
        o[k++] = T[a >> 2]; o[k++] = T[((a & 3) << 4) | (b >> 4)]; o[k++] = T[(b & 15) << 2]; o[k++] = '=';
    }
    o[k] = '\0';
    return o;
}

/* ===================================================================== */
/* PORTED FUNCTIONS (all verified UNCLAIMED by CRLF-aware PoP grep)        */
/* ===================================================================== */

/* PoP: tms2_b64_2 @ tui_gateway/methods_session.py:_b64 */
static json_t *tms2_b64_2(tms2_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const json_t *data = json_obj_get(args, "data");
    const char *s = tms2_str(data);
    char *enc = tms2_b64(s ? s : "");
    json_t *out = json_string(enc ? enc : "");
    free(enc);
    return out;
}

/* PoP: tms2_terminal_resize @ tui_gateway/methods_session.py:_ */
static json_t *tms2_terminal_resize(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    const json_t *v = params ? json_obj_get(params, "cols") : NULL;
    if (!v) v = json_number(80);   /* params.get("cols", 80) */
    long long cols = 0;
    json_t *bad = tms2_pyint(v, &cols);
    if (bad) { json_free(bad); return tms2_err_spec("ValueError", "invalid cols"); }
    json_set(ctx->state, "cols", json_number((double)cols));
    json_t *payload = json_object();
    json_set(payload, "cols", json_number((double)cols));
    return tms2_ok(rid, payload);
}

/* PoP: tms2_delegation_status @ tui_gateway/methods_session.py:_ */
static json_t *tms2_delegation_status(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    json_t *p = json_object();
    json_set(p, "active", tms2_call(ctx, "list_active_subagents", tms2_args0()));
    json_set(p, "paused", tms2_call(ctx, "is_spawn_paused", tms2_args0()));
    json_set(p, "max_spawn_depth", tms2_call(ctx, "_get_max_spawn_depth", tms2_args0()));
    json_set(p, "max_concurrent_children", tms2_call(ctx, "_get_max_concurrent_children", tms2_args0()));
    return tms2_ok(rid, p);
}

/* PoP: tms2_delegation_pause @ tui_gateway/methods_session.py:_ */
static json_t *tms2_delegation_pause(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    bool paused = true;
    if (params && json_has(params, "paused"))
        paused = tms2_truthy(json_obj_get(params, "paused"));
    json_t *a = tms2_args0();
    tms2_asetb(a, "paused", paused);
    json_t *r = tms2_call(ctx, "set_spawn_paused", a);
    json_t *p = json_object();
    json_set(p, "paused", r);
    return tms2_ok(rid, p);
}

/* PoP: tms2_events_stats @ tui_gateway/methods_session.py:_ */
static json_t *tms2_events_stats(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    json_t *r = tms2_call(ctx, "replay_stats", tms2_args0());
    return tms2_ok(rid, r);
}

/* PoP: tms2_events_since @ tui_gateway/methods_session.py:_ */
static json_t *tms2_events_since(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    const json_t *sidv = params ? json_obj_get(params, "session_id") : NULL;
    char *sid = tms2_pystr(sidv && tms2_truthy(sidv) ? sidv : json_null());
    if (!strcmp(sid, "None")) { free(sid); sid = tms2_strdup(""); }
    const json_t *ls = params ? json_obj_get(params, "last_seen") : NULL;
    if (!ls) ls = json_number(0);
    long long last_seen = 0;
    json_t *bad = tms2_pyint(ls, &last_seen);
    if (bad) { json_free(bad); free(sid);
        return tms2_err(rid, -32602, "invalid params: last_seen must be an integer"); }

    json_t *a = tms2_args0();
    tms2_asets(a, "sid", sid); tms2_asetn(a, "last_seen", (double)last_seen);
    json_t *frames = tms2_call(ctx, "events_since", a);
    a = tms2_args0(); tms2_asets(a, "sid", sid);
    json_t *latest = tms2_call(ctx, "latest_seq", a);
    a = tms2_args0(); tms2_asets(a, "sid", sid); tms2_asetn(a, "last_seen", (double)last_seen);
    json_t *trunc = tms2_call(ctx, "is_truncated", a);
    json_t *epoch = tms2_call(ctx, "replay_epoch", tms2_args0());
    a = tms2_args0(); tms2_asets(a, "sid", sid);
    json_t *open = tms2_call(ctx, "open_requests", a);
    size_t count = (frames && frames->type == JSON_ARRAY) ? json_len(frames) : 0;

    json_t *p = json_object();
    json_set(p, "events", frames);
    json_set(p, "latest_seq", latest);
    json_set(p, "truncated", trunc);
    json_set(p, "count", json_number((double)count));
    json_set(p, "epoch", epoch);
    json_set(p, "open_requests", open);
    free(sid);
    return tms2_ok(rid, p);
}

/* PoP: tms2_pet_disable @ tui_gateway/methods_session.py:_ */
static json_t *tms2_pet_disable(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    json_t *a = tms2_args0();
    tms2_asetb(a, "enabled", false);
    tms2_call(ctx, "set_enabled", a);   /* _set_enabled(False) */
    json_t *p = json_object();
    json_set(p, "ok", json_bool(true));
    return tms2_ok(rid, p);
}

/* PoP: tms2_pet_scale @ tui_gateway/methods_session.py:_ */
static json_t *tms2_pet_scale(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    json_t *a = tms2_args0();
    json_set(a, "scale", json_obj_get(params, "scale") ? json_copy(json_obj_get(params, "scale")) : json_null());
    json_t *r = tms2_call(ctx, "set_pet_scale", a);   /* returns [scale, err] */
    json_t *scale = (r && r->type == JSON_ARRAY && json_len(r) > 0) ? json_get(r, 0) : json_null();
    json_t *err = (r && r->type == JSON_ARRAY && json_len(r) > 1) ? json_get(r, 1) : NULL;
    if (err && tms2_truthy(err)) {
        char *msg = tms2_pystr(err);
        json_t *e = tms2_err(rid, 4004, msg ? msg : "");
        free(msg);
        return e;
    }
    json_t *p = json_object();
    json_set(p, "ok", json_bool(true));
    json_set(p, "scale", scale ? json_copy(scale) : json_null());
    return tms2_ok(rid, p);
}

/* PoP: tms2_pet_cancel @ tui_gateway/methods_session.py:_ */
static json_t *tms2_pet_cancel(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    char *token = tms2_str_param(params, "token");
    if (token && token[0]) {
        json_t *a = tms2_args0();
        tms2_asets(a, "token", token);
        tms2_call(ctx, "pet_cancel_request", a);
    }
    free(token);
    json_t *p = json_object();
    json_set(p, "ok", json_bool(true));
    return tms2_ok(rid, p);
}

/* PoP: tms2_pet_gen_status @ tui_gateway/methods_session.py:_ */
static json_t *tms2_pet_gen_status(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    bool available = true;
    json_t *a = tms2_args0();
    tms2_asetb(a, "require_references", true);
    json_t *r1 = tms2_call(ctx, "resolve_provider", a);
    if (tms2_is_raise(r1) && tms2_catches(r1, "GenerationError")) available = false;
    json_t *r2 = tms2_call(ctx, "list_sprite_providers", tms2_args0());
    json_t *providers = (r2 && r2->type == JSON_ARRAY && !(tms2_is_raise(r2))) ? json_copy(r2) : json_array();
    if (tms2_is_raise(r2)) { json_free(providers); providers = json_array(); }
    json_t *p = json_object();
    json_set(p, "available", json_bool(available));
    json_set(p, "providers", providers);
    return tms2_ok(rid, p);
}

/* PoP: tms2_session_close @ tui_gateway/methods_session.py:_ */
static json_t *tms2_session_close(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    const json_t *sidv = params ? json_obj_get(params, "session_id") : NULL;
    json_t *a = tms2_args0();
    json_set(a, "session_id", sidv ? json_copy(sidv) : json_string(""));
    json_t *session = tms2_call(ctx, "pop_session_by_id", a);
    a = tms2_args0();
    json_set(a, "session", session ? json_copy(session) : json_null());
    tms2_asets(a, "end_reason", "tui_close");
    json_t *closed = tms2_call(ctx, "teardown_popped_session", a);
    json_t *msgs = json_array();
    if (session && session->type == JSON_OBJECT) {
        const json_t *em = json_obj_get(session, "_end_msgs");
        if (em && em->type == JSON_ARRAY) msgs = json_copy(em);
        else if (em && em->type == JSON_STRING) {   /* list("text") -> chars */
            for (const char *p = tms2_str(em); p && *p; p++) {
                char c[2] = { *p, '\0' };
                json_append(msgs, json_string(c));
            }
        } else if (em && em->type == JSON_OBJECT) {  /* list(dict) -> keys */
            for (size_t i = 0; i < em->c.count; i++)
                json_append(msgs, json_string(em->c.keys[i] ? em->c.keys[i] : ""));
        }
    }
    json_t *p = json_object();
    json_set(p, "closed", closed);
    json_set(p, "messages", msgs);
    return tms2_ok(rid, p);
}

/* PoP: tms2_session_branch @ tui_gateway/methods_session.py:_ */
static json_t *tms2_session_branch(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    const json_t *session = json_obj_get(args, "session");
    json_t *a = tms2_args0();
    json_set(a, "rid", rid ? json_copy(rid) : json_null());
    json_set(a, "params", json_copy(params));
    json_set(a, "session", json_copy(session));
    tms2_asetb(a, "omit_messages", false);
    return tms2_call(ctx, "branch_live", a);
}

/* PoP: tms2_session_branch_whole @ tui_gateway/methods_session.py:_ */
static json_t *tms2_session_branch_whole(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    const json_t *session = json_obj_get(args, "session");
    json_t *a = tms2_args0();
    json_set(a, "rid", rid ? json_copy(rid) : json_null());
    json_set(a, "params", json_copy(params));
    json_set(a, "session", json_copy(session));
    tms2_asetb(a, "omit_messages", true);
    return tms2_call(ctx, "branch_live", a);
}

/* PoP: tms2_save_via_compute_host @ tui_gateway/methods_session.py:_save_via_compute_host */
static json_t *tms2_save_via_compute_host(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    char *sid = tms2_str_or_empty(params, "session_id");
    json_t *a = tms2_args0();
    tms2_asets(a, "session_id", sid);
    tms2_asets(a, "route_name", "session.save");
    tms2_asetb(a, "wait", true);
    json_t *ack = tms2_call(ctx, "send_compute_host_control", a);
    free(sid);
    if (tms2_is_raise(ack)) {
        char buf[512];
        snprintf(buf, sizeof buf, "compute-host session save failed: %s", tms2_raise_msg(ack));
        return tms2_err(rid, 5011, buf);
    }
    const json_t *codev = ack ? json_obj_get(ack, "code") : NULL;
    double code = (codev && tms2_truthy(codev) && (codev->type == JSON_NUMBER || codev->type == JSON_STRING))
                  ? (codev->type == JSON_NUMBER ? codev->num_val : atof(tms2_str(codev))) : 5011.0;
    json_t *ca = tms2_args0();
    json_set(ca, "rid", rid ? json_copy(rid) : json_null());
    json_set(ca, "ack", ack ? json_copy(ack) : json_null());
    tms2_asetn(ca, "code", code);
    tms2_asets(ca, "message", "compute-host session save failed");
    json_t *resp = tms2_call(ctx, "compute_host_ack_error", ca);
    if (resp && !tms2_is_raise(resp) && resp->type != JSON_NULL) {
        /* _compute_host_ack_error returns None or an error envelope. */
        if (json_obj_get(resp, "error") || json_obj_get(resp, "__raise__") || json_obj_get(resp, "jsonrpc"))
            return resp;
    }
    json_t *result = ack ? json_obj_get(ack, "result") : NULL;
    if (!result || result->type != JSON_OBJECT)
        return tms2_err(rid, 5011, "compute-host session save returned an invalid response");
    return tms2_ok(rid, json_copy(result));
}

/* PoP: tms2_spawn_tree_load @ tui_gateway/methods_session.py:_ */
static json_t *tms2_spawn_tree_load(tms2_ctx_t *ctx, const json_t *args) {
    const json_t *rid = json_obj_get(args, "rid");
    const json_t *params = json_obj_get(args, "params");
    char *raw = tms2_str_param(params, "path");
    if (!raw || !raw[0]) { free(raw); return tms2_err(rid, 4000, "path required"); }
    json_t *a = tms2_args0();
    tms2_asets(a, "path", raw);
    json_t *resolved = tms2_call(ctx, "path_resolve", a);   /* str or raise */
    free(raw);
    if (tms2_is_raise(resolved)) {
        char buf[512];
        snprintf(buf, sizeof buf, "path outside spawn-trees root: %s", tms2_raise_msg(resolved));
        return tms2_err(rid, 4030, buf);
    }
    char *rp = tms2_pystr(resolved);
    a = tms2_args0();
    tms2_asets(a, "path", rp);
    json_t *payload = tms2_call(ctx, "path_read_json", a);  /* dict or raise */
    free(rp);
    if (tms2_is_raise(payload)) {
        char buf[512];
        snprintf(buf, sizeof buf, "spawn_tree.load failed: %s", tms2_raise_msg(payload));
        return tms2_err(rid, 5000, buf);
    }
    if (!payload || payload->type != JSON_OBJECT)
        return tms2_err(rid, 5000, "spawn_tree.load failed: snapshot is not a JSON object");
    return tms2_ok(rid, json_copy(payload));
}
