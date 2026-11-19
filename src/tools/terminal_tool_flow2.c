/*
 * terminal_tool_flow2.c — tools/terminal_tool.py UNCLAIMED-remainder FLOW cores
 * (universal-seam convention, sibling of terminal_tool_lifecycle_flow.c).
 *
 * Ports the DECISION SEQUENCES of the profile/session keying cluster
 * (_container_profile, _routed_home_task_key, _sessionless_container_key,
 * _session_scope, _shell_state_chain, _workspace_container_key,
 * workspace_key_for), the cwd sanitizers / visibility helpers
 * (_sanitize_cwd_for_live_env, _backend_default_cwd, _resolve_config_cwd,
 * _rewrite_via_env_mount, _mount_envs, _container_visible_cwd,
 * _container_visible_default, _session_workspace_mount_source), the env-cache
 * lookup / acquisition (_lookup_active_env, _acquire_env), image selection
 * (_select_image), the fatal error envelope (_fatal_error_json) and the
 * yield-to-background kwargs binding (_yield_kwargs): call order,
 * short-circuit branches, early returns, exception->fallback paths and
 * mutation order of the live Python. Every live I/O (config reads, os.path
 * probes, clocks, logging, env creation, claimed siblings) enters through
 * tt2_seams_t.call(ctx, name, args); args is OWNED by tt2_call (freed after
 * the borrowed call); results are NEW json_t with NULL == Python None and
 * {"__raise__":K,"msg":M} == a raised exception. BaseException kinds
 * (SystemExit/KeyboardInterrupt/CancelledError/GeneratorExit) pass through
 * every except site (the only catches are `except OSError` in
 * _routed_home_task_key and `except ImportError` in _acquire_env; the
 * exception-parent table carries the FULL builtin hierarchy).
 *
 * Self-contained: libjson only (no cross-object symbols). All helpers static
 * tt2_*. SHA-256 is hand-rolled (workspace_key_for) — no libhash/libcrypto.
 *
 * Modeling notes (differential oracle tests/oracle_terminal_tool_flow2.py):
 *  - Module state is injected json_t (in.state) mutated in place and echoed
 *    as out.state with the FULL key set: active_environments, last_activity,
 *    task_env_overrides, container_aliases, creation_locks, os_name.
 *    active_environments values are env attribute doubles (objects read via
 *    getattr-with-default semantics: absent key -> None). creation_locks
 *    values are presence markers (true == a held threading.Lock).
 *  - Claimed siblings are seams: current_session_profile,
 *    ensure_terminal_env_bridged, resolve_container_alias, qualify_task_key,
 *    resolve_task_overrides, redact_terminal_error_text, error_json,
 *    start_cleanup_thread. Foreign leaves are seams: get_hermes_home_override,
 *    profile_name_for_home, is_routed_home, home_profile_name,
 *    profile_container_key, coerce_ssh_remote_cwd, is_container_backend,
 *    is_unusable_container_cwd, is_mounted_host_cwd, translate_mounted_host_path,
 *    tenv, tenv_bool, safe_getcwd, plugin_env_flag, is_host_cwd,
 *    is_windows_drive_path, is_ssh_remote_tilde_cwd, hosted_scope, admit_env,
 *    create_configured_env, yield_to_background_handler, time.time,
 *    os.path.{expanduser,abspath,isdir,isabs,realpath}, traceback.format_exc,
 *    logger.*.
 *  - Ported siblings run inline (real) on both sides: _routed_home_task_key
 *    inside _sessionless_container_key; _backend_default_cwd inside
 *    _resolve_config_cwd; _mount_envs / _rewrite_via_env_mount inside the
 *    visibility helpers; workspace_key_for inside _workspace_container_key;
 *    _lookup_active_env inside _acquire_env.
 *  - _session_scope returns the _SessionScope FIELD dict {env_type,persistent}
 *    (dataclasses.asdict projection); its derived properties are consumed only
 *    by claimed siblings (seamed). _acquire_env's plan is the _ExecPlan field
 *    dict; fixtures carry every read field.
 *  - Integral floats == ints (libjson has no int/float identity): fixtures
 *    keep str()-computed positions integral-int or non-integral, and
 *    json.dumps positions (_fatal_error_json) integral. Embedded NUL bytes
 *    are outside the fixture domain (C strings are NUL-terminated; the
 *    "\0" not in cwd guard is transcribed but never fires). KeyError str()
 *    repr-quoting is modeled for printable-ASCII messages without quotes /
 *    backslashes. _yield_kwargs ctx is always a mapping (the non-mapping **
 *    surface lives at the OUTER drive binding, outside the ported body).
 *  - Ordered collections: active_environments / task_env_overrides /
 *    container_aliases iterate in JSON insertion order (== dict order).
 *    dict.fromkeys((task_id, alias)) dedupes first-wins.
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
#include <stdarg.h>
#include "libjson/json.h"

typedef struct tt2_seams_t {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} tt2_seams_t;

/* ── universal-seam helpers ── */
static json_t *tt2_call(const tt2_seams_t *s, const char *name, json_t *args) {
    json_t *r = (s && s->call) ? s->call(s->ctx, name, args) : NULL;
    json_free(args);
    return r;
}
static bool tt2_raised(const json_t *r) {
    if (!r || r->type != JSON_OBJECT) return false;
    const json_t *k = json_obj_get(r, "__raise__");
    return k && k->type == JSON_STRING;
}
static const char *tt2_kind(const json_t *r) {
    const json_t *k = json_obj_get(r, "__raise__");
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static const char *tt2_emsg(const json_t *r) {
    const json_t *k = json_obj_get(r, "msg");
    return (k && k->type == JSON_STRING) ? k->str_val : "";
}
static json_t *tt2_raise(const char *kind, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(kind));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static json_t *tt2_args(void) { return json_object(); }
static void tt2_s(json_t *a, const char *k, const char *v) {
    json_set(a, k, v ? json_string(v) : json_null());
}
static void tt2_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void tt2_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }
static void tt2_j(json_t *a, const char *k, const json_t *v) {
    json_set(a, k, v ? json_copy(v) : json_null());
}
static const json_t *tt2_oget(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

/* ── exception-parent table (FULL builtin hierarchy) ──
 * Unknown kinds default to Exception (the oracle's make_exc builds
 * type(kind, (Exception,), {})); the BaseException trio + CancelledError
 * stay outside Exception so they pass through every except site. */
typedef struct { const char *kind; const char *parent; } tt2_exc_ent;
static const tt2_exc_ent TT2_EXC_PARENTS[] = {
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"CancelledError", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
    {"Exception", "BaseException"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"AttributeError", "Exception"},
    {"NameError", "Exception"},
    {"LookupError", "Exception"},
    {"ArithmeticError", "Exception"},
    {"OSError", "Exception"},
    {"EnvironmentError", "OSError"},
    {"IOError", "OSError"},
    {"StopIteration", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"AssertionError", "Exception"},
    {"SyntaxError", "Exception"},
    {"SystemError", "Exception"},
    {"ImportError", "Exception"},
    {"Warning", "Exception"},
    {"RuntimeError", "Exception"},
    {"MemoryError", "Exception"},
    {"BufferError", "Exception"},
    {"EOFError", "Exception"},
    {"ReferenceError", "Exception"},
    {"ExceptionGroup", "Exception"},
    {"_Rejected", "Exception"},
    {"NotImplementedError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"UnboundLocalError", "NameError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"FloatingPointError", "ArithmeticError"},
    {"FileNotFoundError", "OSError"},
    {"PermissionError", "OSError"},
    {"IsADirectoryError", "OSError"},
    {"NotADirectoryError", "OSError"},
    {"InterruptedError", "OSError"},
    {"TimeoutError", "OSError"},
    {"ConnectionError", "OSError"},
    {"ProcessLookupError", "OSError"},
    {"FileExistsError", "OSError"},
    {"ChildProcessError", "OSError"},
    {"BlockingIOError", "OSError"},
    {"BrokenPipeError", "ConnectionError"},
    {"ConnectionAbortedError", "ConnectionError"},
    {"ConnectionRefusedError", "ConnectionError"},
    {"ConnectionResetError", "ConnectionError"},
    {"ModuleNotFoundError", "ImportError"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"UserWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"BytesWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {NULL, NULL},
};
static const char *tt2_exc_parent(const char *kind) {
    for (const tt2_exc_ent *e = TT2_EXC_PARENTS; e->kind; e++)
        if (strcmp(e->kind, kind) == 0) return e->parent;
    if (strcmp(kind, "BaseException") == 0) return NULL;
    return "Exception";   /* oracle make_exc: unknown -> Exception subclass */
}
static bool tt2_exc_is(const char *kind, const char *base) {
    const char *k = kind;
    for (int depth = 0; k && depth < 32; depth++) {
        if (strcmp(k, base) == 0) return true;
        k = tt2_exc_parent(k);
    }
    return false;
}

/* ── Python value helpers ── */
static bool tt2_truth(const json_t *v) {
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
static bool tt2_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }
static bool tt2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static const char *tt2_strval(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static const char *tt2_typename(const json_t *v) {
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
/* str() of a scalar (int/bool/str/None). Non-integral floats are outside the
 * fixture domain for str()-computed positions (%.17g fallback). */
static char *tt2_pystr(const json_t *v) {
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
    return strdup("");   /* containers outside the str() domain */
}
/* str(e) from a raise descriptor. KeyError repr-quoting applies to its single
 * arg (printable-ASCII msg domain, no quotes/backslashes). */
static char *tt2_exc_str(const json_t *spec) {
    const char *kind = tt2_kind(spec);
    const char *msg = tt2_emsg(spec);
    if (strcmp(kind, "KeyError") == 0) {
        size_t n = strlen(msg);
        char *q = malloc(n + 3);
        if (q) { q[0] = '\''; memcpy(q + 1, msg, n); q[n + 1] = '\''; q[n + 2] = '\0'; }
        return q ? q : strdup("");
    }
    return strdup(msg);
}
/* malloc'd sprintf */
static char *tt2_fmt(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) return strdup("");
    char *buf = malloc((size_t)n + 1);
    if (!buf) return strdup("");
    va_start(ap, fmt);
    vsnprintf(buf, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return buf;
}
static bool tt2_streq(const char *a, const char *b) {
    return strcmp(a ? a : "", b ? b : "") == 0;
}
static bool tt2_startswith(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}
/* str.strip() truthiness: any non-White_Space codepoint (full Unicode set). */
static bool tt2_nonws(const char *s) {
    if (!s) return false;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        uint32_t cp;
        unsigned char c = *p;
        if (c < 0x80) { cp = c; p += 1; }
        else if ((c & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
            cp = ((uint32_t)(c & 0x1F) << 6) | (p[1] & 0x3F); p += 2;
        } else if ((c & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
            cp = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (p[2] & 0x3F); p += 3;
        } else if ((c & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
            cp = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(p[1] & 0x3F) << 12) | ((uint32_t)(p[2] & 0x3F) << 6) | (p[3] & 0x3F); p += 4;
        } else { cp = c; p += 1; }
        bool ws = (cp >= 0x09 && cp <= 0x0D) || cp == 0x20 || cp == 0x85 || cp == 0xA0
               || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200A)
               || cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F || cp == 0x3000;
        if (!ws) return true;
    }
    return false;
}
/* "\0" in cwd — transcribed; embedded NUL is outside the C string domain. */
static bool tt2_has_nul(const char *s) {
    if (!s) return false;
    return memchr(s, '\0', strlen(s)) != NULL;   /* always false: strlen stops at NUL */
}

/* ── SHA-256 (workspace_key_for; hand-rolled, libjson-only port) ── */
static uint32_t tt2_rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }
static void tt2_sha256(const unsigned char *data, size_t len, unsigned char out[32]) {
    static const uint32_t K[64] = {
        0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
        0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
        0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
        0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
        0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
        0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
        0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
        0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u,
    };
    uint32_t h[8] = {
        0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u,
    };
    size_t total = len + 1 + 8;
    size_t padded = ((total + 63) / 64) * 64;
    unsigned char *msg = calloc(padded, 1);
    if (!msg) { memset(out, 0, 32); return; }
    memcpy(msg, data, len);
    msg[len] = 0x80;
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) msg[padded - 1 - i] = (unsigned char)(bits >> (8 * i));
    for (size_t off = 0; off < padded; off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)msg[off + 4*i] << 24) | ((uint32_t)msg[off + 4*i + 1] << 16)
                 | ((uint32_t)msg[off + 4*i + 2] << 8) | (uint32_t)msg[off + 4*i + 3];
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = tt2_rotr(w[i-15], 7) ^ tt2_rotr(w[i-15], 18) ^ (w[i-15] >> 3);
            uint32_t s1 = tt2_rotr(w[i-2], 17) ^ tt2_rotr(w[i-2], 19) ^ (w[i-2] >> 10);
            w[i] = w[i-16] + s0 + w[i-7] + s1;
        }
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = tt2_rotr(e,6) ^ tt2_rotr(e,11) ^ tt2_rotr(e,25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t t1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = tt2_rotr(a,2) ^ tt2_rotr(a,13) ^ tt2_rotr(a,22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + maj;
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }
    free(msg);
    for (int i = 0; i < 8; i++) {
        out[4*i]   = (unsigned char)(h[i] >> 24);
        out[4*i+1] = (unsigned char)(h[i] >> 16);
        out[4*i+2] = (unsigned char)(h[i] >> 8);
        out[4*i+3] = (unsigned char)h[i];
    }
}
static char *tt2_sha256_hex16(const char *s) {
    unsigned char dig[32];
    tt2_sha256((const unsigned char *)(s ? s : ""), strlen(s ? s : ""), dig);
    char *hex = malloc(17);
    if (!hex) return strdup("");
    static const char H[] = "0123456789abcdef";
    for (int i = 0; i < 8; i++) {
        hex[2*i]   = H[dig[i] >> 4];
        hex[2*i+1] = H[dig[i] & 0xF];
    }
    hex[16] = '\0';
    return hex;
}

/* ── Python json.dumps(obj, ensure_ascii=False) ──
 * Insertion-order keys, ", " / ": " separators, ESCAPE control chars only
 * (\b \f \n \r \t short forms, other <0x20 as \u00xx), non-ASCII passthrough.
 * Numbers: integral -> int form; non-integral floats are outside the fixture
 * domain for dumps positions (%.17g fallback). */
static void tt2_dump_str(char **buf, size_t *len, size_t *cap, const char *s) {
    size_t n = strlen(s);
    if (*len + n * 6 + 8 > *cap) {
        *cap = (*len + n * 6 + 8) * 2;
        *buf = realloc(*buf, *cap);
    }
    char *b = *buf + *len;
    size_t k = 0;
    b[k++] = '"';
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"':  b[k++] = '\\'; b[k++] = '"'; break;
            case '\\': b[k++] = '\\'; b[k++] = '\\'; break;
            case '\b': b[k++] = '\\'; b[k++] = 'b'; break;
            case '\f': b[k++] = '\\'; b[k++] = 'f'; break;
            case '\n': b[k++] = '\\'; b[k++] = 'n'; break;
            case '\r': b[k++] = '\\'; b[k++] = 'r'; break;
            case '\t': b[k++] = '\\'; b[k++] = 't'; break;
            default:
                if (c < 0x20) {
                    static const char H[] = "0123456789abcdef";
                    b[k++] = '\\'; b[k++] = 'u'; b[k++] = '0'; b[k++] = '0';
                    b[k++] = H[c >> 4]; b[k++] = H[c & 0xF];
                } else b[k++] = (char)c;
        }
    }
    b[k++] = '"';
    *len += k;
    (*buf)[*len] = '\0';
}
static void tt2_dump_node(char **buf, size_t *len, size_t *cap, const json_t *v) {
    char tmp[64];
    if (!v || v->type == JSON_NULL) {
        if (*len + 8 > *cap) { *cap = (*len + 8) * 2; *buf = realloc(*buf, *cap); }
        memcpy(*buf + *len, "null", 4); *len += 4; (*buf)[*len] = '\0';
        return;
    }
    switch (v->type) {
        case JSON_BOOL: {
            const char *s = v->bool_val ? "true" : "false";
            size_t n = strlen(s);
            if (*len + n + 1 > *cap) { *cap = (*len + n + 1) * 2; *buf = realloc(*buf, *cap); }
            memcpy(*buf + *len, s, n); *len += n; (*buf)[*len] = '\0';
            break;
        }
        case JSON_NUMBER: {
            double d = v->num_val;
            if (!isinf(d) && !isnan(d) && d == (double)(long long)d)
                snprintf(tmp, sizeof(tmp), "%lld", (long long)d);
            else
                snprintf(tmp, sizeof(tmp), "%.17g", d);
            size_t n = strlen(tmp);
            if (*len + n + 1 > *cap) { *cap = (*len + n + 1) * 2; *buf = realloc(*buf, *cap); }
            memcpy(*buf + *len, tmp, n); *len += n; (*buf)[*len] = '\0';
            break;
        }
        case JSON_STRING:
            tt2_dump_str(buf, len, cap, v->str_val ? v->str_val : "");
            break;
        case JSON_ARRAY: {
            if (*len + 2 > *cap) { *cap = (*len + 2) * 2; *buf = realloc(*buf, *cap); }
            (*buf)[(*len)++] = '[';
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) {
                    if (*len + 2 > *cap) { *cap = (*len + 2) * 2; *buf = realloc(*buf, *cap); }
                    (*buf)[(*len)++] = ','; (*buf)[(*len)++] = ' ';
                }
                tt2_dump_node(buf, len, cap, v->c.items[i]);
            }
            if (*len + 2 > *cap) { *cap = (*len + 2) * 2; *buf = realloc(*buf, *cap); }
            (*buf)[(*len)++] = ']'; (*buf)[*len] = '\0';
            break;
        }
        case JSON_OBJECT: {
            if (*len + 2 > *cap) { *cap = (*len + 2) * 2; *buf = realloc(*buf, *cap); }
            (*buf)[(*len)++] = '{';
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) {
                    if (*len + 2 > *cap) { *cap = (*len + 2) * 2; *buf = realloc(*buf, *cap); }
                    (*buf)[(*len)++] = ','; (*buf)[(*len)++] = ' ';
                }
                tt2_dump_str(buf, len, cap, v->c.keys[i]);
                if (*len + 2 > *cap) { *cap = (*len + 2) * 2; *buf = realloc(*buf, *cap); }
                (*buf)[(*len)++] = ':'; (*buf)[(*len)++] = ' ';
                tt2_dump_node(buf, len, cap, v->c.items[i]);
            }
            if (*len + 2 > *cap) { *cap = (*len + 2) * 2; *buf = realloc(*buf, *cap); }
            (*buf)[(*len)++] = '}'; (*buf)[*len] = '\0';
            break;
        }
        default: break;
    }
}
static char *tt2_dumps(const json_t *v) {
    size_t cap = 128, len = 0;
    char *buf = malloc(cap);
    if (!buf) return strdup("");
    buf[0] = '\0';
    tt2_dump_node(&buf, &len, &cap, v);
    return buf;
}

/* ── logging seams (RecLogger twin) ──
 * logger.<level>(fmt, *args): {fmt, args?}; empty positional args omit the
 * "args" key. Returns the seam result (caller frees / propagates). */
static json_t *tt2_log(const tt2_seams_t *s, const char *level,
                       const char *fmt, json_t *args_arr) {
    json_t *a = tt2_args();
    tt2_s(a, "fmt", fmt);
    if (args_arr && json_len(args_arr) > 0) json_set(a, "args", args_arr);
    else json_free(args_arr);
    char name[32];
    snprintf(name, sizeof(name), "logger.%s", level);
    return tt2_call(s, name, a);
}
static json_t *tt2_log1(const tt2_seams_t *s, const char *level,
                        const char *fmt, const json_t *arg) {
    json_t *arr = json_array();
    json_append(arr, arg ? json_copy(arg) : json_null());
    return tt2_log(s, level, fmt, arr);
}

/* ── state helpers ── */
static const json_t *tt2_st_obj(const json_t *st, const char *key) {
    const json_t *o = tt2_oget(st, key);
    return (o && o->type == JSON_OBJECT) ? o : NULL;
}
static json_t *tt2_st_ensure(json_t *st, const char *key) {
    json_t *o = tt2_oget(st, key);
    if (o && o->type == JSON_OBJECT) return o;
    json_t *n = json_object();
    json_set(st, key, n);
    return n;
}
/* dict.get(k) on a state table: NULL when absent / table missing. */
static const json_t *tt2_table_get(const json_t *st, const char *table, const char *key) {
    const json_t *t = tt2_st_obj(st, table);
    if (!t || !key) return NULL;
    return json_obj_get(t, key);
}
/* getattr(env, name, None) on an env double: absent -> NULL (None). */
static const json_t *tt2_env_attr(const json_t *env, const char *name) {
    return tt2_oget(env, name);
}
/* .get on a seam result that live dereferences unguarded: a non-object
 * raises the live AttributeError BEFORE any downstream gate. */
static json_t *tt2_get_guard(const json_t *v) {
    if (v && v->type == JSON_OBJECT) return NULL;
    char buf[128];
    snprintf(buf, sizeof(buf), "'%s' object has no attribute 'get'", tt2_typename(v));
    return tt2_raise("AttributeError", buf);
}

/* ── exported flow cores (forward declarations) ── */
static json_t *tt2_container_profile(const tt2_seams_t *s);
static json_t *tt2_sanitize_cwd_for_live_env(const json_t *env, const char *new_cwd, const tt2_seams_t *s);
static json_t *tt2_shell_state_chain(const json_t *st, const char *task_id);
static json_t *tt2_session_scope(const tt2_seams_t *s);
static json_t *tt2_routed_home_task_key(bool profile_scoped, const tt2_seams_t *s);
static json_t *tt2_sessionless_container_key(bool profile_scoped, const tt2_seams_t *s);
static json_t *tt2_workspace_key_for(const char *cwd, const tt2_seams_t *s);
static json_t *tt2_workspace_container_key(const char *base, const char *task_id, json_t *st, const tt2_seams_t *s);
static json_t *tt2_select_image(const char *env_type, const json_t *overrides, const json_t *config);
static json_t *tt2_lookup_active_env(const char *eff, const char *task_id, json_t *st, const tt2_seams_t *s);
static json_t *tt2_session_workspace_mount_source(const char *task_id, const tt2_seams_t *s);
static json_t *tt2_backend_default_cwd(const char *env_type, const tt2_seams_t *s);
static json_t *tt2_resolve_config_cwd(const char *env_type, bool mount_docker_cwd, json_t *st, const tt2_seams_t *s);
static json_t *tt2_rewrite_via_env_mount(const char *path, const json_t *env, const tt2_seams_t *s);
static json_t *tt2_mount_envs(const json_t *env, const json_t *st);
static json_t *tt2_container_visible_cwd(const char *path, const char *env_type, const json_t *env,
                                         const json_t *st, const tt2_seams_t *s);
static json_t *tt2_container_visible_default(const char *default_cwd, const char *env_type, const json_t *env,
                                             const json_t *st, const tt2_seams_t *s);
static json_t *tt2_fatal_error_json(const json_t *exc_spec, const tt2_seams_t *s);
static json_t *tt2_acquire_env(const json_t *plan, const char *task_id, json_t *st, const tt2_seams_t *s);
static json_t *tt2_yield_kwargs(const char *command, const json_t *ctx, const tt2_seams_t *s);

/* PoP: tt2_container_profile @ tools/terminal_tool.py:_container_profile */
static json_t *tt2_container_profile(const tt2_seams_t *s) {
    json_t *override = tt2_call(s, "get_hermes_home_override", tt2_args());
    if (tt2_raised(override)) return override;
    json_t *session = tt2_call(s, "current_session_profile", tt2_args());
    if (tt2_raised(session)) { json_free(override); return session; }
    json_t *bound = NULL;
    if (tt2_truth(override)) {
        json_t *a = tt2_args();
        tt2_j(a, "home", override);
        bound = tt2_call(s, "profile_name_for_home", a);
        if (tt2_raised(bound)) { json_free(override); json_free(session); return bound; }
    }
    bool take = false;
    if (tt2_truth(bound)) {
        if (tt2_truth(session)) {
            take = !tt2_streq(tt2_strval(bound), tt2_strval(session));   /* bound != session */
        } else {
            json_t *a = tt2_args();
            tt2_j(a, "home", override);
            json_t *r = tt2_call(s, "is_routed_home", a);
            if (tt2_raised(r)) { json_free(override); json_free(session); json_free(bound); return r; }
            take = tt2_truth(r);
            json_free(r);
        }
        if (take) { json_free(override); json_free(session); return bound; }
    }
    json_free(bound);
    json_free(override);
    if (tt2_truth(session)) return session;   /* session or home_profile_name() */
    json_free(session);
    return tt2_call(s, "home_profile_name", tt2_args());
}

/* PoP: tt2_sanitize_cwd_for_live_env @ tools/terminal_tool.py:_sanitize_cwd_for_live_env */
static json_t *tt2_sanitize_cwd_for_live_env(const json_t *env, const char *new_cwd, const tt2_seams_t *s) {
    const json_t *env_type = tt2_env_attr(env, "env_type");
    json_t *a = tt2_args();
    tt2_s(a, "cwd", new_cwd);
    tt2_j(a, "env_type", env_type);
    json_t *cwd = tt2_call(s, "coerce_ssh_remote_cwd", a);
    if (tt2_raised(cwd)) return cwd;
    if (!tt2_truth(env_type)) return cwd;
    json_t *a2 = tt2_args();
    tt2_j(a2, "env_type", env_type);
    json_t *cb = tt2_call(s, "is_container_backend", a2);
    if (tt2_raised(cb)) { json_free(cwd); return cb; }
    if (!tt2_truth(cb)) { json_free(cb); return cwd; }
    json_free(cb);
    const json_t *host_mount = tt2_env_attr(env, "host_cwd");
    const json_t *mounted = (tt2_is_str(host_mount) && tt2_truth(host_mount)) ? host_mount : NULL;
    json_t *a3 = tt2_args();
    tt2_j(a3, "cwd", cwd);
    tt2_j(a3, "mounted_host", mounted);
    json_t *uc = tt2_call(s, "is_unusable_container_cwd", a3);
    if (tt2_raised(uc)) { json_free(cwd); return uc; }
    if (!tt2_truth(uc)) { json_free(uc); return cwd; }
    json_free(uc);
    if (mounted) {
        const json_t *cm = tt2_env_attr(env, "host_cwd_mount");
        const json_t *container_mount = tt2_truth(cm) ? cm : NULL;
        const char *cm_str = container_mount ? tt2_strval(container_mount) : "/workspace";
        json_t *a4 = tt2_args();
        tt2_j(a4, "cwd", cwd);
        tt2_j(a4, "mounted_host", mounted);
        json_t *mh = tt2_call(s, "is_mounted_host_cwd", a4);
        if (tt2_raised(mh)) { json_free(cwd); return mh; }
        if (tt2_truth(mh)) {
            json_free(mh); json_free(cwd);
            return container_mount ? json_copy(container_mount) : json_string("/workspace");
        }
        json_free(mh);
        json_t *a5 = tt2_args();
        tt2_j(a5, "path", cwd);
        tt2_j(a5, "host_root", mounted);
        tt2_s(a5, "container_root", cm_str);
        json_t *tr = tt2_call(s, "translate_mounted_host_path", a5);
        if (tt2_raised(tr)) { json_free(cwd); return tr; }
        if (tt2_truth(tr)) { json_free(cwd); return tr; }
        json_free(tr);
    }
    json_free(cwd);
    return NULL;
}

/* PoP: tt2_shell_state_chain @ tools/terminal_tool.py:_shell_state_chain */
static json_t *tt2_shell_state_chain(const json_t *st, const char *task_id) {
    json_t *chain = json_array();
    const json_t *aliases = tt2_st_obj(st, "container_aliases");
    const char *key = task_id;
    while (key && key[0] && aliases && json_has(aliases, key)) {
        bool seen = false;
        for (size_t i = 0; i < json_len(chain); i++) {
            const json_t *it = json_get(chain, i);
            if (tt2_is_str(it) && tt2_streq(it->str_val, key)) { seen = true; break; }
        }
        if (seen) break;
        json_append(chain, json_string(key));
        const json_t *nxt = json_obj_get(aliases, key);
        key = tt2_strval(nxt);
    }
    return chain;
}

/* PoP: tt2_session_scope @ tools/terminal_tool.py:_session_scope */
static json_t *tt2_session_scope(const tt2_seams_t *s) {
    json_t *br = tt2_call(s, "ensure_terminal_env_bridged", tt2_args());
    if (tt2_raised(br)) return br;
    json_free(br);
    json_t *a1 = tt2_args();
    tt2_s(a1, "name", "TERMINAL_ENV");
    tt2_s(a1, "default", "local");
    json_t *env_type = tt2_call(s, "tenv", a1);
    if (tt2_raised(env_type)) return env_type;
    json_t *a2 = tt2_args();
    tt2_s(a2, "name", "TERMINAL_CONTAINER_PERSISTENT");
    tt2_s(a2, "default", "true");
    json_t *persistent = tt2_call(s, "tenv_bool", a2);
    if (tt2_raised(persistent)) { json_free(env_type); return persistent; }
    json_t *scope = json_object();
    json_set(scope, "env_type", env_type);
    json_set(scope, "persistent", persistent);
    return scope;
}

/* PoP: tt2_routed_home_task_key @ tools/terminal_tool.py:_routed_home_task_key */
static json_t *tt2_routed_home_task_key(bool profile_scoped, const tt2_seams_t *s) {
    json_t *override = tt2_call(s, "get_hermes_home_override", tt2_args());
    if (tt2_raised(override)) return override;
    if (!tt2_truth(override)) { json_free(override); return NULL; }
    json_t *a1 = tt2_args();
    tt2_j(a1, "home", override);
    json_t *routed = tt2_call(s, "is_routed_home", a1);
    if (tt2_raised(routed)) { json_free(override); return routed; }
    if (!tt2_truth(routed)) { json_free(routed); json_free(override); return NULL; }
    json_free(routed);
    json_t *profile = NULL;
    if (profile_scoped) {
        json_t *a2 = tt2_args();
        tt2_j(a2, "home", override);
        profile = tt2_call(s, "profile_name_for_home", a2);
        if (tt2_raised(profile)) { json_free(override); return profile; }
    }
    if (tt2_truth(profile)) {
        json_t *a3 = tt2_args();
        tt2_j(a3, "profile", profile);
        json_free(profile); json_free(override);
        return tt2_call(s, "profile_container_key", a3);
    }
    json_free(profile);
    json_t *a4 = tt2_args();
    tt2_j(a4, "path", override);
    json_t *rp = tt2_call(s, "os.path.realpath", a4);
    if (tt2_raised(rp)) {
        if (tt2_exc_is(tt2_kind(rp), "OSError")) {
            json_free(rp);
            char *res = tt2_fmt("home:%s", tt2_strval(override));
            json_free(override);
            json_t *out = json_string(res);
            free(res);
            return out;
        }
        json_free(override);
        return rp;   /* BaseException / non-OSError passes through */
    }
    char *rps = tt2_pystr(rp);
    char *res = tt2_fmt("home:%s", rps);
    free(rps);
    json_free(rp); json_free(override);
    json_t *out = json_string(res);
    free(res);
    return out;
}

/* PoP: tt2_sessionless_container_key @ tools/terminal_tool.py:_sessionless_container_key */
static json_t *tt2_sessionless_container_key(bool profile_scoped, const tt2_seams_t *s) {
    json_t *routed = tt2_routed_home_task_key(profile_scoped, s);
    if (tt2_raised(routed)) return routed;
    if (tt2_truth(routed)) return routed;
    json_free(routed);
    if (profile_scoped) {
        json_t *hp = tt2_call(s, "home_profile_name", tt2_args());
        if (tt2_raised(hp)) return hp;
        json_t *a = tt2_args();
        tt2_j(a, "profile", hp);
        json_free(hp);
        return tt2_call(s, "profile_container_key", a);
    }
    return json_string("default");
}

/* PoP: tt2_workspace_key_for @ tools/terminal_tool.py:workspace_key_for */
static json_t *tt2_workspace_key_for(const char *cwd, const tt2_seams_t *s) {
    json_t *a1 = tt2_args();
    tt2_s(a1, "path", cwd);
    json_t *eu = tt2_call(s, "os.path.expanduser", a1);
    if (tt2_raised(eu)) return eu;
    json_t *a2 = tt2_args();
    tt2_j(a2, "path", eu);
    json_free(eu);
    json_t *rp = tt2_call(s, "os.path.realpath", a2);
    if (tt2_raised(rp)) return rp;
    char *hex = tt2_sha256_hex16(tt2_strval(rp));
    json_free(rp);
    json_t *out = json_string(hex);
    free(hex);
    return out;
}

/* PoP: tt2_workspace_container_key @ tools/terminal_tool.py:_workspace_container_key */
static json_t *tt2_workspace_container_key(const char *base, const char *task_id, json_t *st, const tt2_seams_t *s) {
    const char *b = base ? base : "";
    if (!task_id || !task_id[0]) return json_string(b);
    char *prefix = tt2_fmt("%s:ws:", b);
    bool already = tt2_startswith(task_id, prefix);
    free(prefix);
    if (already) return json_string(task_id);
    json_t *a0 = tt2_args();
    tt2_s(a0, "name", "TERMINAL_DOCKER_MOUNT_CWD_TO_WORKSPACE");
    tt2_s(a0, "default", "false");
    json_t *tb = tt2_call(s, "tenv_bool", a0);
    if (tt2_raised(tb)) return tb;
    if (!tt2_truth(tb)) { json_free(tb); return json_string(b); }
    json_free(tb);
    json_t *a1 = tt2_args();
    tt2_s(a1, "task_id", task_id);
    json_t *alias = tt2_call(s, "resolve_container_alias", a1);
    if (tt2_raised(alias)) return alias;
    /* dict.fromkeys((task_id, alias)): first-wins dedupe, insertion order */
    json_t *keys = json_array();
    json_append(keys, json_string(task_id));
    const char *alias_s = tt2_strval(alias);
    if (!(alias_s && tt2_streq(alias_s, task_id)))
        json_append(keys, alias ? json_copy(alias) : json_null());
    json_free(alias);
    json_t *result = NULL;
    for (size_t i = 0; i < json_len(keys) && !result; i++) {
        const json_t *key = json_get(keys, i);
        json_t *a2 = tt2_args();
        tt2_j(a2, "key", key);
        json_t *qk = tt2_call(s, "qualify_task_key", a2);
        if (tt2_raised(qk)) { json_free(keys); return qk; }
        const json_t *overrides = tt2_table_get(st, "task_env_overrides", tt2_strval(qk));
        json_free(qk);
        if (!tt2_truth(overrides)) continue;
        const json_t *cs = tt2_oget(overrides, "cwd_source");
        if (!(tt2_is_str(cs) && tt2_streq(cs->str_val, "session"))) continue;
        const json_t *digest = tt2_oget(overrides, "workspace_key");
        const json_t *cwd_v = tt2_oget(overrides, "cwd");
        json_t *computed = NULL;
        if (!tt2_truth(digest)) {
            if (tt2_is_str(cwd_v) && tt2_nonws(cwd_v->str_val) && !tt2_has_nul(cwd_v->str_val)) {
                computed = tt2_workspace_key_for(cwd_v->str_val, s);
                if (tt2_raised(computed)) { json_free(keys); return computed; }
            }
        }
        const json_t *use_digest = tt2_truth(digest) ? digest : computed;
        if (tt2_truth(use_digest)) {
            char *ds = tt2_pystr(use_digest);
            char *res = tt2_fmt("%s:ws:%s", b, ds);
            free(ds);
            result = json_string(res);
            free(res);
        }
        json_free(computed);
    }
    json_free(keys);
    return result ? result : json_string(b);
}

/* PoP: tt2_select_image @ tools/terminal_tool.py:_select_image */
static json_t *tt2_select_image(const char *env_type, const json_t *overrides, const json_t *config) {
    const char *et = env_type ? env_type : "";
    json_t *g = tt2_get_guard(overrides);
    if (g) return g;
    const char *key = NULL;
    if (strcmp(et, "docker") == 0) key = "docker_image";
    else if (strcmp(et, "modal") == 0) key = "modal_image";
    else if (strcmp(et, "daytona") == 0) key = "daytona_image";
    if (!key) {
        char *k = tt2_fmt("%s_image", et);
        const json_t *v = json_obj_get(overrides, k);
        free(k);
        return tt2_truth(v) ? json_copy(v) : json_string("");
    }
    const json_t *v = json_obj_get(overrides, key);
    if (tt2_truth(v)) return json_copy(v);
    /* config[key] — subscript, not .get */
    if (!config || config->type != JSON_OBJECT) {
        char buf[128];
        snprintf(buf, sizeof(buf), "'%s' object is not subscriptable", tt2_typename(config));
        return tt2_raise("TypeError", buf);
    }
    const json_t *cv = json_obj_get(config, key);
    if (!cv) {
        char buf[128];
        snprintf(buf, sizeof(buf), "'%s'", key);
        return tt2_raise("KeyError", buf);
    }
    return json_copy(cv);
}

/* PoP: tt2_lookup_active_env @ tools/terminal_tool.py:_lookup_active_env */
static json_t *tt2_lookup_active_env(const char *eff, const char *task_id, json_t *st, const tt2_seams_t *s) {
    json_t *hs = tt2_call(s, "hosted_scope", tt2_args());
    if (tt2_raised(hs)) return hs;
    bool hosted = !tt2_is_null(hs);   /* hosted_scope() is None — identity */
    json_free(hs);
    const char *keys[2];
    size_t nkeys = 0;
    keys[nkeys++] = eff;
    if (!hosted) keys[nkeys++] = task_id;
    const json_t *active = tt2_st_obj(st, "active_environments");
    for (size_t i = 0; i < nkeys; i++) {
        const char *key = keys[i];
        if (!key || !key[0] || !active || !json_has(active, key)) continue;
        json_t *tt = tt2_call(s, "time.time", tt2_args());
        if (tt2_raised(tt)) return tt;
        json_t *la = tt2_st_ensure(st, "last_activity");
        json_set(la, key, tt);
        json_t *a = tt2_args();
        tt2_j(a, "env", json_obj_get(active, key));
        return tt2_call(s, "admit_env", a);
    }
    return NULL;
}

/* PoP: tt2_session_workspace_mount_source @ tools/terminal_tool.py:_session_workspace_mount_source */
static json_t *tt2_session_workspace_mount_source(const char *task_id, const tt2_seams_t *s) {
    json_t *a0 = tt2_args();
    tt2_s(a0, "task_id", task_id);
    json_t *ov = tt2_call(s, "resolve_task_overrides", a0);
    if (tt2_raised(ov)) return ov;
    json_t *g = tt2_get_guard(ov);
    if (g) { json_free(ov); return g; }
    const json_t *candidate = tt2_oget(ov, "cwd");
    const json_t *cs = tt2_oget(ov, "cwd_source");
    bool process_tag = tt2_is_str(cs) && tt2_streq(cs->str_val, "process");
    if (process_tag || !tt2_is_str(candidate) || !tt2_nonws(candidate->str_val)) {
        json_free(ov);
        return NULL;
    }
    json_t *a1 = tt2_args();
    tt2_j(a1, "path", candidate);
    json_t *eu = tt2_call(s, "os.path.expanduser", a1);
    if (tt2_raised(eu)) { json_free(ov); return eu; }
    json_t *a2 = tt2_args();
    tt2_j(a2, "path", eu);
    json_free(eu);
    json_t *ab = tt2_call(s, "os.path.abspath", a2);
    if (tt2_raised(ab)) { json_free(ov); return ab; }
    json_t *a3 = tt2_args();
    tt2_j(a3, "path", ab);
    json_t *id = tt2_call(s, "os.path.isdir", a3);
    if (tt2_raised(id)) { json_free(ov); json_free(ab); return id; }
    bool isdir = tt2_truth(id);
    json_free(id);
    json_free(ov);
    if (!isdir) { json_free(ab); return NULL; }
    const char *cand = tt2_strval(ab);
    if (tt2_streq(cand, "/workspace") || tt2_startswith(cand, "/workspace/")
        || tt2_streq(cand, "/root") || tt2_startswith(cand, "/root/")) {
        json_free(ab);
        return NULL;
    }
    return ab;
}

/* PoP: tt2_backend_default_cwd @ tools/terminal_tool.py:_backend_default_cwd */
static json_t *tt2_backend_default_cwd(const char *env_type, const tt2_seams_t *s) {
    const char *et = env_type ? env_type : "";
    if (strcmp(et, "local") == 0) return tt2_call(s, "safe_getcwd", tt2_args());
    if (strcmp(et, "ssh") == 0) return json_string("~");
    if (strcmp(et, "vercel_sandbox") == 0) return json_string("/vercel/sandbox");
    json_t *a = tt2_args();
    tt2_s(a, "env_type", et);
    tt2_s(a, "attr", "default_cwd");
    json_set(a, "default", json_null());
    json_t *declared = tt2_call(s, "plugin_env_flag", a);
    if (tt2_raised(declared)) return declared;
    if (tt2_is_str(declared)) {
        const char *d = declared->str_val ? declared->str_val : "";
        if (strcmp(d, "~") == 0 || tt2_startswith(d, "/")) return declared;
    }
    json_free(declared);
    return json_string("/root");
}

/* PoP: tt2_resolve_config_cwd @ tools/terminal_tool.py:_resolve_config_cwd */
static json_t *tt2_resolve_config_cwd(const char *env_type, bool mount_docker_cwd, json_t *st, const tt2_seams_t *s) {
    static const char *IGN_FMT =
        "Ignoring TERMINAL_CWD=%r for %s backend "
        "(host/relative path won't work in sandbox). Using %r instead.";
    const char *et = env_type ? env_type : "";
    const char *os_name = json_get_str(st, "os_name", "posix");
    json_t *default_cwd = tt2_backend_default_cwd(et, s);
    if (tt2_raised(default_cwd)) return default_cwd;
    json_t *a0 = tt2_args();
    tt2_s(a0, "name", "TERMINAL_CWD");
    tt2_j(a0, "default", default_cwd);
    json_t *cwd = tt2_call(s, "tenv", a0);
    if (tt2_raised(cwd)) { json_free(default_cwd); return cwd; }
    /* if cwd and cwd != default_cwd and not _is_ssh_remote_tilde_cwd(...) and hosted_scope() is None */
    bool gate = tt2_truth(cwd) && !tt2_streq(tt2_strval(cwd), tt2_strval(default_cwd));
    if (gate) {
        json_t *a1 = tt2_args();
        tt2_s(a1, "backend", et);
        tt2_j(a1, "cwd", cwd);
        json_t *tilde = tt2_call(s, "is_ssh_remote_tilde_cwd", a1);
        if (tt2_raised(tilde)) { json_free(cwd); json_free(default_cwd); return tilde; }
        gate = !tt2_truth(tilde);
        json_free(tilde);
    }
    if (gate) {
        json_t *hs = tt2_call(s, "hosted_scope", tt2_args());
        if (tt2_raised(hs)) { json_free(cwd); json_free(default_cwd); return hs; }
        gate = tt2_is_null(hs);
        json_free(hs);
    }
    if (gate) {
        json_t *a2 = tt2_args();
        tt2_j(a2, "path", cwd);
        json_t *eu = tt2_call(s, "os.path.expanduser", a2);
        if (tt2_raised(eu)) { json_free(cwd); json_free(default_cwd); return eu; }
        json_free(cwd);
        cwd = eu;
    }
    json_t *host_cwd = NULL;
    bool is_docker = strcmp(et, "docker") == 0;
    if (is_docker && mount_docker_cwd) {
        json_t *a3 = tt2_args();
        tt2_s(a3, "name", "TERMINAL_CWD");
        tt2_s(a3, "default", "");
        json_t *t1 = tt2_call(s, "tenv", a3);
        if (tt2_raised(t1)) { json_free(cwd); json_free(default_cwd); return t1; }
        json_t *cand_src;
        if (tt2_truth(t1)) {
            cand_src = t1;
        } else {
            json_free(t1);
            cand_src = tt2_call(s, "safe_getcwd", tt2_args());
            if (tt2_raised(cand_src)) { json_free(cwd); json_free(default_cwd); return cand_src; }
        }
        json_t *a4 = tt2_args();
        tt2_j(a4, "path", cand_src);
        json_free(cand_src);
        json_t *eu = tt2_call(s, "os.path.expanduser", a4);
        if (tt2_raised(eu)) { json_free(cwd); json_free(default_cwd); return eu; }
        json_t *a5 = tt2_args();
        tt2_j(a5, "path", eu);
        json_free(eu);
        json_t *ab = tt2_call(s, "os.path.abspath", a5);
        if (tt2_raised(ab)) { json_free(cwd); json_free(default_cwd); return ab; }
        json_t *a6 = tt2_args();
        tt2_j(a6, "path", ab);
        json_t *hc = tt2_call(s, "is_host_cwd", a6);
        if (tt2_raised(hc)) { json_free(ab); json_free(cwd); json_free(default_cwd); return hc; }
        bool ok = tt2_truth(hc);
        json_free(hc);
        if (!ok) {
            json_t *a7 = tt2_args();
            tt2_j(a7, "path", ab);
            json_t *ia = tt2_call(s, "os.path.isabs", a7);
            if (tt2_raised(ia)) { json_free(ab); json_free(cwd); json_free(default_cwd); return ia; }
            if (tt2_truth(ia)) {
                json_t *a8 = tt2_args();
                tt2_j(a8, "path", ab);
                json_t *id = tt2_call(s, "os.path.isdir", a8);
                if (tt2_raised(id)) { json_free(ia); json_free(ab); json_free(cwd); json_free(default_cwd); return id; }
                const char *cand = tt2_strval(ab);
                ok = tt2_truth(id) && !tt2_startswith(cand, "/workspace") && !tt2_startswith(cand, "/root");
                json_free(id);
            }
            json_free(ia);
        }
        if (ok) {
            host_cwd = ab;
            json_free(cwd);
            cwd = json_string("/workspace");
        } else {
            json_free(ab);
        }
    } else if (is_docker) {
        json_t *a9 = tt2_args();
        tt2_j(a9, "path", cwd);
        json_t *wd = tt2_call(s, "is_windows_drive_path", a9);
        if (tt2_raised(wd)) { json_free(cwd); json_free(default_cwd); return wd; }
        if (tt2_truth(wd)) {
            json_t *a10 = tt2_args();
            tt2_j(a10, "path", cwd);
            json_t *cand = tt2_call(s, "os.path.expanduser", a10);
            if (tt2_raised(cand)) { json_free(wd); json_free(cwd); json_free(default_cwd); return cand; }
            if (strcmp(os_name, "nt") == 0) {
                json_t *a11 = tt2_args();
                tt2_j(a11, "path", cand);
                json_t *ab = tt2_call(s, "os.path.abspath", a11);
                if (tt2_raised(ab)) { json_free(cand); json_free(wd); json_free(cwd); json_free(default_cwd); return ab; }
                json_free(cand);
                cand = ab;
            }
            json_t *a12 = tt2_args();
            tt2_j(a12, "path", cand);
            json_t *id = tt2_call(s, "os.path.isdir", a12);
            if (tt2_raised(id)) { json_free(cand); json_free(wd); json_free(cwd); json_free(default_cwd); return id; }
            if (tt2_truth(id)) {
                json_free(host_cwd);
                host_cwd = json_copy(cand);
                json_free(cwd);
                cwd = cand;
            } else {
                json_t *la = json_array();
                json_append(la, json_copy(cwd));
                json_append(la, json_string(et));
                json_append(la, json_copy(default_cwd));
                json_t *lr = tt2_log(s, "info", IGN_FMT, la);
                if (tt2_raised(lr)) { json_free(cand); json_free(id); json_free(wd); json_free(cwd); json_free(default_cwd); return lr; }
                json_free(lr);
                json_free(cwd);
                cwd = json_copy(default_cwd);
                json_free(cand);
            }
            json_free(id);
        }
        json_free(wd);
    } else {
        json_t *a13 = tt2_args();
        tt2_s(a13, "env_type", et);
        json_t *cb = tt2_call(s, "is_container_backend", a13);
        if (tt2_raised(cb)) { json_free(cwd); json_free(default_cwd); return cb; }
        if (tt2_truth(cb) && tt2_truth(cwd)) {
            json_t *a14 = tt2_args();
            tt2_j(a14, "cwd", cwd);
            json_set(a14, "mounted_host", json_null());
            json_t *uc = tt2_call(s, "is_unusable_container_cwd", a14);
            if (tt2_raised(uc)) { json_free(cb); json_free(cwd); json_free(default_cwd); return uc; }
            if (tt2_truth(uc) && !tt2_streq(tt2_strval(cwd), tt2_strval(default_cwd))) {
                json_t *la = json_array();
                json_append(la, json_copy(cwd));
                json_append(la, json_string(et));
                json_append(la, json_copy(default_cwd));
                json_t *lr = tt2_log(s, "info", IGN_FMT, la);
                if (tt2_raised(lr)) { json_free(uc); json_free(cb); json_free(cwd); json_free(default_cwd); return lr; }
                json_free(lr);
                json_free(cwd);
                cwd = json_copy(default_cwd);
            }
            json_free(uc);
        }
        json_free(cb);
    }
    json_free(default_cwd);
    json_t *out = json_array();
    json_append(out, cwd);
    json_append(out, host_cwd ? host_cwd : json_null());
    return out;
}

/* PoP: tt2_rewrite_via_env_mount @ tools/terminal_tool.py:_rewrite_via_env_mount */
static json_t *tt2_rewrite_via_env_mount(const char *path, const json_t *env, const tt2_seams_t *s) {
    if (tt2_is_null(env) || !path || !path[0]) return NULL;
    const json_t *host = tt2_env_attr(env, "host_cwd");
    const json_t *mount = tt2_env_attr(env, "host_cwd_mount");
    if (!tt2_is_str(host) || !tt2_truth(host) || !tt2_truth(mount)) return NULL;
    json_t *a = tt2_args();
    tt2_s(a, "path", path);
    tt2_j(a, "host_root", host);
    tt2_j(a, "container_root", mount);
    return tt2_call(s, "translate_mounted_host_path", a);
}

/* PoP: tt2_mount_envs @ tools/terminal_tool.py:_mount_envs */
static json_t *tt2_mount_envs(const json_t *env, const json_t *st) {
    json_t *out = json_array();
    if (!tt2_is_null(env)) {
        json_append(out, json_copy(env));
        return out;
    }
    const json_t *active = tt2_st_obj(st, "active_environments");
    if (active) {
        for (size_t i = 0; i < active->c.count; i++)
            json_append(out, json_copy(active->c.items[i]));
    }
    return out;
}

/* PoP: tt2_container_visible_cwd @ tools/terminal_tool.py:_container_visible_cwd */
static json_t *tt2_container_visible_cwd(const char *path, const char *env_type, const json_t *env,
                                         const json_t *st, const tt2_seams_t *s) {
    const char *p = path ? path : "";
    if (!p[0]) return json_string(p);
    json_t *a = tt2_args();
    tt2_s(a, "env_type", env_type ? env_type : "");
    json_t *cb = tt2_call(s, "is_container_backend", a);
    if (tt2_raised(cb)) return cb;
    if (!tt2_truth(cb)) { json_free(cb); return json_string(p); }
    json_free(cb);
    json_t *envs = tt2_mount_envs(env, st);
    for (size_t i = 0; i < json_len(envs); i++) {
        const json_t *item = json_get(envs, i);
        json_t *tr = tt2_rewrite_via_env_mount(p, item, s);
        if (tt2_raised(tr)) { json_free(envs); return tr; }
        if (tt2_truth(tr)) { json_free(envs); return tr; }
        json_free(tr);
    }
    json_free(envs);
    return json_string(p);
}

/* PoP: tt2_container_visible_default @ tools/terminal_tool.py:_container_visible_default */
static json_t *tt2_container_visible_default(const char *default_cwd, const char *env_type, const json_t *env,
                                             const json_t *st, const tt2_seams_t *s) {
    const char *dc = default_cwd ? default_cwd : "";
    json_t *a = tt2_args();
    tt2_s(a, "env_type", env_type ? env_type : "");
    json_t *cb = tt2_call(s, "is_container_backend", a);
    if (tt2_raised(cb)) return cb;
    if (!tt2_truth(cb)) { json_free(cb); return json_string(dc); }
    json_free(cb);
    json_t *envs = tt2_mount_envs(env, st);
    json_t *result = NULL;
    for (size_t i = 0; i < json_len(envs) && !result; i++) {
        const json_t *item = json_get(envs, i);
        const json_t *mount = tt2_env_attr(item, "host_cwd_mount");
        const json_t *host = tt2_env_attr(item, "host_cwd");
        if (!tt2_truth(mount) || !tt2_truth(host) || tt2_streq(tt2_strval(mount), dc)) continue;
        if (strcmp(dc, "/workspace") == 0) {
            result = json_copy(mount);
            break;
        }
        json_t *a2 = tt2_args();
        tt2_s(a2, "cwd", dc);
        json_set(a2, "mounted_host", json_null());
        json_t *uc = tt2_call(s, "is_unusable_container_cwd", a2);
        if (tt2_raised(uc)) { json_free(envs); return uc; }
        if (tt2_truth(uc)) result = json_copy(mount);
        json_free(uc);
    }
    json_free(envs);
    return result ? result : json_string(dc);
}

/* PoP: tt2_fatal_error_json @ tools/terminal_tool.py:_fatal_error_json */
static json_t *tt2_fatal_error_json(const json_t *exc_spec, const tt2_seams_t *s) {
    json_t *tb = tt2_call(s, "traceback.format_exc", tt2_args());
    if (tt2_raised(tb)) return tb;
    json_t *lr = tt2_log1(s, "error", "terminal_tool exception:\n%s", tb);
    if (tt2_raised(lr)) { json_free(tb); return lr; }
    json_free(lr);
    char *estr = tt2_exc_str(exc_spec);
    char *msg = tt2_fmt("Failed to execute command: %s", estr);
    free(estr);
    json_t *a1 = tt2_args();
    tt2_s(a1, "value", msg);
    free(msg);
    json_t *rd = tt2_call(s, "redact_terminal_error_text", a1);
    if (tt2_raised(rd)) { json_free(tb); return rd; }
    json_t *body = json_object();
    json_set(body, "output", json_string(""));
    json_set(body, "exit_code", json_number(-1));
    json_set(body, "error", rd);
    json_t *hs = tt2_call(s, "hosted_scope", tt2_args());
    if (tt2_raised(hs)) { json_free(body); json_free(tb); return hs; }
    if (tt2_is_null(hs)) {
        json_t *a2 = tt2_args();
        tt2_j(a2, "value", tb);
        json_t *rd2 = tt2_call(s, "redact_terminal_error_text", a2);
        if (tt2_raised(rd2)) { json_free(hs); json_free(body); json_free(tb); return rd2; }
        json_set(body, "traceback", rd2);
    }
    json_free(hs);
    json_set(body, "status", json_string("error"));
    char *dumped = tt2_dumps(body);
    json_free(body); json_free(tb);
    json_t *out = json_string(dumped);
    free(dumped);
    return out;
}

/* PoP: tt2_acquire_env @ tools/terminal_tool.py:_acquire_env */
static json_t *tt2_acquire_env(const json_t *plan, const char *task_id, json_t *st, const tt2_seams_t *s) {
    json_t *sc = tt2_call(s, "start_cleanup_thread", tt2_args());
    if (tt2_raised(sc)) return sc;
    json_free(sc);
    const char *env_type = tt2_strval(tt2_oget(plan, "env_type"));
    const char *eff = tt2_strval(tt2_oget(plan, "effective_task_id"));
    if (!env_type) env_type = "";
    if (!eff) eff = "";
    json_t *env = tt2_lookup_active_env(eff, task_id, st, s);
    if (tt2_raised(env)) return env;
    if (!tt2_is_null(env)) return env;   /* `env is not None` */
    json_free(env);
    /* _creation_locks.setdefault(eff, threading.Lock()) — presence marker */
    json_t *cl = tt2_st_ensure(st, "creation_locks");
    if (!json_has(cl, eff)) json_set(cl, eff, json_bool(true));
    env = tt2_lookup_active_env(eff, task_id, st, s);
    if (tt2_raised(env)) return env;
    if (!tt2_is_null(env)) return env;
    json_free(env);
    char eff8[9];
    snprintf(eff8, sizeof(eff8), "%.8s", eff);
    json_t *la1 = json_array();
    json_append(la1, json_string(env_type));
    json_append(la1, json_string(eff8));
    json_t *lr1 = tt2_log(s, "info", "Creating new %s environment for task %s...", la1);
    if (tt2_raised(lr1)) return lr1;
    json_free(lr1);
    json_t *a = tt2_args();
    tt2_j(a, "config", tt2_oget(plan, "config"));
    tt2_s(a, "env_type", env_type);
    tt2_j(a, "image", tt2_oget(plan, "image"));
    tt2_j(a, "cwd", tt2_oget(plan, "cwd"));
    tt2_j(a, "timeout", tt2_oget(plan, "effective_timeout"));
    tt2_s(a, "task_id", eff);
    tt2_j(a, "host_cwd", tt2_oget(plan, "host_cwd"));
    json_t *new_env = tt2_call(s, "create_configured_env", a);
    if (tt2_raised(new_env)) {
        if (tt2_exc_is(tt2_kind(new_env), "ImportError")) {
            char *estr = tt2_exc_str(new_env);
            char *msg = tt2_fmt("Terminal tool disabled: environment creation failed (%s)", estr);
            free(estr);
            json_free(new_env);
            json_t *a2 = tt2_args();
            tt2_s(a2, "value", msg);
            free(msg);
            json_t *rd = tt2_call(s, "redact_terminal_error_text", a2);
            if (tt2_raised(rd)) return rd;
            json_t *a3 = tt2_args();
            tt2_j(a3, "error", rd);
            tt2_n(a3, "exit_code", -1);
            tt2_s(a3, "status", "disabled");
            json_free(rd);
            json_t *ej = tt2_call(s, "error_json", a3);
            if (tt2_raised(ej)) return ej;
            json_t *rej = tt2_raise("_Rejected", tt2_strval(ej));
            json_free(ej);
            return rej;
        }
        return new_env;   /* other exceptions propagate */
    }
    json_t *ae = tt2_st_ensure(st, "active_environments");
    json_set(ae, eff, new_env);   /* ownership transfers into state */
    json_t *tt = tt2_call(s, "time.time", tt2_args());
    if (tt2_raised(tt)) return tt;
    json_t *la = tt2_st_ensure(st, "last_activity");
    json_set(la, eff, tt);
    json_t *la2 = json_array();
    json_append(la2, json_string(env_type));
    json_append(la2, json_string(eff8));
    json_t *lr2 = tt2_log(s, "info", "%s environment ready for task %s", la2);
    if (tt2_raised(lr2)) return lr2;
    json_free(lr2);
    return json_copy(json_obj_get(ae, eff));
}

/* PoP: tt2_yield_kwargs @ tools/terminal_tool.py:_yield_kwargs */
static json_t *tt2_yield_kwargs(const char *command, const json_t *ctx, const tt2_seams_t *s) {
    static const char *KNOWN[6] = {
        "command", "env_type", "cwd", "effective_task_id", "task_id", "session_key",
    };
    const json_t *c = (ctx && ctx->type == JSON_OBJECT) ? ctx : NULL;
    /* 1. keyword collision (wins over unexpected / missing) — the stub's
     * module-qualified name carries the oracle namespace prefix. */
    if (c && json_has(c, "command"))
        return tt2_raise("TypeError",
            "oracle_terminal_tool_flow2.yield_to_background_handler() "
            "got multiple values for keyword argument 'command'");
    /* 2. first unexpected key in ctx insertion order */
    if (c) {
        for (size_t i = 0; i < c->c.count; i++) {
            const char *k = c->c.keys[i];
            bool known = false;
            for (int j = 0; j < 6; j++) if (strcmp(k, KNOWN[j]) == 0) { known = true; break; }
            if (!known) {
                char *m = tt2_fmt("yield_to_background_handler() got an unexpected keyword argument '%s'", k);
                json_t *r = tt2_raise("TypeError", m);
                free(m);
                return r;
            }
        }
    }
    /* 3. missing required keyword-only args, signature order, Oxford comma */
    const char *missing[5];
    size_t nmiss = 0;
    for (int j = 1; j < 6; j++)
        if (!c || !json_has(c, KNOWN[j])) missing[nmiss++] = KNOWN[j];
    if (nmiss > 0) {
        char quoted[5][64];
        for (size_t i = 0; i < nmiss; i++)
            snprintf(quoted[i], sizeof(quoted[i]), "'%s'", missing[i]);
        char list[320];
        if (nmiss == 1) snprintf(list, sizeof(list), "%s", quoted[0]);
        else if (nmiss == 2) snprintf(list, sizeof(list), "%s and %s", quoted[0], quoted[1]);
        else {
            size_t off = 0;
            for (size_t i = 0; i < nmiss; i++) {
                int n = snprintf(list + off, sizeof(list) - off, "%s%s",
                                 quoted[i], (i + 1 < nmiss) ? ", " : "");
                if (n < 0) break;
                off += (size_t)n;
            }
            /* Oxford comma: insert "and" before the last element */
            char *last = strrchr(list, ',');
            if (last) {
                char tail[64];
                snprintf(tail, sizeof(tail), ", and %s", quoted[nmiss - 1]);
                snprintf(last, sizeof(list) - (size_t)(last - list), "%s", tail);
            }
        }
        char *m = tt2_fmt("yield_to_background_handler() missing %zu required keyword-only argument%s: %s",
                          nmiss, nmiss == 1 ? "" : "s", list);
        json_t *r = tt2_raise("TypeError", m);
        free(m);
        return r;
    }
    json_t *a = tt2_args();
    tt2_s(a, "command", command);
    for (int j = 1; j < 6; j++) tt2_j(a, KNOWN[j], json_obj_get(c, KNOWN[j]));
    json_t *handler = tt2_call(s, "yield_to_background_handler", a);
    if (tt2_raised(handler)) return handler;
    if (!tt2_is_null(handler)) {
        json_t *out = json_object();
        json_set(out, "yield_handler", handler);
        return out;
    }
    json_free(handler);
    return json_object();
}