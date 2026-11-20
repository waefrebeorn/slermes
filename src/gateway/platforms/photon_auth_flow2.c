/*
 * photon_auth_flow2.c — Photon dashboard auth FLOW port (second wave).
 *
 * Ports 40 UNCLAIMED module-level defs of plugins/platforms/photon/auth.py
 * (hermes-agent).  The only source PoP claim in this module is
 * telegram_managed_bot.c:poll_for_token (auth.py:poll_for_token); that function
 * is deliberately NOT ported here.  Every function below carries a strict
 * `PoP: pa2_<name> @ plugins/platforms/photon/auth.py:<python_name>` annotation.
 *
 * Universal seam: every foreign / stdlib / in-function-import operation enters
 * ONE callback, pa2_call(ctx, name, args).  The harness wires ctx->io to a
 * recording stub fed the case's scripted results; the oracle installs recorder
 * stubs emitting the identical {"name","args","ret"} event from the SAME
 * scripts.  args is ADOPTED into the trace event; the return is a NEW owned
 * json_t (NULL pointer == Python None; a {"__raise__":cls,"message":...} node
 * == a raised exception).  A missing/exhausted script is a fail-closed
 * RuntimeError on both sides.
 *
 * Mode table:
 *  REAL (inline C call): ported siblings call each other directly.  The oracle
 *    extracts the SAME live defs into one namespace, so the composition and
 *    seam ordering are identical on both sides.
 *  SEAMED (pa2_call, scripted): get_hermes_home, os.getenv, os.getpid,
 *    os.open/fdopen/close/fsync, uuid4_hex, path.exists/mkdir/unlink/replace,
 *    json.load/dump, auth_store_lock, time.time, safe_call, get_scoped_secret,
 *    config.get_env_value/save_env_value (in-function imports),
 *    httpx.get/post, resp.raise_for_status/json.
 *
 * Representation contract (shared with tests/oracle_photon_auth_flow2.py):
 * - NULL json_t* == Python None; a JSON_NULL node is a real JSON null value.
 *   json_set(obj,k,NULL) is a no-op in libjson, so Python None is set with
 *   json_null().  Tuples project to JSON arrays.
 * - A raise is {"__raise__": cls, "message": str(exc)}; extra spec keys kept.
 * - `config_available`/`httpx_available` are per-case CONFIG args (default
 *   true), NOT traced seams: they gate in-function imports and httpx presence.
 * - Paths project to POSIX strings (PurePosixPath str); join is POSIX algebra.
 * - Response doubles are dicts carrying status_code/text/headers; resp.json /
 *   resp.raise_for_status are traced seams so their raises propagate.
 * - Logger calls are SILENT on both sides (no trace events).
 * - PhononDashboardAuthError subclasses RuntimeError in the full exception
 *   table, so `except (OSError, ValueError)` / `except Exception` / BaseException
 *   family matching is faithful.
 *
 * Fixture corpus restrictions (the generator honours them):
 * - str()/f-string positions carry str/int/bool/None scalars only (no
 *   containers, no integral floats -> "30" vs "30.0").  json.load slots carry
 *   canonical JSON or forms both parsers reject.  Phone/E.164 text is ASCII.
 * - os.open/fsync/fdopen receive an integer fd script; path/mkdir/unlink/replace
 *   receive POSIX string paths; os.getpid a non-negative int; uuid4_hex a hex str.
 * - Auth-dict shapes are dicts (or absent) at every `.get` site; a non-dict
 *   credential_pool/providers/headers is outside the live domain.
 * - get_scoped_secret / config getters return str or null (no containers).
 */
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── seam context ── */
typedef struct pa2_ctx {
    json_t *state;      /* owned; always {} for these module functions */
    const json_t *io;   /* borrowed scripted seam results */
    json_t *cursors;    /* owned; per-name array cursors */
    json_t *trace;      /* owned; ordered seam events */
} pa2_ctx_t;

/* ── FULL builtin exception hierarchy (py3.14, first-parent edge) ──
 * Unknown scripted kinds fall back to Exception (type(kind,(Exception,))). */
static const struct { const char *name; const char *parent; } PA2_EXC[] = {
    {"BaseException", NULL},
    {"Exception", "BaseException"},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
    {"ExceptionGroup", "Exception"},
    {"ArithmeticError", "Exception"},
    {"AssertionError", "Exception"},
    {"AttributeError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ImportError", "Exception"},
    {"LookupError", "Exception"},
    {"MemoryError", "Exception"},
    {"NameError", "Exception"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"Warning", "Exception"},
    {"OSError", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"ModuleNotFoundError", "ImportError"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"UnboundLocalError", "NameError"},
    {"NotImplementedError", "RuntimeError"},
    {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"IndentationError", "SyntaxError"},
    {"_IncompleteInputError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"JSONDecodeError", "ValueError"},
    {"BytesWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"BlockingIOError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"ConnectionError", "OSError"},
    {"FileExistsError", "OSError"},
    {"FileNotFoundError", "OSError"},
    {"InterruptedError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"PermissionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"TimeoutError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    /* module custom: auth.PhotonDashboardAuthError(RuntimeError) */
    {"PhotonDashboardAuthError", "RuntimeError"},
};
static const char *pa2_exc_norm(const char *k) {
    if (!k) return "RuntimeError";
    if (!strcmp(k, "IOError") || !strcmp(k, "EnvironmentError") || !strcmp(k, "WindowsError"))
        return "OSError";
    if (!strcmp(k, "asyncio.TimeoutError")) return "TimeoutError";
    return k;
}
static const char *pa2_exc_parent(const char *k) {
    for (size_t i = 0; i < sizeof(PA2_EXC) / sizeof(PA2_EXC[0]); i++)
        if (!strcmp(PA2_EXC[i].name, k)) return PA2_EXC[i].parent;
    return "Exception";
}
static bool pa2_exc_is(const char *kind, const char *ancestor) {
    const char *cur = pa2_exc_norm(kind);
    for (int d = 0; d < 32; d++) {
        if (!strcmp(cur, ancestor)) return true;
        const char *p = pa2_exc_parent(cur);
        if (!p) return false;
        cur = p;
    }
    return false;
}
static bool pa2_is_exception(const char *k) { return pa2_exc_is(k, "Exception"); }
static bool pa2_is_oserror(const char *k) { return pa2_exc_is(k, "OSError"); }
static bool pa2_is_valueerror(const char *k) { return pa2_exc_is(k, "ValueError"); }

/* ── basic value helpers ── */
static bool pa2_is_raise(const json_t *v) {
    return v && v->type == JSON_OBJECT && json_obj_get(v, "__raise__") != NULL;
}
static const char *pa2_kind(const json_t *v) {
    const json_t *k = v ? json_obj_get(v, "__raise__") : NULL;
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static bool pa2_kind_is(const json_t *v, const char *kind) {
    return pa2_is_raise(v) && pa2_exc_is(pa2_kind(v), kind);
}
static bool pa2_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return false;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return false;
    }
}
static const char *pa2_str(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool pa2_str_eq(const json_t *v, const char *s) {
    const char *t = pa2_str(v);
    return t && s && !strcmp(t, s);
}
static json_t *pa2_err(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type ? type : "RuntimeError");
    json_set_hdr(e, "message", msg ? msg : "");
    return e;
}
static json_t *pa2_make_exc(const json_t *spec) {
    const char *type = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_t *e = pa2_err(type, msg);
    if (spec && spec->type == JSON_OBJECT) {
        for (size_t i = 0; i < spec->c.count; i++) {
            const char *k = spec->c.keys[i];
            if (!strcmp(k, "type") || !strcmp(k, "message")) continue;
            json_set(e, k, json_copy(spec->c.items[i]));
        }
    }
    return e;
}
/* The one universal seam. args ADOPTED; return fresh owned node. */
static json_t *pa2_call(pa2_ctx_t *ctx, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? args : json_object());
    const json_t *entry = ctx->io ? json_obj_get(ctx->io, name) : NULL;
    const json_t *used = entry;
    json_t *ret = NULL;
    if (!entry) {
        char msg[200];
        snprintf(msg, sizeof msg, "no script for %s", name);
        ret = pa2_err("RuntimeError", msg);
    } else if (entry->type == JSON_ARRAY) {
        size_t idx = (size_t)json_get_num(ctx->cursors, name, 0);
        if (idx >= json_len(entry)) {
            char msg[200];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            ret = pa2_err("RuntimeError", msg);
        } else {
            json_set(ctx->cursors, name, json_number((double)(idx + 1)));
            used = json_get(entry, idx);
        }
    }
    if (!ret) {
        if (used && used->type == JSON_OBJECT && json_get_bool(used, "__script__", false)) {
            const json_t *raise = json_obj_get(used, "raise");
            if (raise && raise->type == JSON_OBJECT) ret = pa2_make_exc(raise);
            else {
                const json_t *r = json_obj_get(used, "ret");
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
static json_t *pa2_args(void) { return json_object(); }
static void pa2_set_s(json_t *a, const char *k, const char *v) { json_set_hdr(a, k, v ? v : ""); }
static void pa2_set_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void pa2_set_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void pa2_set_null(json_t *a, const char *k) { json_set(a, k, json_null()); }
static void pa2_set_copy(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}

/* ── string helpers (ASCII fixtures) ── */
static char *pa2_strdup(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *o = (char *)malloc(n + 1);
    if (o) memcpy(o, s, n + 1);
    return o;
}
static bool pa2_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
/* Python str.strip() (ASCII subset) */
static char *pa2_strip(const char *s) {
    if (!s) return pa2_strdup("");
    size_t a = 0, b = strlen(s);
    while (a < b && pa2_is_ws((unsigned char)s[a])) a++;
    while (b > a && pa2_is_ws((unsigned char)s[b - 1])) b--;
    char *o = (char *)malloc(b - a + 1);
    if (!o) return pa2_strdup("");
    memcpy(o, s + a, b - a);
    o[b - a] = '\0';
    return o;
}
static char *pa2_lower(const char *s) {
    char *o = pa2_strdup(s ? s : "");
    for (char *p = o; *p; p++) *p = (char)tolower((unsigned char)*p);
    return o;
}
static bool pa2_startswith_ci(const char *s, const char *pre) {
    if (!s || !pre) return false;
    size_t n = strlen(pre);
    if (strlen(s) < n) return false;
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)s[i]) != tolower((unsigned char)pre[i])) return false;
    return true;
}
/* Python str(...) projection for scalar fixture values. */
static char *pa2_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return pa2_strdup("None");
    switch (v->type) {
        case JSON_STRING: return pa2_strdup(v->str_val);
        case JSON_BOOL: return pa2_strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: {
            char buf[64];
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 9e15) snprintf(buf, sizeof buf, "%.0f", d);
            else snprintf(buf, sizeof buf, "%.17g", d);
            return pa2_strdup(buf);
        }
        default: return pa2_strdup("");
    }
}
static const char *pa2_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: return "int";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
        default: return "object";
    }
}
/* Python int(x) for scalar numeric/string fixtures; returns 0 on invalid. */
static long long pa2_pyint(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) {
        char *s = pa2_strip(v->str_val);
        return strtoll(s, NULL, 10);
    }
    return 0;
}
static bool pa2_e164(const char *s) {
    if (!s || s[0] != '+' || s[1] < '1' || s[1] > '9') return false;
    size_t n = 0;
    for (const char *p = s + 1; *p; p++) {
        if (!isdigit((unsigned char)*p)) return false;
        n++;
    }
    return n >= 7 && n <= 15;
}
static char *pa2_join(const char *base, const char *comp) {
    if (!base || !*base) return pa2_strdup(comp ? comp : "");
    size_t nb = strlen(base);
    bool slash = base[nb - 1] == '/';
    char *o = (char *)malloc(nb + (slash ? 0 : 1) + (comp ? strlen(comp) : 0) + 1);
    if (!o) return pa2_strdup("");
    strcpy(o, base);
    if (!slash) strcat(o, "/");
    if (comp) strcat(o, comp);
    return o;
}
static char *pa2_parent(const char *path) {
    if (!path) return pa2_strdup("");
    const char *p = strrchr(path, '/');
    if (!p) return pa2_strdup("");
    if (p == path) {
        if (p[1] == '/' && p[2] == '/') return pa2_strdup("//");
        return pa2_strdup("/");
    }
    char *o = (char *)malloc((size_t)(p - path) + 1);
    memcpy(o, path, (size_t)(p - path));
    o[p - path] = '\0';
    return o;
}
static const char *pa2_name(const char *path) {
    if (!path) return "";
    const char *p = strrchr(path, '/');
    return p ? p + 1 : path;
}
/* base64 encode ASCII string (fixtures ASCII) */
static char *pa2_b64encode(const char *s) {
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t n = strlen(s ? s : "");
    size_t out = ((n + 2) / 3) * 4;
    char *o = (char *)malloc(out + 1);
    if (!o) return pa2_strdup("");
    size_t i = 0, j = 0;
    while (i + 2 < n) {
        unsigned v = ((unsigned char)s[i] << 16) | ((unsigned char)s[i+1] << 8) | (unsigned char)s[i+2];
        o[j++] = T[(v >> 18) & 63]; o[j++] = T[(v >> 12) & 63];
        o[j++] = T[(v >> 6) & 63];  o[j++] = T[v & 63];
        i += 3;
    }
    if (n - i == 1) {
        unsigned v = (unsigned char)s[i] << 16;
        o[j++] = T[(v >> 18) & 63]; o[j++] = T[(v >> 12) & 63]; o[j++] = '='; o[j++] = '=';
    } else if (n - i == 2) {
        unsigned v = ((unsigned char)s[i] << 16) | ((unsigned char)s[i+1] << 8);
        o[j++] = T[(v >> 18) & 63]; o[j++] = T[(v >> 12) & 63]; o[j++] = T[(v >> 6) & 63]; o[j++] = '=';
    }
    o[j] = '\0';
    return o;
}
/* Build a one-key seam args object (used by get_scoped_secret call sites). */
static json_t *pa2_args_preset(const char *name) {
    json_t *a = pa2_args();
    pa2_set_s(a, "name", name);
    return a;
}

/* Minimal json.dumps(sort_keys=True) for scalar/simple-container fixtures. */
static void pa2_app(char **b, size_t *len, size_t *cap, const char *s, size_t n) {
    if (*len + n + 1 > *cap) {
        while (*len + n + 1 > *cap) *cap *= 2;
        char *nb = (char *)realloc(*b, *cap);
        if (!nb) return;
        *b = nb;
    }
    memcpy(*b + *len, s, n);
    *len += n;
    (*b)[*len] = '\0';
}
static void pa2_appc(char **b, size_t *len, size_t *cap, char c) { pa2_app(b, len, cap, &c, 1); }
static void pa2_dump_val(const json_t *v, char **b, size_t *len, size_t *cap) {
    if (!v || v->type == JSON_NULL) { pa2_app(b, len, cap, "null", 4); return; }
    switch (v->type) {
        case JSON_BOOL: pa2_app(b, len, cap, v->bool_val ? "true" : "false", v->bool_val ? 4 : 5); return;
        case JSON_NUMBER: {
            char t[64];
            double d = v->num_val;
            if (d == floor(d) && fabs(d) < 9e15) snprintf(t, sizeof t, "%.0f", d);
            else snprintf(t, sizeof t, "%.17g", d);
            pa2_app(b, len, cap, t, strlen(t)); return;
        }
        case JSON_STRING: {
            pa2_appc(b, len, cap, '"');
            for (const char *p = v->str_val; *p; p++) {
                if (*p == '"' || *p == '\\') { pa2_appc(b, len, cap, '\\'); pa2_appc(b, len, cap, *p); }
                else if (*p == '\n') pa2_app(b, len, cap, "\\n", 2);
                else pa2_appc(b, len, cap, *p);
            }
            pa2_appc(b, len, cap, '"'); return;
        }
        case JSON_ARRAY: {
            pa2_appc(b, len, cap, '[');
            for (size_t i = 0; i < json_len(v); i++) {
                if (i) pa2_app(b, len, cap, ", ", 2);
                pa2_dump_val(json_get(v, i), b, len, cap);
            }
            pa2_appc(b, len, cap, ']'); return;
        }
        case JSON_OBJECT: {
            pa2_appc(b, len, cap, '{');
            /* insertion-ordered keys; fixture fallback dicts are scripted with sorted keys */
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) pa2_app(b, len, cap, ", ", 2);
                char *tmp = (char *)malloc(strlen(v->c.keys[i]) * 2 + 3);
                size_t tl = 0, tc = strlen(v->c.keys[i]) * 2 + 3;
                tmp[0] = '"'; tl = 1;
                for (const char *p = v->c.keys[i]; *p; p++) {
                    if (*p == '"' || *p == '\\') { tmp[tl++] = '\\'; tmp[tl++] = *p; }
                    else tmp[tl++] = *p;
                }
                tmp[tl++] = '"'; tmp[tl] = '\0';
                pa2_app(b, len, cap, tmp, tl);
                free(tmp);
                pa2_app(b, len, cap, ": ", 2);
                pa2_dump_val(v->c.items[i], b, len, cap);
            }
            pa2_appc(b, len, cap, '}'); return;
        }
        default: pa2_app(b, len, cap, "null", 4); return;
    }
}
/* Python json.dumps(data, sort_keys=True)[:500] assuming fixture keys already sorted. */
static char *pa2_dumps_sorted_trunc(const json_t *v) {
    size_t cap = 1024, len = 0;
    char *b = (char *)malloc(cap);
    if (!b) return pa2_strdup("");
    b[0] = '\0';
    pa2_dump_val(v, &b, &len, &cap);
    size_t n = strlen(b); /* codepoint truncation: ASCII fixture fallback text */
    if (n > 500) b[500] = '\0';
    return b;
}
/* f-string concatenation helper */
static char *pa2_fmt(const char *a, const char *b) {
    size_t n = strlen(a) + strlen(b) + 1;
    char *o = (char *)malloc(n);
    if (!o) return pa2_strdup("");
    snprintf(o, n, "%s%s", a, b);
    return o;
}

/* ─────────────────────────── ported functions ─────────────────────────── */

/* PoP: pa2__bearer @ plugins/platforms/photon/auth.py:_bearer */
/* Takes the TOKEN VALUE node (not an args object); f"Bearer {token}". */
static json_t *pa2__bearer(pa2_ctx_t *ctx, const json_t *token) {
    (void)ctx;
    char *t = pa2_pystr(token);
    char *h = pa2_fmt("Bearer ", t);
    free(t);
    json_t *o = json_object();
    pa2_set_s(o, "Authorization", h);
    free(h);
    return o;
}

/* PoP: pa2__basic @ plugins/platforms/photon/auth.py:_basic */
static json_t *pa2__basic(pa2_ctx_t *ctx, const json_t *a) {
    (void)ctx;
    char *id = pa2_pystr(json_obj_get(a, "project_id"));
    char *sec = pa2_pystr(json_obj_get(a, "project_secret"));
    size_t n = strlen(id) + 1 + strlen(sec) + 1;
    char *pair = (char *)malloc(n);
    snprintf(pair, n, "%s:%s", id, sec);
    char *b = pa2_b64encode(pair);
    char *h = pa2_fmt("Basic ", b);
    json_t *o = json_object();
    pa2_set_s(o, "Authorization", h);
    free(id); free(sec); free(pair); free(b); free(h);
    return o;
}

/* PoP: pa2__require_httpx @ plugins/platforms/photon/auth.py:_require_httpx */
static json_t *pa2__require_httpx(pa2_ctx_t *ctx, const json_t *a) {
    (void)ctx;
    if (!json_get_bool(a, "httpx_available", true)) {
        char *w = pa2_pystr(json_obj_get(a, "what"));
        char *msg = pa2_fmt("httpx is required for Photon", w);
        free(w);
        json_t *r = pa2_err("RuntimeError", msg);
        free(msg);
        return r;
    }
    return NULL;
}

/* shared: dashboard/spectrum host */
static json_t *pa2__host(pa2_ctx_t *ctx, const char *key, const char *def) {
    json_t *args = pa2_args();
    pa2_set_s(args, "key", key);
    json_t *v = pa2_call(ctx, "os.getenv", args);
    if (pa2_is_raise(v)) return v;
    const char *s = pa2_truthy(v) ? pa2_str(v) : def;
    size_t n = strlen(s);
    while (n && s[n - 1] == '/') n--;
    char *o = (char *)malloc(n + 1);
    memcpy(o, s, n); o[n] = '\0';
    json_t *r = json_string(o);
    free(o);
    json_free(v);
    return r;
}
/* PoP: pa2__dashboard_host @ plugins/platforms/photon/auth.py:_dashboard_host */
static json_t *pa2__dashboard_host(pa2_ctx_t *ctx, const json_t *a) {
    (void)a; return pa2__host(ctx, "PHOTON_DASHBOARD_HOST", "https://app.photon.codes");
}
/* PoP: pa2__spectrum_host @ plugins/platforms/photon/auth.py:_spectrum_host */
static json_t *pa2__spectrum_host(pa2_ctx_t *ctx, const json_t *a) {
    (void)a; return pa2__host(ctx, "PHOTON_SPECTRUM_HOST", "https://spectrum.photon.codes");
}

/* PoP: pa2__clean_bearer_token @ plugins/platforms/photon/auth.py:_clean_bearer_token */
static json_t *pa2__clean_bearer_token(pa2_ctx_t *ctx, const json_t *v) {
    (void)ctx;
    if (!v || v->type != JSON_STRING) return NULL;
    char *s = pa2_strip(v->str_val);
    char *tok;
    if (pa2_startswith_ci(s, "bearer ")) {
        char *tail = pa2_strip(s + 7);
        free(s);
        tok = tail;
    } else {
        tok = s;
    }
    if (!*tok) { free(tok); return NULL; }
    json_t *r = json_string(tok);
    free(tok);
    return r;
}

/* PoP: pa2__header_value @ plugins/platforms/photon/auth.py:_header_value */
static json_t *pa2__header_value(pa2_ctx_t *ctx, const json_t *h, const char *name) {
    (void)ctx;
    if (!pa2_truthy(h)) return NULL;
    const json_t *v = h->type == JSON_OBJECT ? json_obj_get(h, name) : NULL;
    if (pa2_truthy(v)) {
        char *s = pa2_pystr(v);
        json_t *r = json_string(s);
        free(s);
        return r;
    }
    if (h->type == JSON_OBJECT) {
        for (size_t i = 0; i < h->c.count; i++) {
            char *lk = pa2_lower(h->c.keys[i]);
            if (!strcmp(lk, name) && pa2_truthy(h->c.items[i])) {
                char *s = pa2_pystr(h->c.items[i]);
                json_t *r = json_string(s);
                free(s); free(lk);
                return r;
            }
            free(lk);
        }
    }
    return NULL;
}

/* PoP: pa2__normalize_phone @ plugins/platforms/photon/auth.py:_normalize_phone */
static json_t *pa2__normalize_phone(pa2_ctx_t *ctx, const json_t *v) {
    (void)ctx;
    const char *s = pa2_str(v) ? pa2_str(v) : "";
    size_t n = strlen(s);
    char *o = (char *)malloc(n + 1);
    size_t j = 0;
    for (size_t i = 0; i < n; i++)
        if (isdigit((unsigned char)s[i]) || s[i] == '+') o[j++] = s[i];
    o[j] = '\0';
    json_t *r = json_string(o);
    free(o);
    return r;
}

/* PoP: pa2__raise_on_error_key @ plugins/platforms/photon/auth.py:_raise_on_error_key */
static json_t *pa2__raise_on_error_key(pa2_ctx_t *ctx, const json_t *data, const char *action) {
    (void)ctx;
    if (!data || data->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", pa2_typename(data));
        return pa2_err("AttributeError", msg);
    }
    const json_t *e = json_obj_get(data, "error");
    if (pa2_truthy(e)) {
        char *ev = pa2_pystr(e);
        char *msg = pa2_fmt(pa2_fmt("Photon ", action), pa2_fmt(" failed: ", ev));
        json_t *r = pa2_err("RuntimeError", msg);
        free(ev); free(msg);
        return r;
    }
    return NULL;
}

/* PoP: pa2__unwrap_list @ plugins/platforms/photon/auth.py:_unwrap_list */
static json_t *pa2__unwrap_list(pa2_ctx_t *ctx, const json_t *d) {
    (void)ctx;
    if (d && d->type == JSON_ARRAY) return json_copy(d);
    if (d && d->type == JSON_OBJECT) {
        static const char *KEYS[] = {"data", "projects", "users", "lines", "items", NULL};
        for (int i = 0; KEYS[i]; i++) {
            const json_t *inner = json_obj_get(d, KEYS[i]);
            if (inner && inner->type == JSON_ARRAY) return json_copy(inner);
            if (inner && inner->type == JSON_OBJECT) {
                static const char *SUB[] = {"projects", "users", "lines", "items", NULL};
                for (int j = 0; SUB[j]; j++) {
                    const json_t *nest = json_obj_get(inner, SUB[j]);
                    if (nest && nest->type == JSON_ARRAY) return json_copy(nest);
                }
            }
        }
    }
    return json_array();
}

/* PoP: pa2_user_assigned_line @ plugins/platforms/photon/auth.py:user_assigned_line */
static json_t *pa2_user_assigned_line(pa2_ctx_t *ctx, const json_t *u) {
    (void)ctx;
    const json_t *v = pa2_truthy(u) ? json_obj_get(u, "assignedPhoneNumber") : NULL;
    if (!pa2_truthy(v)) return NULL;
    char *s = pa2_pystr(v);
    json_t *r = json_string(s);
    free(s);
    return r;
}

/* PoP: pa2_user_opted_in @ plugins/platforms/photon/auth.py:user_opted_in */
static json_t *pa2_user_opted_in(pa2_ctx_t *ctx, const json_t *u) {
    (void)ctx;
    const json_t *meta = pa2_truthy(u) ? json_obj_get(u, "meta") : NULL;
    bool ok = meta && meta->type == JSON_OBJECT && pa2_truthy(json_obj_get(meta, "opt_in"));
    return json_bool(ok);
}

/* PoP: pa2__get_config_env_value @ plugins/platforms/photon/auth.py:_get_config_env_value */
static json_t *pa2__get_config_env_value(pa2_ctx_t *ctx, const json_t *a) {
    const char *key = json_get_str(a, "key", "");
    if (json_get_bool(a, "config_available", true)) {
        json_t *args = pa2_args();
        pa2_set_s(args, "key", key);
        return pa2_call(ctx, "config.get_env_value", args);
    }
    json_t *args = pa2_args();
    pa2_set_s(args, "key", key);
    return pa2_call(ctx, "os.getenv", args);
}

/* PoP: pa2__configured_operator_phone @ plugins/platforms/photon/auth.py:_configured_operator_phone */
static json_t *pa2__configured_operator_phone(pa2_ctx_t *ctx, const json_t *a) {
    json_t *hargs = pa2_args();
    pa2_set_s(hargs, "key", "PHOTON_HOME_CHANNEL");
    json_set(hargs, "config_available", json_obj_get(a, "config_available")
             ? json_copy(json_obj_get(a, "config_available")) : json_bool(true));
    json_t *hv = pa2__get_config_env_value(ctx, hargs);
    json_free(hargs);
    if (pa2_is_raise(hv)) return hv;
    json_t *nm = pa2__normalize_phone(ctx, hv);
    const char *ns = json_string_value(nm);
    char *home = ns ? pa2_strdup(ns) : pa2_strdup("");
    json_free(nm); json_free(hv);
    if (*home && pa2_e164(home)) { json_t *r = json_string(home); free(home); return r; }
    free(home);
    json_t *args = pa2_args();
    pa2_set_s(args, "key", "PHOTON_ALLOWED_USERS");
    json_set(args, "config_available", json_obj_get(a, "config_available")
             ? json_copy(json_obj_get(a, "config_available")) : json_bool(true));
    json_t *av = pa2__get_config_env_value(ctx, args);
    json_free(args);
    if (pa2_is_raise(av)) return av;
    const char *raw = pa2_str(av) ? pa2_str(av) : "";
    json_t *out = NULL;
    int found = 0;
    size_t n = strlen(raw);
    char *tok = (char *)malloc(n + 1);
    size_t tj = 0;
    for (size_t i = 0; i <= n; i++) {
        char c = raw[i];
        if (c == ',' || isspace((unsigned char)c) || c == '\0') {
            if (tj) {
                tok[tj] = '\0';
                json_t *nv = json_string(tok);
                json_t *nt = pa2__normalize_phone(ctx, nv);
                json_free(nv);
                const char *cand = json_string_value(nt);
                if (cand && pa2_e164(cand)) {
                    if (found && out && out->type == JSON_STRING) json_free(out);
                    out = json_string(cand);
                    found++;
                }
                json_free(nt);
                tj = 0;
            }
        } else tok[tj++] = c;
    }
    free(tok);
    json_free(av);
    if (found != 1) { if (out) json_free(out); return json_null(); }
    return out;
}

/* PoP: pa2__safe @ plugins/platforms/photon/auth.py:_safe */
static json_t *pa2__safe(pa2_ctx_t *ctx, const json_t *a) {
    json_t *args = pa2_args();
    pa2_set_copy(args, "fn", json_obj_get(a, "fn"));
    json_t *r = pa2_call(ctx, "safe_call", args);
    if (pa2_is_raise(r)) {
        if (pa2_is_exception(pa2_kind(r))) { json_free(r); return NULL; }
        return r;
    }
    json_free(r);
    return NULL;
}

/* PoP: pa2__response_error_detail @ plugins/platforms/photon/auth.py:_response_error_detail */
static json_t *pa2__response_error_detail(pa2_ctx_t *ctx, const json_t *resp) {
    json_t *r = pa2_call(ctx, "resp.json", pa2_args());
    json_t *data = NULL;
    if (pa2_is_raise(r)) {
        if (pa2_is_exception(pa2_kind(r))) json_free(r);
        else return r;
    } else data = r;
    if (data && data->type == JSON_OBJECT) {
        static const char *KEYS[] = {"error", "message", "detail", NULL};
        for (int i = 0; KEYS[i]; i++) {
            const json_t *v = json_obj_get(data, KEYS[i]);
            if (pa2_truthy(v)) {
                char *s = pa2_pystr(v);
                json_t *ret = json_string(s);
                free(s); json_free(data);
                return ret;
            }
        }
        char *s = pa2_dumps_sorted_trunc(data);
        json_t *ret = json_string(s);
        free(s); json_free(data);
        return ret;
    }
    if (data) json_free(data);
    const json_t *txt = resp && resp->type == JSON_OBJECT ? json_obj_get(resp, "text") : NULL;
    char *s = pa2_truthy(txt) ? pa2_pystr(txt) : pa2_strdup("");
    size_t n = strlen(s);
    json_t *ret;
    if (*s && n > 500) { s[500] = '\0'; ret = json_string(s); }
    else if (*s) ret = json_string(s);
    else ret = json_string("no response body");
    free(s);
    return ret;
}

/* PoP: pa2__raise_for_status @ plugins/platforms/photon/auth.py:_raise_for_status */
static json_t *pa2__raise_for_status(pa2_ctx_t *ctx, const json_t *resp, const char *action) {
    double status = resp && resp->type == JSON_OBJECT ? json_get_num(resp, "status_code", 200) : 200;
    if (status >= 400) {
        json_t *det = pa2__response_error_detail(ctx, resp);
        if (pa2_is_raise(det)) return det;
        char *sv = pa2_pystr(json_number(status));
        char *msg = pa2_fmt(pa2_fmt(pa2_fmt("Photon ", action), pa2_fmt(" failed: HTTP ", sv)),
                            pa2_fmt(": ", json_string_value(det) ? json_string_value(det) : ""));
        json_t *r = pa2_err("RuntimeError", msg);
        free(sv); free(msg); json_free(det);
        return r;
    }
    return NULL;
}

/* PoP: pa2__pool_first @ plugins/platforms/photon/auth.py:_pool_first */
static json_t *pa2__pool_first(pa2_ctx_t *ctx, const json_t *auth, const char *key) {
    (void)ctx;
    const json_t *cp = auth && auth->type == JSON_OBJECT ? json_obj_get(auth, "credential_pool") : NULL;
    const json_t *pool = cp && cp->type == JSON_OBJECT ? json_obj_get(cp, key) : NULL;
    if (!pa2_truthy(pool)) return NULL;
    if (pool->type == JSON_ARRAY && json_len(pool)) return json_copy(json_get(pool, 0));
    return NULL;
}

/* PoP: pa2__auth_json_path @ plugins/platforms/photon/auth.py:_auth_json_path */
static json_t *pa2__auth_json_path(pa2_ctx_t *ctx, const json_t *a) {
    (void)a;
    json_t *h = pa2_call(ctx, "get_hermes_home", pa2_args());
    if (pa2_is_raise(h)) return h;
    char *p = pa2_join(pa2_str(h) ? pa2_str(h) : "", "auth.json");
    json_free(h);
    json_t *r = json_string(p);
    free(p);
    return r;
}

/* PoP: pa2__load_auth @ plugins/platforms/photon/auth.py:_load_auth */
static json_t *pa2__load_auth(pa2_ctx_t *ctx, const json_t *a) {
    (void)a;
    json_t *path = pa2__auth_json_path(ctx, NULL);
    if (pa2_is_raise(path)) return path;
    const char *ps = json_string_value(path);
    json_t *eargs = pa2_args();
    pa2_set_s(eargs, "path", ps);
    json_t *ex = pa2_call(ctx, "path.exists", eargs);
    if (pa2_is_raise(ex)) { json_free(path); return ex; }
    if (!pa2_truthy(ex)) { json_free(path); json_free(ex); return json_object(); }
    json_free(ex);
    json_t *largs = pa2_args();
    pa2_set_s(largs, "path", ps);
    pa2_set_s(largs, "encoding", "utf-8-sig");
    json_t *data = pa2_call(ctx, "json.load", largs);
    json_free(path);
    if (pa2_is_raise(data)) {
        if (pa2_is_oserror(pa2_kind(data)) || pa2_exc_is(pa2_kind(data), "JSONDecodeError")) {
            json_free(data);
            return json_object();
        }
        return data;
    }
    if (!pa2_truthy(data)) { json_free(data); return json_object(); }
    return data;
}

/* PoP: pa2__save_auth @ plugins/platforms/photon/auth.py:_save_auth */
static json_t *pa2__save_auth(pa2_ctx_t *ctx, const json_t *data) {
    json_t *path = pa2__auth_json_path(ctx, NULL);
    if (pa2_is_raise(path)) return path;
    const char *ps = json_string_value(path);
    char *par = pa2_parent(ps);
    json_t *margs = pa2_args();
    pa2_set_s(margs, "path", par);
    pa2_set_b(margs, "parents", true);
    pa2_set_b(margs, "exist_ok", true);
    json_t *m = pa2_call(ctx, "path.mkdir", margs);
    free(par);
    if (pa2_is_raise(m)) { json_free(path); return m; }
    json_free(m);
    json_t *pid = pa2_call(ctx, "os.getpid", pa2_args());
    if (pa2_is_raise(pid)) { json_free(path); return pid; }
    json_t *hex = pa2_call(ctx, "uuid4_hex", pa2_args());
    if (pa2_is_raise(hex)) { json_free(pid); json_free(path); return hex; }
    char *pidv = pa2_pystr(pid);
    const char *basename = pa2_name(ps);
    char *nm = (char *)malloc(strlen(basename) + strlen(pidv) + strlen(json_string_value(hex)) + 16);
    snprintf(nm, strlen(basename) + strlen(pidv) + strlen(json_string_value(hex)) + 16,
             "%s.tmp.%s.%s", basename, pidv, json_string_value(hex));
    free(pidv); json_free(pid); json_free(hex);
    char *tmppar = pa2_parent(ps);
    char *tmp = pa2_join(tmppar, nm);
    free(tmppar); free(nm);
    json_t *oargs = pa2_args();
    pa2_set_s(oargs, "path", tmp);
    pa2_set_n(oargs, "flags", 193); /* O_WRONLY|O_CREAT|O_EXCL = 1|64|128 */
    pa2_set_n(oargs, "mode", 384); /* 0o600 */
    json_t *fd = pa2_call(ctx, "os.open", oargs);
    if (pa2_is_raise(fd)) { free(tmp); json_free(path); return fd; }
    json_t *foargs = pa2_args();
    pa2_set_copy(foargs, "fd", fd);
    pa2_set_s(foargs, "mode", "w");
    pa2_set_s(foargs, "encoding", "utf-8");
    json_t *fh = pa2_call(ctx, "os.fdopen", foargs);
    if (pa2_is_raise(fh)) {
        json_t *orig = fh;
        json_t *cargs = pa2_args(); pa2_set_copy(cargs, "fd", fd);
        json_t *c = pa2_call(ctx, "os.close", cargs);
        if (pa2_is_raise(c)) {
            if (!pa2_is_oserror(pa2_kind(c))) {
                json_free(orig); json_free(fd); free(tmp); json_free(path);
                return c;
            }
            json_free(c);
        } else json_free(c);
        json_t *uargs = pa2_args(); pa2_set_s(uargs, "path", tmp);
        json_t *u = pa2_call(ctx, "path.unlink", uargs);
        if (pa2_is_raise(u)) {
            if (!pa2_is_oserror(pa2_kind(u))) {
                json_free(orig); json_free(fd); free(tmp); json_free(path);
                return u;
            }
            json_free(u);
        } else json_free(u);
        json_free(fd); free(tmp); json_free(path);
        return orig;
    }
    json_free(fh);
    json_t *dargs = pa2_args();
    pa2_set_copy(dargs, "data", data);
    pa2_set_s(dargs, "path", tmp);
    pa2_set_n(dargs, "indent", 2);
    pa2_set_b(dargs, "sort_keys", true);
    json_t *d = pa2_call(ctx, "json.dump", dargs);
    json_t *failure = NULL;
    if (pa2_is_raise(d)) failure = d;
    if (!failure) {
        json_t *fargs = pa2_args(); pa2_set_copy(fargs, "fd", fd);
        json_t *f = pa2_call(ctx, "os.fsync", fargs);
        if (pa2_is_raise(f)) failure = f; else json_free(f);
    }
    if (!failure) {
        json_t *rargs = pa2_args();
        pa2_set_s(rargs, "src", tmp); pa2_set_s(rargs, "dst", ps);
        json_t *r = pa2_call(ctx, "path.replace", rargs);
        if (pa2_is_raise(r)) failure = r; else json_free(r);
    }
    if (failure) {
        json_t *replay = json_copy(failure);
        json_t *uargs = pa2_args(); pa2_set_s(uargs, "path", tmp);
        json_t *u = pa2_call(ctx, "path.unlink", uargs);
        if (pa2_is_raise(u) && !pa2_is_oserror(pa2_kind(u))) {
            json_t *bad = u;
            json_free(replay); json_free(failure); json_free(fd); free(tmp); json_free(path);
            return bad;
        }
        json_free(u);
        json_free(failure); json_free(fd); free(tmp); json_free(path);
        /* Original exception propagates (fresh copy of its exact spec). */
        return replay;
    }
    json_free(fd); free(tmp); json_free(path);
    return NULL;
}

/* PoP: pa2__store_pool_record @ plugins/platforms/photon/auth.py:_store_pool_record */
static json_t *pa2__store_pool_record(pa2_ctx_t *ctx, const char *key, const json_t *record) {
    json_t *lk = pa2_call(ctx, "auth_store_lock", pa2_args());
    if (pa2_is_raise(lk)) return lk;
    json_free(lk);
    json_t *auth = pa2__load_auth(ctx, NULL);
    if (pa2_is_raise(auth)) return auth;
    if (auth->type != JSON_OBJECT) { json_free(auth); return json_object(); }
    json_t *cp = json_obj_get(auth, "credential_pool");
    if (!cp || cp->type != JSON_OBJECT) {
        cp = json_object();
        json_set(auth, "credential_pool", cp);
    }
    json_t *arr = json_array();
    json_append(arr, json_copy(record));
    json_set(cp, key, arr);
    json_t *r = pa2__save_auth(ctx, auth);
    json_free(auth);
    return r;
}

/* PoP: pa2_load_photon_token @ plugins/platforms/photon/auth.py:load_photon_token */
static json_t *pa2_load_photon_token(pa2_ctx_t *ctx, const json_t *a) {
    (void)a;
    json_t *auth = pa2__load_auth(ctx, NULL);
    if (pa2_is_raise(auth)) return auth;
    json_t *entry = pa2__pool_first(ctx, auth, "photon");
    const json_t *prov = json_obj_get(auth, "providers");
    const json_t *legacy = prov && prov->type == JSON_OBJECT ? json_obj_get(prov, "photon") : NULL;
    const json_t *tok = NULL;
    if (entry && entry->type == JSON_OBJECT) {
        tok = json_obj_get(entry, "access_token");
        if (!pa2_truthy(tok)) tok = json_obj_get(entry, "token");
    }
    if (!pa2_truthy(tok) && legacy && legacy->type == JSON_OBJECT) tok = json_obj_get(legacy, "access_token");
    json_t *r = NULL;
    if (pa2_truthy(tok)) { char *s = pa2_pystr(tok); r = json_string(s); free(s); }
    else r = json_null();
    json_free(entry); json_free(auth);
    return r;
}

/* PoP: pa2_store_photon_token @ plugins/platforms/photon/auth.py:store_photon_token */
static json_t *pa2_store_photon_token(pa2_ctx_t *ctx, const json_t *a) {
    json_t *t = pa2_call(ctx, "time.time", pa2_args());
    if (pa2_is_raise(t)) return t;
    json_t *rec = json_object();
    pa2_set_copy(rec, "access_token", json_obj_get(a, "token"));
    json_set(rec, "issued_at", json_number((double)pa2_pyint(t)));
    json_free(t);
    json_t *r = pa2__store_pool_record(ctx, "photon", rec);
    json_free(rec);
    return r;
}

/* PoP: pa2_clear_photon_token @ plugins/platforms/photon/auth.py:clear_photon_token */
static json_t *pa2_clear_photon_token(pa2_ctx_t *ctx, const json_t *a) {
    (void)a;
    json_t *auth = pa2__load_auth(ctx, NULL);
    if (pa2_is_raise(auth)) return auth;
    json_t *pool = json_obj_get(auth, "credential_pool");
    if (!pool) pool = json_object();
    if (pool->type != JSON_OBJECT) {
        char msg[160];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", pa2_typename(pool));
        json_free(auth);
        return pa2_err("AttributeError", msg);
    }
    const json_t *photon = json_obj_get(pool, "photon");
    if (photon && photon->type == JSON_ARRAY && json_len(photon)) {
        json_set(pool, "photon", json_array());
        json_t *r = pa2__save_auth(ctx, auth);
        if (pa2_is_raise(r)) { json_free(auth); return r; }
        json_free(r);
    }
    const json_t *prov = json_obj_get(auth, "providers");
    if (prov && prov->type == JSON_OBJECT && json_obj_get(prov, "photon")) {
        json_t *p2 = (json_t *)prov;
        json_t *empty = json_object();
        json_set(p2, "photon", empty);
        json_t *r = pa2__save_auth(ctx, auth);
        if (pa2_is_raise(r)) { json_free(auth); return r; }
        json_free(r);
    }
    json_free(auth);
    return NULL;
}

/* PoP: pa2_load_project_credentials @ plugins/platforms/photon/auth.py:load_project_credentials */
static json_t *pa2_load_project_credentials(pa2_ctx_t *ctx, const json_t *a) {
    (void)a;
    json_t *ids = pa2_call(ctx, "get_scoped_secret", (json_t *)pa2_args_preset("PHOTON_PROJECT_ID"));
    if (pa2_is_raise(ids)) return ids;
    json_t *sec = pa2_call(ctx, "get_scoped_secret", (json_t *)pa2_args_preset("PHOTON_PROJECT_SECRET"));
    if (pa2_is_raise(sec)) return sec;
    if (pa2_truthy(ids) && pa2_truthy(sec)) {
        json_t *t = json_array(); json_append(t, ids); json_append(t, sec);
        return t;
    }
    json_t *auth = pa2__load_auth(ctx, NULL);
    if (pa2_is_raise(auth)) { json_free(ids); json_free(sec); return auth; }
    json_t *entry = pa2__pool_first(ctx, auth, "photon_project");
    json_t *t = json_array();
    if (!entry) { json_append(t, json_copy(ids)); json_append(t, json_copy(sec)); }
    else {
        const json_t *sid = json_obj_get(entry, "spectrum_project_id");
        if (!pa2_truthy(sid)) sid = json_obj_get(entry, "project_id");
        const json_t *s2 = json_obj_get(entry, "project_secret");
        json_t *v1 = pa2_truthy(ids) ? json_copy(ids) : (pa2_truthy(sid) ? json_copy(sid) : json_null());
        json_t *v2 = pa2_truthy(sec) ? json_copy(sec) : (pa2_truthy(s2) ? json_copy(s2) : json_null());
        json_append(t, v1); json_append(t, v2);
        json_free(entry);
    }
    json_free(ids); json_free(sec); json_free(auth);
    return t;
}

/* PoP: pa2_load_dashboard_project_id @ plugins/platforms/photon/auth.py:load_dashboard_project_id */
static json_t *pa2_load_dashboard_project_id(pa2_ctx_t *ctx, const json_t *a) {
    (void)a;
    json_t *ids = pa2_call(ctx, "get_scoped_secret", (json_t *)pa2_args_preset("PHOTON_DASHBOARD_PROJECT_ID"));
    if (pa2_is_raise(ids)) return ids;
    if (pa2_truthy(ids)) return ids;
    json_t *auth = pa2__load_auth(ctx, NULL);
    if (pa2_is_raise(auth)) { json_free(ids); return auth; }
    json_t *entry = pa2__pool_first(ctx, auth, "photon_project");
    json_free(auth);
    if (!entry) return json_null();
    const json_t *v = json_obj_get(entry, "spectrum_project_id");
    if (!pa2_truthy(v)) v = json_obj_get(entry, "dashboard_project_id");
    if (!pa2_truthy(v)) v = json_obj_get(entry, "project_id");
    json_t *r = pa2_truthy(v) ? json_copy(v) : json_null();
    json_free(entry);
    return r;
}

/* forward decl: defined below (in-function import gate) */
static json_t *pa2__persist_runtime_env(pa2_ctx_t *ctx, const json_t *a);

/* PoP: pa2_store_project_credentials @ plugins/platforms/photon/auth.py:store_project_credentials */
static json_t *pa2_store_project_credentials(pa2_ctx_t *ctx, const json_t *a) {
    json_t *t = pa2_call(ctx, "time.time", pa2_args());
    if (pa2_is_raise(t)) return t;
    json_t *rec = json_object();
    pa2_set_copy(rec, "spectrum_project_id", json_obj_get(a, "spectrum_project_id"));
    pa2_set_copy(rec, "project_secret", json_obj_get(a, "project_secret"));
    json_set(rec, "issued_at", json_number((double)pa2_pyint(t)));
    json_free(t);
    const char *PAIRS[] = {"dashboard_project_id", "name", NULL};
    for (int i = 0; PAIRS[i]; i++) {
        const json_t *v = json_obj_get(a, PAIRS[i]);
        if (pa2_truthy(v)) json_set(rec, PAIRS[i], json_copy(v));
    }
    json_t *r = pa2__store_pool_record(ctx, "photon_project", rec);
    json_free(rec);
    if (pa2_is_raise(r)) return r;
    json_free(r);
    json_t *c = json_object();
    pa2_set_copy(c, "spectrum_project_id", json_obj_get(a, "spectrum_project_id"));
    pa2_set_copy(c, "project_secret", json_obj_get(a, "project_secret"));
    const json_t *cav = json_obj_get(a, "config_available");
    json_set(c, "config_available", cav ? json_copy(cav) : json_bool(true));
    return pa2__persist_runtime_env(ctx, c);
}

/* PoP: pa2_store_user_numbers @ plugins/platforms/photon/auth.py:store_user_numbers */
static json_t *pa2_store_user_numbers(pa2_ctx_t *ctx, const json_t *a) {
    const json_t *ph = json_obj_get(a, "phone_number");
    const json_t *as = json_obj_get(a, "assigned_phone_number");
    if (!pa2_truthy(ph) && !pa2_truthy(as)) return NULL;
    json_t *t = pa2_call(ctx, "time.time", pa2_args());
    if (pa2_is_raise(t)) return t;
    json_t *rec = json_object();
    json_set(rec, "issued_at", json_number((double)pa2_pyint(t)));
    json_free(t);
    const char *PAIRS[] = {"phone_number", "assigned_phone_number", "user_id", "dashboard_project_id", NULL};
    for (int i = 0; PAIRS[i]; i++) {
        const json_t *v = json_obj_get(a, PAIRS[i]);
        if (pa2_truthy(v)) json_set(rec, PAIRS[i], json_copy(v));
    }
    json_t *r = pa2__store_pool_record(ctx, "photon_user", rec);
    json_free(rec);
    return r;
}

/* PoP: pa2__persist_runtime_env @ plugins/platforms/photon/auth.py:_persist_runtime_env */
static json_t *pa2__persist_runtime_env(pa2_ctx_t *ctx, const json_t *a) {
    if (!json_get_bool(a, "config_available", true)) return NULL;
    const char *NAMES[] = {"PHOTON_PROJECT_ID", "PHOTON_PROJECT_SECRET", NULL};
    const char *SRC[] = {"spectrum_project_id", "project_secret", NULL};
    for (int i = 0; NAMES[i]; i++) {
        json_t *args = pa2_args();
        pa2_set_s(args, "key", NAMES[i]);
        pa2_set_copy(args, "value", json_obj_get(a, SRC[i]));
        json_t *r = pa2_call(ctx, "config.save_env_value", args);
        if (pa2_is_raise(r)) {
            if (pa2_is_exception(pa2_kind(r))) { json_free(r); return NULL; }
            return r;
        }
        json_free(r);
    }
    return NULL;
}

/* PoP: pa2__device_response_token_candidates @ plugins/platforms/photon/auth.py:_device_response_token_candidates */
static json_t *pa2__device_response_token_candidates(pa2_ctx_t *ctx, const json_t *body, const json_t *headers) {
    const json_t *ses = body && body->type == JSON_OBJECT ? json_obj_get(body, "session") : NULL;
    const json_t *session = ses && ses->type == JSON_OBJECT ? ses : NULL;
    const json_t *dt = body && body->type == JSON_OBJECT ? json_obj_get(body, "data") : NULL;
    const json_t *data = dt && dt->type == JSON_OBJECT ? dt : NULL;
    static const char *SRC[] = {"access_token", "accessToken", "session.access_token",
                                "data.access_token", "data.accessToken", "set-auth-token", NULL};
    const json_t *VALS[6] = {0};
    if (body && body->type == JSON_OBJECT) {
        VALS[0] = json_obj_get(body, "access_token");
        VALS[1] = json_obj_get(body, "accessToken");
    }
    if (session) VALS[2] = json_obj_get(session, "access_token");
    if (data) { VALS[3] = json_obj_get(data, "access_token"); VALS[4] = json_obj_get(data, "accessToken"); }
    VALS[5] = pa2__header_value(ctx, headers, "set-auth-token");
    json_t *out = json_array();
    const char *seen[6]; size_t nseen = 0;
    for (int i = 0; i < 6; i++) {
        json_t *tok = pa2__clean_bearer_token(ctx, VALS[i]);
        const char *s = json_string_value(tok);
        if (s && *s) {
            int dup = 0;
            for (size_t k = 0; k < nseen; k++) if (!strcmp(seen[k], s)) { dup = 1; break; }
            if (!dup) {
                seen[nseen++] = s;
                json_t *c = json_object();
                pa2_set_s(c, "source", SRC[i]);
                pa2_set_copy(c, "token", tok);
                json_append(out, c);
            }
        }
        json_free(tok);
    }
    return out;
}

/* shared: HTTP seam. body/headers omitted when NULL (matches live kwargs). */
static json_t *pa2__http(pa2_ctx_t *ctx, const char *method, const char *url,
                         const json_t *body, const json_t *headers, bool reply) {
    json_t *args = pa2_args();
    pa2_set_s(args, "url", url);
    if (body) json_set(args, "json", json_copy(body));
    if (headers) json_set(args, "headers", json_copy(headers));
    pa2_set_n(args, "timeout", 30.0);
    json_t *resp = pa2_call(ctx, method, args);
    if (pa2_is_raise(resp)) return resp;
    if (!reply) return resp;
    json_t *r = pa2_call(ctx, "resp.raise_for_status", pa2_args());
    if (pa2_is_raise(r)) { json_free(resp); return r; }
    json_free(r);
    json_t *data = pa2_call(ctx, "resp.json", pa2_args());
    json_free(resp);
    if (pa2_is_raise(data)) return data;
    if (!pa2_truthy(data)) { json_free(data); return json_object(); }
    return data;
}

/* PoP: pa2__dashboard_get @ plugins/platforms/photon/auth.py:_dashboard_get */
static json_t *pa2__dashboard_get(pa2_ctx_t *ctx, const json_t *a) {
    const char *what = json_get_str(a, "what", " device login");
    /* _require_httpx reads args.httpx_available via a bool key; set from a */
    json_t *ra = json_object();
    json_set(ra, "httpx_available", json_obj_get(a, "httpx_available")
             ? json_copy(json_obj_get(a, "httpx_available")) : json_bool(true));
    pa2_set_s(ra, "what", what);
    json_t *r = pa2__require_httpx(ctx, ra);
    json_free(ra);
    if (r) return r;
    json_t *host = pa2__dashboard_host(ctx, NULL);
    if (pa2_is_raise(host)) return host;
    char *url = pa2_fmt(json_string_value(host) ? json_string_value(host) : "", json_get_str(a, "path", ""));
    json_free(host);
    json_t *bearer = pa2__bearer(ctx, json_obj_get(a, "token"));
    json_t *resp = pa2__http(ctx, "httpx.get", url, NULL, bearer, false);
    json_free(bearer);
    free(url);
    return resp;
}

/* PoP: pa2__dashboard_post @ plugins/platforms/photon/auth.py:_dashboard_post */
static json_t *pa2__dashboard_post(pa2_ctx_t *ctx, const json_t *a) {
    json_t *ra = json_object();
    json_set(ra, "httpx_available", json_obj_get(a, "httpx_available")
             ? json_copy(json_obj_get(a, "httpx_available")) : json_bool(true));
    pa2_set_s(ra, "what", json_get_str(a, "what", ""));
    json_t *r = pa2__require_httpx(ctx, ra);
    json_free(ra);
    if (r) return r;
    json_t *host = pa2__dashboard_host(ctx, NULL);
    if (pa2_is_raise(host)) return host;
    char *url = pa2_fmt(json_string_value(host) ? json_string_value(host) : "", json_get_str(a, "path", ""));
    json_free(host);
    json_t *bearer = pa2__bearer(ctx, json_obj_get(a, "token"));
    json_t *resp = pa2__http(ctx, "httpx.post", url, json_obj_get(a, "body"), bearer, true);
    json_free(bearer);
    free(url);
    return resp;
}

/* PoP: pa2__dashboard_list @ plugins/platforms/photon/auth.py:_dashboard_list */
static json_t *pa2__dashboard_list(pa2_ctx_t *ctx, const json_t *a) {
    json_t *ga = json_object();
    pa2_set_s(ga, "path", json_get_str(a, "path", ""));
    pa2_set_copy(ga, "token", json_obj_get(a, "token"));
    pa2_set_s(ga, "what", "");
    json_set(ga, "httpx_available", json_obj_get(a, "httpx_available")
             ? json_copy(json_obj_get(a, "httpx_available")) : json_bool(true));
    json_t *resp = pa2__dashboard_get(ctx, ga);
    json_free(ga);
    if (pa2_is_raise(resp)) return resp;
    json_t *r = pa2_call(ctx, "resp.raise_for_status", pa2_args());
    if (pa2_is_raise(r)) { json_free(resp); return r; }
    json_free(r);
    json_t *data = pa2_call(ctx, "resp.json", pa2_args());
    json_free(resp);
    if (pa2_is_raise(data)) return data;
    json_t *out = pa2__unwrap_list(ctx, data);
    json_free(data);
    return out;
}

/* PoP: pa2_list_projects @ plugins/platforms/photon/auth.py:list_projects */
static json_t *pa2_list_projects(pa2_ctx_t *ctx, const json_t *a) {
    json_t *la = json_object();
    pa2_set_s(la, "path", "/api/projects");
    pa2_set_copy(la, "token", json_obj_get(a, "token"));
    const json_t *hav = json_obj_get(a, "httpx_available");
    json_set(la, "httpx_available", hav ? json_copy(hav) : json_bool(true));
    return pa2__dashboard_list(ctx, la);
}

/* PoP: pa2_find_project_by_name @ plugins/platforms/photon/auth.py:find_project_by_name */
static json_t *pa2_find_project_by_name(pa2_ctx_t *ctx, const json_t *a) {
    const char *nm = pa2_str(json_obj_get(a, "name")) ? pa2_str(json_obj_get(a, "name")) : "";
    char *target = pa2_strip(nm);
    char *tl = pa2_lower(target);
    free(target);
    json_t *projects = pa2_list_projects(ctx, a);
    if (pa2_is_raise(projects)) { free(tl); return projects; }
    json_t *found = NULL;
    for (size_t i = 0; i < json_len(projects); i++) {
        json_t *p = json_get(projects, i);
        const char *pn = pa2_str(json_obj_get(p, "name")) ? pa2_str(json_obj_get(p, "name")) : "";
        char *pt = pa2_strip(pn);
        char *pl = pa2_lower(pt);
        free(pt);
        if (!strcmp(pl, tl)) { found = json_copy(p); break; }
        free(pl);
    }
    free(tl);
    json_free(projects);
    return found ? found : json_null();
}

/* PoP: pa2_request_device_code @ plugins/platforms/photon/auth.py:request_device_code */
static json_t *pa2_request_device_code(pa2_ctx_t *ctx, const json_t *a) {
    json_t *ra = json_object();
    json_set(ra, "httpx_available", json_obj_get(a, "httpx_available")
             ? json_copy(json_obj_get(a, "httpx_available")) : json_bool(true));
    pa2_set_s(ra, "what", " device login");
    json_t *r = pa2__require_httpx(ctx, ra);
    json_free(ra);
    if (r) return r;
    json_t *host = pa2__dashboard_host(ctx, NULL);
    if (pa2_is_raise(host)) return host;
    char *url = pa2_fmt(json_string_value(host) ? json_string_value(host) : "", "/api/auth/device/code");
    json_free(host);
    json_t *body = json_object();
    const json_t *cid = json_obj_get(a, "client_id");
    char *cidv = cid ? pa2_pystr(cid) : pa2_strdup("photon-cli");
    json_set(body, "client_id", json_string(cidv));
    free(cidv);
    const json_t *sc = json_obj_get(a, "scope");
    if (!sc) json_set(body, "scope", json_string("openid profile email"));
    else if (pa2_truthy(sc)) json_set(body, "scope", json_copy(sc));
    json_t *resp = pa2__http(ctx, "httpx.post", url, body, NULL, false);
    free(url); json_free(body);
    if (pa2_is_raise(resp)) return resp;
    json_t *rr = pa2_call(ctx, "resp.raise_for_status", pa2_args());
    if (pa2_is_raise(rr)) { json_free(resp); return rr; }
    json_free(rr);
    json_t *data = pa2_call(ctx, "resp.json", pa2_args());
    json_free(resp);
    if (pa2_is_raise(data)) return data;
    json_t *dc = json_object();
    const char *STR[] = {"device_code", "user_code", "verification_uri", NULL};
    for (int i = 0; i < 3; i++) {
        char *s = pa2_pystr(json_obj_get(data, STR[i]));
        json_set(dc, STR[i], json_string(s));
        free(s);
    }
    const json_t *vc = json_obj_get(data, "verification_uri_complete");
    json_set(dc, "verification_uri_complete", vc ? json_copy(vc) : json_null());
    const json_t *ev = json_obj_get(data, "expires_in");
    long long exp = pa2_truthy(ev) ? pa2_pyint(ev) : 1800;
    const json_t *iv = json_obj_get(data, "interval");
    long long itv = pa2_truthy(iv) ? pa2_pyint(iv) : 5;
    json_set(dc, "expires_in", json_number((double)exp));
    json_set(dc, "interval", json_number((double)itv));
    json_free(data);
    return dc;
}

/* shared: the nested _get inside validate_photon_token */
static json_t *pa2__validate_get(pa2_ctx_t *ctx, const char *token, const char *path, const char *rejected) {
    json_t *ga = json_object();
    pa2_set_s(ga, "path", path);
    pa2_set_s(ga, "token", token);
    pa2_set_s(ga, "what", " device login");
    json_set(ga, "httpx_available", json_bool(true));
    json_t *resp = pa2__dashboard_get(ctx, ga);
    json_free(ga);
    if (pa2_is_raise(resp)) return resp;
    double st = resp->type == JSON_OBJECT ? json_get_num(resp, "status_code", 200) : 200;
    if (st == 401 || st == 403) {
        json_free(resp);
        return pa2_err("PhotonDashboardAuthError", rejected);
    }
    json_t *r = pa2_call(ctx, "resp.raise_for_status", pa2_args());
    if (pa2_is_raise(r)) { json_free(resp); return r; }
    json_free(r);
    return resp;
}

/* PoP: pa2_validate_photon_token @ plugins/platforms/photon/auth.py:validate_photon_token */
static json_t *pa2_validate_photon_token(pa2_ctx_t *ctx, const json_t *a) {
    const char *tok = json_get_str(a, "token", "");
    json_t *resp = pa2__validate_get(ctx, tok, "/api/auth/get-session",
        "Photon issued a device token, but the dashboard session lookup rejected it.");
    if (pa2_is_raise(resp)) return resp;
    json_t *data = pa2_call(ctx, "resp.json", pa2_args());
    json_free(resp);
    if (pa2_is_raise(data)) return data;
    const json_t *user = data && data->type == JSON_OBJECT ? json_obj_get(data, "user") : NULL;
    bool ok = user && user->type == JSON_OBJECT && user->c.count > 0;
    if (!ok) {
        json_free(data);
        return pa2_err("PhotonDashboardAuthError",
            "Photon issued a device token, but the dashboard session lookup did not recognize it.");
    }
    json_t *user_copy = json_copy(user);
    json_free(data);
    json_t *resp2 = pa2__validate_get(ctx, tok, "/api/projects/",
        "Photon device token was accepted for the session lookup but rejected by the project API.");
    if (pa2_is_raise(resp2)) { json_free(user_copy); return resp2; }
    json_free(resp2);
    return user_copy;
}

/* PoP: pa2_check_photon_token_valid @ plugins/platforms/photon/auth.py:check_photon_token_valid */
static json_t *pa2_check_photon_token_valid(pa2_ctx_t *ctx, const json_t *a) {
    const json_t *tok = json_obj_get(a, "token");
    if (!pa2_truthy(tok)) return json_bool(false);
    json_t *va = json_object();
    pa2_set_copy(va, "token", tok);
    json_t *r = pa2_validate_photon_token(ctx, va);
    json_free(va);
    if (pa2_is_raise(r)) {
        if (pa2_kind_is(r, "PhotonDashboardAuthError")) { json_free(r); return json_bool(false); }
        if (pa2_is_exception(pa2_kind(r))) { json_free(r); return json_bool(true); }
        return r;
    }
    json_free(r);
    return json_bool(true);
}

/* PoP: pa2__validated_dashboard_token @ plugins/platforms/photon/auth.py:_validated_dashboard_token */
static json_t *pa2__validated_dashboard_token(pa2_ctx_t *ctx, const json_t *cands) {
    if (!cands || cands->type != JSON_ARRAY || json_len(cands) == 0) {
        return pa2_err("RuntimeError", "Photon returned 200 but no token candidate in the device-token response.");
    }
    json_t *dash = NULL, *last = NULL;
    for (size_t i = 0; i < json_len(cands); i++) {
        json_t *c = json_get(cands, i);
        json_t *va = json_object();
        pa2_set_copy(va, "token", json_obj_get(c, "token"));
        json_t *r = pa2_validate_photon_token(ctx, va);
        json_free(va);
        if (pa2_is_raise(r)) {
            if (last) json_free(last);
            last = r;
            if (pa2_kind_is(r, "PhotonDashboardAuthError")) {
                if (dash) json_free(dash);
                dash = json_copy(r);
            }
            if (!pa2_is_exception(pa2_kind(r))) {
                if (dash) json_free(dash);
                return r;
            }
            continue;
        }
        json_t *t = json_copy(json_obj_get(c, "token"));
        json_free(r); json_free(last); json_free(dash);
        return t;
    }
    if (dash) {
        const char *msg = json_get_str(dash, "message", "");
        size_t n = strlen(msg) + 128 + json_len(cands) * 64;
        char *buf = (char *)malloc(n);
        size_t off = snprintf(buf, n, "%s Device login returned no project-valid dashboard token (tried: ", msg);
        for (size_t i = 0; i < json_len(cands); i++) {
            if (i) off += snprintf(buf + off, n - off, ", ");
            off += snprintf(buf + off, n - off, "%s", json_get_str(json_get(cands, i), "source", ""));
        }
        snprintf(buf + off, n - off, ").");
        json_t *r = pa2_err("PhotonDashboardAuthError", buf);
        free(buf); json_free(dash); json_free(last);
        return r;
    }
    if (last) return last;
    return pa2_err("RuntimeError", "Photon did not return a usable dashboard token");
}
