/*
 * scale_to_zero_flow.c — gateway/scale_to_zero.py UNCLAIMED-remainder FLOW cores
 * (universal-seam convention, sibling of run_shutdown_flow2.c / mcp_oauth_flow.c).
 *
 * Ports the SUSPEND lifecycle of the scale-to-zero module: the env-string leaf,
 * the Fly self-suspend probe, the NAS brokered-sleep URL validator (with its
 * once-per-process malformed latch), the any-lever composition, the brokered
 * POST and the local flaps unix-socket POST. Call order, short-circuit branches,
 * early returns, context-manager enter/exit protocol, exception->fallback paths
 * and the latch mutation order of the live Python are transcribed statement by
 * statement. Every live I/O (environ reads, os.path.exists, urlsplit, logging,
 * urllib Request/urlopen/opener, socket ctor + sock methods, CM enter/exit)
 * enters through stz_seams_t.call(ctx, name, args); args is OWNED by stz_call
 * (freed after the borrowed call); results are NEW json_t with NULL == Python
 * None and {"__raise__":K,"msg":M} == a raised exception. BaseException kinds
 * (SystemExit/KeyboardInterrupt/GeneratorExit/CancelledError/BaseExceptionGroup)
 * pass through every `except OSError` / `except (URLError,OSError,ValueError)`
 * site (they are not Exception descendants).
 *
 * Self-contained: libjson only (no cross-object symbols). All helpers static stz_*.
 *
 * The 8 CLAIMED siblings (scale_to_zero_enabled, parse_idle_timeout_seconds,
 * messaging_is_relay_only_or_absent, should_arm, is_idle,
 * dashboard_client_heartbeat_path, touch_dashboard_client_heartbeat,
 * dashboard_client_last_seen) are the DECISION layer; none is called by these 6
 * suspend-layer functions, so they appear here only as documented seams of the
 * wider module (no re-claim).
 *
 * Modeling notes (differential oracle tests/oracle_scale_to_zero_flow.py):
 *  - environ is a JSON object (dict) or null (None -> the "os.environ" seam).
 *    A scripted-null os.environ makes `.get` raise AttributeError 'NoneType'.
 *    env values are str/int/bool/null; integral-FLOAT values are excluded from
 *    str()-computed positions (libjson has no int/float identity).
 *  - The malformed-sleep-url latch (_malformed_sleep_url_logged) is the module's
 *    only mutable global; it travels as the per-case state object
 *    {"malformed_logged": bool}, reset from in.state on both sides and echoed
 *    as out.state (mutation history preserved even on a raise).
 *  - urlsplit results are objects carrying scheme+netloc (domain restriction;
 *    a non-object result raises the 'NoneType'/'<T>' scheme AttributeError).
 *  - CM handles: an OBJECT seam result models a context manager (urlopen/opener
 *    response, socket sock). __enter__ fires "<role>.__enter__" {} and returns
 *    the handle itself (urlopen/socket contract); the script gates raise only.
 *    __exit__ fires "<role>.__exit__" {"exc": <raise-spec|null>}; a truthy result
 *    suppresses the body exception, an __exit__ raise overrides it (discarding
 *    the body return/raise). A NON-object handle raises the py3.14 TypeError
 *    "'<T>' object does not support the context manager protocol (missed
 *    __exit__ method)". A suppressed body that never bound `status`/`response`
 *    raises UnboundLocalError "cannot access local variable '<v>' where it is
 *    not associated with a value".
 *  - bytes positions (Request data=b"", sock.sendall/recv, the flaps response)
 *    project to ASCII str on both sides; decode("ascii"/"utf-8","replace") is
 *    identity in the ASCII fixture domain. recv chunks are str|null; a truthy
 *    non-str chunk raises "can't concat <T> to bytes". The 65536-byte recv cap
 *    and the [:500] body/json.dumps slices are codepoint==byte in this domain.
 *  - socket constants model AF_UNIX=1, SOCK_STREAM=1 (POSIX values; the socket
 *    module is fully stubbed on both sides — Windows py3.14 lacks AF_UNIX).
 *  - opener is null (use the "urlopen" seam) or a TRUTHY marker (use the
 *    "opener" seam); `opener or urlopen` is truthiness, not presence.
 *  - int() tokens are printable ASCII without quotes/backslashes (repr quoting
 *    unambiguous); status codes <= 9 digits (no big-int overflow). str.strip /
 *    str.split use the White_Space set (incl \x1c-\x1f); int() uses the narrower
 *    C-isspace set. json.dumps(body) uses the Python encoder (\b \f \n \r \t
 *    short forms, other <0x20 -> \u00xx, " and \ escaped).
 *  - Ordered trace; integral floats == ints (canon folds). ASCII scope.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>
#include "libjson/json.h"

typedef struct stz_seams_t {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} stz_seams_t;

/* ── universal-seam helpers ── */
static json_t *stz_call(const stz_seams_t *s, const char *name, json_t *args) {
    json_t *r = (s && s->call) ? s->call(s->ctx, name, args) : NULL;
    json_free(args);
    return r;
}
static bool stz_raised(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return false;
    const json_t *k = json_obj_get(r, "__raise__");
    return k && k->type == JSON_STRING;
}
static const char *stz_kind(const json_t *r) {
    const json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static const char *stz_emsg(const json_t *r) {
    const json_t *k = json_obj_get(r, "msg");
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static json_t *stz_raise(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static json_t *stz_args(void) { return json_object(); }
static void stz_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void stz_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void stz_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static const json_t *stz_oget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

/* ── full builtin exception hierarchy (py3.14) + urllib stub classes ──
 * Single-parent walk; ExceptionGroup is an Exception (also BaseExceptionGroup).
 * EnvironmentError/IOError/WindowsError are OSError aliases. CancelledError and
 * the BaseException trio/group sit OUTSIDE Exception so they pass through every
 * `except Exception`-family site. URLError/HTTPError are the oracle's stub
 * classes (URLError subclasses OSError, HTTPError subclasses URLError). */
static const char *stz_exc_parent(const char *k) {
    static const struct { const char *n, *p; } T[] = {
        {"Exception", "BaseException"},
        {"BaseExceptionGroup", "BaseException"},
        {"GeneratorExit", "BaseException"},
        {"KeyboardInterrupt", "BaseException"},
        {"SystemExit", "BaseException"},
        {"CancelledError", "BaseException"},
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
        {"EnvironmentError", "OSError"},
        {"IOError", "OSError"},
        {"WindowsError", "OSError"},
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
        {"FloatingPointError", "ArithmeticError"},
        {"OverflowError", "ArithmeticError"},
        {"ZeroDivisionError", "ArithmeticError"},
        {"IndexError", "LookupError"},
        {"KeyError", "LookupError"},
        {"ModuleNotFoundError", "ImportError"},
        {"NotImplementedError", "RuntimeError"},
        {"PythonFinalizationError", "RuntimeError"},
        {"RecursionError", "RuntimeError"},
        {"IndentationError", "SyntaxError"},
        {"TabError", "IndentationError"},
        {"_IncompleteInputError", "SyntaxError"},
        {"UnboundLocalError", "NameError"},
        {"UnicodeError", "ValueError"},
        {"UnicodeDecodeError", "UnicodeError"},
        {"UnicodeEncodeError", "UnicodeError"},
        {"UnicodeTranslateError", "UnicodeError"},
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
        {"URLError", "OSError"},
        {"HTTPError", "URLError"},
    };
    for (size_t i = 0; i < sizeof(T) / sizeof(T[0]); i++)
        if (strcmp(T[i].n, k) == 0) return T[i].p;
    return NULL;
}
static bool stz_is_a(const char *kind, const char *ancestor) {
    const char *k = kind;
    for (int depth = 0; k && depth < 64; depth++) {
        if (strcmp(k, ancestor) == 0) return true;
        k = stz_exc_parent(k);
    }
    return false;
}
static bool stz_is_oserror(const char *k) { return stz_is_a(k, "OSError"); }
static bool stz_is_valueerror(const char *k) { return stz_is_a(k, "ValueError"); }
static bool stz_is_httperror(const char *k) { return stz_is_a(k, "HTTPError"); }

/* ── Python value helpers ── */
static bool stz_truth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL:   return false;
        case JSON_BOOL:   return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}
static bool stz_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool stz_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool stz_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static const char *stz_strval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static const char *stz_typename(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    if (v->type == JSON_BOOL) return "bool";
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isinf(d) && !isnan(d) && d == (double)(long long)d) return "int";
        return "float";
    }
    if (v->type == JSON_STRING) return "str";
    if (v->type == JSON_ARRAY) return "list";
    return "dict";
}
/* str() of a scalar (int/bool/str/None). Integral floats render as int (the
 * fixture domain excludes integral floats in str() positions). malloc'd. */
static char *stz_pystr(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isinf(d) && !isnan(d) && d == (double)(long long)d) {
            snprintf(buf, sizeof(buf), "%lld", (long long)d);
            return strdup(buf);
        }
        snprintf(buf, sizeof(buf), "%.17g", d);
        return strdup(buf);
    }
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    return strdup("");
}
/* White_Space codepoint set used by str.strip / str.split (incl \x1c-\x1f). */
static bool stz_is_ws_cp(unsigned int cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0D) || (cp >= 0x1C && cp <= 0x1F))
        return true;
    switch (cp) {
        case 0x85: case 0xA0: case 0x1680:
        case 0x2028: case 0x2029: case 0x202F: case 0x205F: case 0x3000:
            return true;
        default:
            return (cp >= 0x2000 && cp <= 0x200A);
    }
}
/* decode one UTF-8 codepoint at s; advances *i. ASCII fast path. */
static unsigned int stz_next_cp(const char *s, size_t n, size_t *i) {
    unsigned char c = (unsigned char)s[*i];
    if (c < 0x80) { (*i)++; return c; }
    unsigned int cp = 0; int extra = 0;
    if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
    else { (*i)++; return 0xFFFD; }
    (*i)++;
    for (int k = 0; k < extra && *i < n; k++, (*i)++) {
        unsigned char cc = (unsigned char)s[*i];
        if ((cc & 0xC0) != 0x80) break;
        cp = (cp << 6) | (cc & 0x3F);
    }
    return cp;
}
/* str.strip(): drop leading/trailing White_Space codepoints. malloc'd. */
static char *stz_strip(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    size_t start = 0, end = n;
    size_t i = 0;
    while (i < n) {
        size_t save = i;
        unsigned int cp = stz_next_cp(s, n, &i);
        if (!stz_is_ws_cp(cp)) { start = save; break; }
        start = i;
    }
    /* trim from the end: scan codepoints, remember last non-ws boundary */
    i = start; size_t last = start;
    while (i < n) {
        size_t save = i;
        unsigned int cp = stz_next_cp(s, n, &i);
        if (!stz_is_ws_cp(cp)) last = i;
        (void)save;
    }
    end = last;
    if (end < start) end = start;
    size_t len = end - start;
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s + start, len);
    out[len] = '\0';
    return out;
}
/* str.split() (no arg): maximal runs of White_Space, no empty tokens.
 * Returns a json array of string nodes. */
static json_t *stz_split_ws(const char *s) {
    json_t *arr = json_array();
    if (!s) s = "";
    size_t n = strlen(s), i = 0;
    while (i < n) {
        unsigned int cp = stz_next_cp(s, n, &i);
        if (stz_is_ws_cp(cp)) continue;
        size_t start = i - 1; /* cp consumed at least one byte */
        /* recompute start robustly: back up to the byte before this cp */
        /* (ASCII fast path: i-1 correct; multibyte: find lead byte) */
        while (start > 0 && ((unsigned char)s[start] & 0xC0) == 0x80) start--;
        while (i < n) {
            size_t save = i;
            unsigned int c2 = stz_next_cp(s, n, &i);
            if (stz_is_ws_cp(c2)) { i = save; break; }
        }
        size_t len = i - start;
        char *tok = (char *)malloc(len + 1);
        if (tok) { memcpy(tok, s + start, len); tok[len] = '\0'; json_append(arr, json_string(tok)); free(tok); }
    }
    return arr;
}
/* str.isdigit() over the ASCII domain: non-empty, all '0'-'9'. */
static bool stz_isdigit(const char *s) {
    if (!s || !s[0]) return false;
    for (const char *p = s; *p; p++) if (*p < '0' || *p > '9') return false;
    return true;
}
/* Python numeric-literal underscores: every '_' BETWEEN two digits. */
static bool stz_underscores_ok(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '_') {
            if (i == 0 || i + 1 >= n) return false;
            if (!(s[i - 1] >= '0' && s[i - 1] <= '9')) return false;
            if (!(s[i + 1] >= '0' && s[i + 1] <= '9')) return false;
        }
    }
    return true;
}
/* int(v) for a JSON value (ASCII int-strip set == C isspace). On failure sets
 * *kind and builds *msg (caller frees). Truncates toward zero; bool -> 0/1. */
static long long stz_py_int(const json_t *v, const char **kind, char **msg) {
    *kind = NULL; *msg = NULL;
    if (!v || v->type == JSON_NULL) {
        *kind = "TypeError";
        *msg = strdup("int() argument must be a string, a bytes-like object or a real number, not 'NoneType'");
        return 0;
    }
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (isinf(d)) { *kind = "OverflowError"; *msg = strdup("cannot convert float infinity to integer"); return 0; }
        if (isnan(d)) { *kind = "ValueError"; *msg = strdup("cannot convert float NaN to integer"); return 0; }
        return (long long)d;
    }
    if (v->type == JSON_STRING) {
        const char *raw = v->str_val ? v->str_val : "";
        const char *s = raw;
        while (*s && isspace((unsigned char)*s)) s++;
        const char *end = s + strlen(s);
        while (end > s && isspace((unsigned char)end[-1])) end--;
        size_t n = (size_t)(end - s);
        bool ok = stz_underscores_ok(s, n);
        const char *p = s;
        bool neg = false;
        if (ok && p < end && (*p == '+' || *p == '-')) { neg = (*p == '-'); p++; }
        long long acc = 0; bool any = false;
        if (ok) {
            for (; p < end; p++) {
                char c = *p;
                if (c == '_') continue;
                if (c < '0' || c > '9') { ok = false; break; }
                acc = acc * 10 + (c - '0');
                any = true;
            }
        }
        if (!ok || !any) {
            *kind = "ValueError";
            size_t rl = strlen(raw);
            char *q = (char *)malloc(rl + 64);
            if (q) {
                strcpy(q, "invalid literal for int() with base 10: '");
                size_t k = strlen(q);
                for (size_t i = 0; i < rl; i++) {
                    char c = raw[i];
                    if (c == '\n') { q[k++] = '\\'; q[k++] = 'n'; }
                    else if (c == '\r') { q[k++] = '\\'; q[k++] = 'r'; }
                    else if (c == '\t') { q[k++] = '\\'; q[k++] = 't'; }
                    else q[k++] = c;
                }
                q[k++] = '\''; q[k] = '\0';
                *msg = q;
            }
            return 0;
        }
        return neg ? -acc : acc;
    }
    *kind = "TypeError";
    {
        char buf[160];
        snprintf(buf, sizeof(buf),
                 "int() argument must be a string, a bytes-like object or a real number, not '%s'",
                 stz_typename(v));
        *msg = strdup(buf);
    }
    return 0;
}
/* json.dumps(s) for a str (ensure_ascii=True, ASCII domain): surrounding quotes,
 * \b \f \n \r \t short forms, other <0x20 OR ==0x7f -> \u00xx, " and \ escaped.
 * malloc'd. */
static char *stz_json_dumps_str(const char *s) {
    if (!s) s = "";
    size_t need = 2;
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\' || c == '\b' || c == '\f' || c == '\n' || c == '\r' || c == '\t')
            need += 2;
        else if (c < 0x20 || c >= 0x7f) need += 6;
        else need += 1;
    }
    char *out = (char *)malloc(need + 1);
    if (!out) return NULL;
    char *q = out;
    *q++ = '"';
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"':  *q++ = '\\'; *q++ = '"'; break;
            case '\\': *q++ = '\\'; *q++ = '\\'; break;
            case '\b': *q++ = '\\'; *q++ = 'b'; break;
            case '\f': *q++ = '\\'; *q++ = 'f'; break;
            case '\n': *q++ = '\\'; *q++ = 'n'; break;
            case '\r': *q++ = '\\'; *q++ = 'r'; break;
            case '\t': *q++ = '\\'; *q++ = 't'; break;
            default:
                if (c < 0x20 || c >= 0x7f) q += snprintf(q, 7, "\\u%04x", c);
                else *q++ = (char)c;
        }
    }
    *q++ = '"';
    *q = '\0';
    return out;
}
/* first n codepoints (ASCII domain: bytes). malloc'd. */
static char *stz_slice(const char *s, size_t n) {
    if (!s) s = "";
    size_t len = strlen(s);
    if (len > n) len = n;
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}
/* s.split(sep, 1)[0] — prefix before the first sep (whole s if absent). */
static char *stz_split1_first(const char *s, const char *sep) {
    if (!s) s = "";
    const char *hit = strstr(s, sep);
    size_t len = hit ? (size_t)(hit - s) : strlen(s);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}
/* s.split(sep, 1)[-1] — suffix after the first sep (whole s if absent). */
static char *stz_split1_last(const char *s, const char *sep) {
    if (!s) s = "";
    const char *hit = strstr(s, sep);
    const char *start = hit ? hit + strlen(sep) : s;
    return strdup(start);
}
/* AttributeError "'<T>' object has no attribute '<name>'". */
static json_t *stz_attr_raise(const json_t *obj, const char *name) {
    char buf[160];
    snprintf(buf, sizeof(buf), "'%s' object has no attribute '%s'", stz_typename(obj), name);
    return stz_raise("AttributeError", buf);
}
/* py3.14 context-manager protocol TypeError for a non-CM handle. */
static json_t *stz_cm_protocol_raise(const json_t *handle) {
    char buf[192];
    snprintf(buf, sizeof(buf),
             "'%s' object does not support the context manager protocol (missed __exit__ method)",
             stz_typename(handle));
    return stz_raise("TypeError", buf);
}
/* logger.<level>(fmt, *args): {fmt, args?}; empty positional args omit "args". */
static json_t *stz_log(const stz_seams_t *s, const char *level, const char *fmt, json_t *args_arr) {
    json_t *a = stz_args();
    stz_s(a, "fmt", fmt);
    if (args_arr && json_len(args_arr) > 0) json_set(a, "args", args_arr);
    else json_free(args_arr);
    char name[32];
    snprintf(name, sizeof(name), "logger.%s", level);
    return stz_call(s, name, a);
}
static json_t *stz_args1s(const char *v) {
    json_t *x = json_array();
    json_append(x, json_string(v ? v : ""));
    return x;
}
/* project a raise spec to {__raise__, msg} (the RecLogger exception-arg shape). */
static json_t *stz_exc_proj(const json_t *r) {
    json_t *o = json_object();
    stz_s(o, "__raise__", stz_kind(r));
    stz_s(o, "msg", stz_emsg(r));
    return o;
}

/* ── exported flow cores (forward declarations) ── */
static json_t *stz_env_str(const json_t *env, const char *key, const stz_seams_t *s);
static json_t *stz_self_suspend_available(const json_t *env, const stz_seams_t *s);
static json_t *stz_brokered_sleep_url(const json_t *env, json_t *state, const stz_seams_t *s);
static json_t *stz_suspend_available(const json_t *env, json_t *state, const stz_seams_t *s);
static json_t *stz_request_brokered_suspend(const char *url, double timeout,
                                            const json_t *opener, const stz_seams_t *s);
static json_t *stz_suspend_self(const json_t *env, const char *socket_path,
                                double timeout, const stz_seams_t *s);

/* resolve `(os.environ if env is None else env)`; returns an OWNED node (may be
 * a raise spec). *raised set when the os.environ seam raised. */
static json_t *stz_resolve_env(const json_t *env, const stz_seams_t *s) {
    if (stz_is_null(env)) return stz_call(s, "os.environ", stz_args());
    return json_copy(env);
}
/* E.get(key, "") then str(...).strip(); E must be an object else AttributeError.
 * Returns an owned string node or a raise spec. */
static json_t *stz_env_get_str(const json_t *E, const char *key) {
    if (!stz_is_obj(E)) return stz_attr_raise(E, "get");
    const json_t *v = json_obj_get(E, key);
    char *raw = v ? stz_pystr(v) : strdup("");
    char *stripped = stz_strip(raw);
    free(raw);
    json_t *r = json_string(stripped ? stripped : "");
    free(stripped);
    return r;
}

/* PoP: stz_env_str @ gateway/scale_to_zero.py:_env_str */
static json_t *stz_env_str(const json_t *env, const char *key, const stz_seams_t *s) {
    json_t *E = stz_resolve_env(env, s);
    if (stz_raised(E)) return E;
    json_t *r = stz_env_get_str(E, key ? key : "");
    json_free(E);
    return r;
}

/* PoP: stz_self_suspend_available @ gateway/scale_to_zero.py:self_suspend_available */
static json_t *stz_self_suspend_available(const json_t *env, const stz_seams_t *s) {
    json_t *app = stz_env_str(env, "FLY_APP_NAME", s);
    if (stz_raised(app)) return app;
    if (!stz_truth(app)) { json_free(app); return json_bool(false); }
    json_free(app);
    json_t *machine = stz_env_str(env, "FLY_MACHINE_ID", s);
    if (stz_raised(machine)) return machine;
    if (!stz_truth(machine)) { json_free(machine); return json_bool(false); }
    json_free(machine);
    json_t *a = stz_args();
    stz_s(a, "path", "/.fly/api");
    json_t *ex = stz_call(s, "os.path.exists", a);
    if (stz_raised(ex)) return ex;
    json_t *r = json_bool(stz_truth(ex));
    json_free(ex);
    return r;
}

/* PoP: stz_brokered_sleep_url @ gateway/scale_to_zero.py:brokered_sleep_url */
static json_t *stz_brokered_sleep_url(const json_t *env, json_t *state, const stz_seams_t *s) {
    json_t *E = stz_resolve_env(env, s);
    if (stz_raised(E)) return E;
    if (!stz_is_obj(E)) { json_t *r = stz_attr_raise(E, "get"); json_free(E); return r; }
    const json_t *v = json_obj_get(E, "GATEWAY_RELAY_SLEEP_URL");
    char *raw = v ? stz_pystr(v) : strdup("");
    char *url = stz_strip(raw);
    free(raw);
    json_free(E);
    if (!url || !url[0]) { free(url); return json_null(); }
    json_t *a = stz_args();
    stz_s(a, "url", url);
    json_t *parsed = stz_call(s, "urlsplit", a);
    if (stz_raised(parsed)) { free(url); return parsed; }
    if (!stz_is_obj(parsed)) {
        json_t *r = stz_attr_raise(parsed, "scheme");
        json_free(parsed); free(url);
        return r;
    }
    const json_t *scheme = json_obj_get(parsed, "scheme");
    const json_t *netloc = json_obj_get(parsed, "netloc");
    const char *scheme_s = stz_strval(scheme);
    bool malformed = (strcmp(scheme_s ? scheme_s : "", "https") != 0) || !stz_truth(netloc);
    json_free(parsed);
    if (malformed) {
        if (!json_get_bool(state, "malformed_logged", false)) {
            json_set(state, "malformed_logged", json_bool(true));
            json_t *r = stz_log(s, "warning",
                "scale-to-zero: ignoring malformed %s (want an absolute https URL)",
                stz_args1s("GATEWAY_RELAY_SLEEP_URL"));
            if (stz_raised(r)) { free(url); return r; }
            json_free(r);
        }
        free(url);
        return json_null();
    }
    json_t *r = json_string(url);
    free(url);
    return r;
}

/* PoP: stz_suspend_available @ gateway/scale_to_zero.py:suspend_available */
static json_t *stz_suspend_available(const json_t *env, json_t *state, const stz_seams_t *s) {
    json_t *E = stz_resolve_env(env, s);
    if (stz_raised(E)) return E;
    json_t *ssa = stz_self_suspend_available(E, s);
    if (stz_raised(ssa)) { json_free(E); return ssa; }
    if (stz_truth(ssa)) { json_free(ssa); json_free(E); return json_bool(true); }
    json_free(ssa);
    json_t *bsu = stz_brokered_sleep_url(E, state, s);
    json_free(E);
    if (stz_raised(bsu)) return bsu;
    json_t *r = json_bool(!stz_is_null(bsu));
    json_free(bsu);
    return r;
}

/* CM exit protocol: given the body raise (owned, may be NULL) and the handle
 * role, fire "<role>.__exit__" {"exc": proj|null}; return the pending raise
 * (owned, may be NULL) after applying suppress/override. Frees body_raise. */
static json_t *stz_cm_exit(const stz_seams_t *s, const char *role, json_t *body_raise) {
    json_t *a = stz_args();
    if (body_raise) json_set(a, "exc", stz_exc_proj(body_raise));
    else json_set(a, "exc", json_null());
    char name[64];
    snprintf(name, sizeof(name), "%s.__exit__", role);
    json_t *xr = stz_call(s, name, a);
    if (stz_raised(xr)) { json_free(body_raise); return xr; }   /* exit raise overrides */
    if (stz_truth(xr)) { json_free(xr); json_free(body_raise); return NULL; } /* suppress */
    json_free(xr);
    return body_raise;                                          /* body raise propagates */
}

/* PoP: stz_request_brokered_suspend @ gateway/scale_to_zero.py:request_brokered_suspend */
static json_t *stz_request_brokered_suspend(const char *url, double timeout,
                                            const json_t *opener, const stz_seams_t *s) {
    /* request = urllib.request.Request(url, data=b"", method="POST")  [outside try] */
    json_t *a = stz_args();
    stz_s(a, "url", url ? url : "");
    stz_s(a, "data", "");
    stz_s(a, "method", "POST");
    json_t *request = stz_call(s, "urllib.request.Request", a);
    if (stz_raised(request)) return request;
    /* open_url = opener or urllib.request.urlopen */
    const char *open_name = stz_truth(opener) ? "opener" : "urlopen";
    a = stz_args();
    stz_j(a, "request", request);
    stz_n(a, "timeout", timeout);
    json_free(request);
    json_t *handle = stz_call(s, open_name, a);
    json_t *pending = NULL;
    long long status = 0;
    bool status_assigned = false;
    if (stz_raised(handle)) {
        pending = handle;                           /* call raise (inside try): no enter/exit */
    } else if (!stz_is_obj(handle)) {
        pending = stz_cm_protocol_raise(handle);    /* non-CM protocol TypeError (inside try) */
        json_free(handle);
    } else {
        /* __enter__ returns self; script gates raise only */
        json_t *ent = stz_call(s, "response.__enter__", stz_args());
        if (stz_raised(ent)) {
            pending = ent;                          /* enter raise: no exit */
            json_free(handle);
        } else {
            json_free(ent);
            /* body: status = int(getattr(response, "status", 0) or 0) */
            json_t *body_raise = NULL;
            const json_t *sv = stz_oget(handle, "status");
            if (!stz_truth(sv)) { status = 0; status_assigned = true; }
            else {
                const char *kind = NULL; char *msg = NULL;
                status = stz_py_int(sv, &kind, &msg);
                if (kind) { body_raise = stz_raise(kind, msg); free(msg); }
                else status_assigned = true;
            }
            json_free(handle);
            pending = stz_cm_exit(s, "response", body_raise);
        }
    }
    if (pending) {
        const char *k = stz_kind(pending);
        if (stz_is_httperror(k)) {
            const json_t *code = stz_oget(pending, "code");
            const json_t *reason = stz_oget(pending, "reason");
            json_t *aa = json_array();
            json_append(aa, code ? json_copy(code) : json_null());
            json_append(aa, reason ? json_copy(reason) : json_null());
            json_t *r = stz_log(s, "warning",
                "scale-to-zero: brokered suspend rejected: %s %s", aa);
            json_free(pending);
            if (stz_raised(r)) return r;
            json_free(r);
            return json_bool(false);
        }
        if (stz_is_oserror(k) || stz_is_valueerror(k)) {
            json_t *aa = json_array();
            json_append(aa, stz_exc_proj(pending));
            json_t *r = stz_log(s, "warning",
                "scale-to-zero: brokered suspend request failed: %s", aa);
            json_free(pending);
            if (stz_raised(r)) return r;
            json_free(r);
            return json_bool(false);
        }
        return pending;                             /* propagate (incl BaseException) */
    }
    if (!status_assigned)
        return stz_raise("UnboundLocalError",
            "cannot access local variable 'status' where it is not associated with a value");
    bool ok = (status >= 200 && status < 300);
    json_t *aa = json_array();
    json_append(aa, json_number((double)status));
    json_t *r = ok
        ? stz_log(s, "info", "scale-to-zero: machine suspend accepted by NAS (%s)", aa)
        : stz_log(s, "warning", "scale-to-zero: brokered suspend returned %s", aa);
    if (stz_raised(r)) return r;
    json_free(r);
    return json_bool(ok);
}

/* PoP: stz_suspend_self @ gateway/scale_to_zero.py:suspend_self */
static json_t *stz_suspend_self(const json_t *env, const char *socket_path,
                                double timeout, const stz_seams_t *s) {
    /* app, machine_id = _env_str(...), _env_str(...)  [both evaluated] */
    json_t *app = stz_env_str(env, "FLY_APP_NAME", s);
    if (stz_raised(app)) return app;
    json_t *machine = stz_env_str(env, "FLY_MACHINE_ID", s);
    if (stz_raised(machine)) { json_free(app); return machine; }
    if (!stz_truth(app) || !stz_truth(machine)) {
        json_free(app); json_free(machine);
        json_t *r = stz_log(s, "warning",
            "scale-to-zero: suspend_self called without Fly machine identity", NULL);
        if (stz_raised(r)) return r;
        json_free(r);
        return json_bool(false);
    }
    const char *app_s = stz_strval(app);
    const char *machine_s = stz_strval(machine);
    size_t rlen = strlen(app_s) + strlen(machine_s) + 160;
    char *request = (char *)malloc(rlen);
    if (!request) { json_free(app); json_free(machine); return stz_raise("MemoryError", ""); }
    snprintf(request, rlen,
        "POST /v1/apps/%s/machines/%s/suspend HTTP/1.1\r\n"
        "Host: flaps\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",
        app_s, machine_s);
    json_free(app); json_free(machine);
    const char *spath = (socket_path && socket_path[0]) ? socket_path : "/.fly/api";

    /* try: with socket.socket(AF_UNIX, SOCK_STREAM) as sock: ... */
    json_t *a = stz_args();
    stz_n(a, "family", 1);
    stz_n(a, "type", 1);
    json_t *handle = stz_call(s, "socket.socket", a);
    json_t *pending = NULL;
    char *response = NULL;
    bool response_assigned = false;
    if (stz_raised(handle)) {
        pending = handle;                           /* ctor raise: no enter/exit */
    } else if (!stz_is_obj(handle)) {
        pending = stz_cm_protocol_raise(handle);    /* protocol TypeError (in try) */
        json_free(handle);
    } else {
        json_t *ent = stz_call(s, "sock.__enter__", stz_args());
        if (stz_raised(ent)) {
            pending = ent;                          /* enter raise: no exit */
            json_free(handle);
        } else {
            json_free(ent);
            json_t *body_raise = NULL;
            response = strdup("");
            /* sock.settimeout(timeout) */
            a = stz_args(); stz_n(a, "timeout", timeout);
            json_t *r = stz_call(s, "sock.settimeout", a);
            if (stz_raised(r)) body_raise = r; else json_free(r);
            /* sock.connect(socket_path) */
            if (!body_raise) {
                a = stz_args(); stz_s(a, "path", spath);
                r = stz_call(s, "sock.connect", a);
                if (stz_raised(r)) body_raise = r; else json_free(r);
            }
            /* sock.sendall(request.encode("ascii")) */
            if (!body_raise) {
                a = stz_args(); stz_s(a, "data", request);
                r = stz_call(s, "sock.sendall", a);
                if (stz_raised(r)) body_raise = r; else json_free(r);
            }
            /* response = b""; while len(response) < 65536 and (chunk := recv(4096)): */
            if (!body_raise) {
                response_assigned = true;
                while (strlen(response) < 65536) {
                    a = stz_args(); stz_n(a, "size", 4096);
                    json_t *chunk = stz_call(s, "sock.recv", a);
                    if (stz_raised(chunk)) { body_raise = chunk; break; }
                    if (!stz_truth(chunk)) { json_free(chunk); break; }
                    if (!stz_is_str(chunk)) {
                        char buf[96];
                        snprintf(buf, sizeof(buf), "can't concat %s to bytes", stz_typename(chunk));
                        body_raise = stz_raise("TypeError", buf);
                        json_free(chunk);
                        break;
                    }
                    const char *cs = stz_strval(chunk);
                    size_t old = strlen(response), cl = strlen(cs);
                    char *nr = (char *)realloc(response, old + cl + 1);
                    if (!nr) { body_raise = stz_raise("MemoryError", ""); json_free(chunk); break; }
                    response = nr;
                    memcpy(response + old, cs, cl + 1);
                    json_free(chunk);
                }
            }
            json_free(handle);
            pending = stz_cm_exit(s, "sock", body_raise);
        }
    }
    free(request);
    if (pending) {
        const char *k = stz_kind(pending);
        if (stz_is_oserror(k)) {
            json_t *aa = json_array();
            json_append(aa, stz_exc_proj(pending));
            json_t *r = stz_log(s, "warning",
                "scale-to-zero: flaps suspend request failed: %s", aa);
            json_free(pending); free(response);
            if (stz_raised(r)) return r;
            json_free(r);
            return json_bool(false);
        }
        free(response);
        return pending;                             /* propagate (incl BaseException) */
    }
    if (!response_assigned) {
        free(response);
        return stz_raise("UnboundLocalError",
            "cannot access local variable 'response' where it is not associated with a value");
    }
    /* status_line = response.split(b"\r\n", 1)[0].decode("ascii", "replace") */
    char *status_line = stz_split1_first(response, "\r\n");
    json_t *parts = stz_split_ws(status_line);
    bool ok = false;
    if (json_len(parts) >= 2) {
        const json_t *p1 = json_get(parts, 1);
        const char *p1s = stz_strval(p1);
        if (stz_isdigit(p1s)) {
            const char *kind = NULL; char *msg = NULL;
            long long code = stz_py_int(p1, &kind, &msg);
            free(msg);
            if (!kind) ok = (code >= 200 && code < 300);
        }
    }
    json_free(parts);
    json_t *r;
    if (ok) {
        r = stz_log(s, "info", "scale-to-zero: machine suspend accepted by flaps (%s)",
                    stz_args1s(status_line));
    } else {
        char *body = stz_split1_last(response, "\r\n\r\n");
        char *body500 = stz_slice(body, 500);
        free(body);
        char *jd = stz_json_dumps_str(body500);
        free(body500);
        char *jd500 = stz_slice(jd, 500);
        free(jd);
        json_t *aa = json_array();
        json_append(aa, json_string(status_line));
        json_append(aa, json_string(jd500 ? jd500 : ""));
        free(jd500);
        r = stz_log(s, "warning", "scale-to-zero: flaps suspend rejected: %s %s", aa);
    }
    free(status_line);
    free(response);
    if (stz_raised(r)) return r;
    json_free(r);
    return json_bool(ok);
}
