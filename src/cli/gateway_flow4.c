/*
 * gateway_flow4.c -- FLOW port of the UNCLAIMED-remainder of
 * hermes_cli/gateway.py (LIVE authority) beyond the flow2 PoP wave.
 * Self-contained: libjson only. All functions static gf4_*; the harness
 * (#include "cli/gateway_flow4.c") wires the universal seam to scripted
 * results and records the ordered trace.
 *
 * PoP coverage (each function carries its PoP annotation immediately above
 * the definition, CRLF-safe):
 *   _parse_ps_line, _iter_windows_list_processes, _scm_service_field,
 *   _gateway_detached_env, _stdin_is_tty, _home_owns_bare_service_name,
 *   _cmd_setup, _cmd_list, _cmd_migrate_legacy,
 *   _print_unserved_shared_ingress, _print_duplicate_credential_warnings,
 *   _absorb_windows_console_controls, _ping_loop_tick_witness,
 *   _iter_proc_cmdlines, _wait_for_tcp_port_free,
 *   _restart_argv_is_host_gateway, _gw_windows.
 *
 * SEAM DOCTRINE: every live effect enters through ONE universal seam
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 * returning a NEW json_t* (NULL == Python None). A raising live call is
 * scripted {"__raise__": "<ExcClass>", "msg": ...}; the cores propagate it
 * exactly where live try/except catches it. except Exception does NOT catch
 * BaseException/SystemExit/KeyboardInterrupt/GeneratorExit/BaseExceptionGroup
 * -- those pass through to the harness ret envelope. A MISSING seam key
 * fail-closes (RuntimeError) on both sides; a present JSON null is a valid
 * None result. Arrays scripted as per-call queues (last repeats) in the
 * harness; the oracle mirrors that with an index counter.
 *
 * Full builtin exception parent hierarchy is transcribed below (audited
 * against the py3.14 quarry interpreter's builtins). Unknown scripted kinds
 * are synthetic Exception subclasses in the oracle (make_exc), so they are
 * caught by gf4_caught(r, "Exception") exactly as the C walk predicts.
 *
 * Fixture domains (documented exclusions):
 * - Pure string helpers (parse_ps_line / iter_windows_list_processes) run on
 *   ASCII only; Python str.strip's White_Space incl. \\x1c-\\x1f is modelled,
 *   int() accepts an optional sign and between-digit underscores, and
 *   str.isdigit() is the ASCII digit test. Unicode digits / non-ASCII WS are
 *   out of corpus.
 * - Paths are POSIX strings; path algebra is string equality of already
 *   resolved paths (no .. / symlink resolution in fixtures).
 * - Bytes in ping recv are JSON arrays of ints; b"1" == [49].
 * - Nested closures (_port_free inside _wait_for_tcp_port_free) are SEAMED
 *   as one poll_until call (marker argument), so the closure body is covered
 *   by its own direct cases only where it is a separately addressable entry.
 * - import_<name> / constant seams record module attribute access on the
 *   same channel, mirroring the oracle's stub modules.
 *
 * Ownership: seam results and returned json_t are malloc'd, caller frees.
 * Inputs (including seam args) are borrowed.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "json.h"

/* -- universal seam + trace -- */

typedef struct gf4_trace { json_t *events; } gf4_trace_t;

typedef struct gf4_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} gf4_seams_t;

static void gf4_trace_add(gf4_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static void gf4_trace_free(gf4_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}
static json_t *gf4_call(const gf4_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
static json_t *gf4_call0(const gf4_seams_t *s, const char *name) {
    json_t *a = json_object();
    json_t *r = gf4_call(s, name, a);
    json_free(a);
    return r;
}
static json_t *gf4_call1(const gf4_seams_t *s, const char *name, const char *k, const json_t *v) {
    json_t *a = json_object();
    json_set(a, k, v ? json_copy(v) : json_null());
    json_t *r = gf4_call(s, name, a);
    json_free(a);
    return r;
}

/* -- JSON value helpers -- */

static bool gf4_jtruth(const json_t *v) {
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
static const char *gf4_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool gf4_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static char *gf4_xsf(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return buf ? buf : strdup("");
}

/* growable byte buffer */
typedef struct { char *p; size_t len, cap; } gf4_buf;
static void gf4_binit(gf4_buf *b) { b->p = NULL; b->len = 0; b->cap = 0; }
static void gf4_bfree(gf4_buf *b) { free(b->p); b->p = NULL; b->len = b->cap = 0; }
static void gf4_bput(gf4_buf *b, const char *s, size_t n) {
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 64;
        while (nc < b->len + n + 1) nc *= 2;
        char *np = realloc(b->p, nc);
        if (!np) return;
        b->p = np; b->cap = nc;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}
static void gf4_bputs(gf4_buf *b, const char *s) { gf4_bput(b, s, strlen(s)); }
static char *gf4_btake(gf4_buf *b) {
    if (!b->p) return strdup("");
    char *r = b->p; b->p = NULL; b->len = b->cap = 0; return r;
}

/* -- exception model -- */

static const char *gf4_raise_class(const json_t *r) {
    if (r && r->type == JSON_OBJECT) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (c && c->type == JSON_STRING) return c->str_val;
    }
    return NULL;
}
static bool gf4_is_raise(const json_t *r) { return gf4_raise_class(r) != NULL; }

static json_t *gf4_raise_spec(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}

/* FULL builtin exception parent hierarchy (audited against the py3.14.9
 * quarry interpreter's builtins; IOError/EnvironmentError/WindowsError are
 * OSError aliases; ExceptionGroup routes through Exception to keep the
 * except-Exception contract; BaseExceptionGroup stays BaseException-only).
 * Unknown kinds are synthetic Exception subclasses (oracle make_exc). */
typedef struct { const char *name; const char *parent; } gf4_exc_parent;
static const gf4_exc_parent GF4_EXC_PARENTS[] = {
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
    {"OSError", "Exception"},
    {"ReferenceError", "Exception"},
    {"RuntimeError", "Exception"},
    {"StopAsyncIteration", "Exception"},
    {"StopIteration", "Exception"},
    {"SyntaxError", "Exception"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"Warning", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"ModuleNotFoundError", "ImportError"},
    {"ImportWarning", "Warning"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
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
    {"NotImplementedError", "RuntimeError"},
    {"PythonFinalizationError", "RuntimeError"},
    {"RecursionError", "RuntimeError"},
    {"UnboundLocalError", "NameError"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"_IncompleteInputError", "SyntaxError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"JSONDecodeError", "ValueError"},
    {"BytesWarning", "Warning"},
    {"DeprecationWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"EncodingWarning", "Warning"},
    {"IOError", "OSError"},
    {"EnvironmentError", "OSError"},
    {"WindowsError", "OSError"},
    {NULL, NULL}
};

/* BaseException-only classes: never caught by except Exception/OSError/... */
static bool gf4_base_only(const char *k) {
    return !strcmp(k, "BaseException") || !strcmp(k, "KeyboardInterrupt") ||
           !strcmp(k, "SystemExit") || !strcmp(k, "GeneratorExit") ||
           !strcmp(k, "BaseExceptionGroup");
}
static bool gf4_is_exception(const json_t *r) {
    const char *k = gf4_raise_class(r);
    return k && !gf4_base_only(k);
}
/* caught(r, cls): cls appears in the kind's ancestry walk */
static bool gf4_caught(const json_t *r, const char *cls) {
    const char *k = gf4_raise_class(r);
    if (!k) return false;
    const char *cur = k;
    for (int depth = 0; cur && depth < 64; depth++) {
        if (!strcmp(cur, cls)) return true;
        if (!strcmp(cur, "BaseException")) return false;
        const char *parent = NULL;
        for (size_t i = 0; GF4_EXC_PARENTS[i].name; i++) {
            if (!strcmp(cur, GF4_EXC_PARENTS[i].name)) {
                parent = GF4_EXC_PARENTS[i].parent;
                break;
            }
        }
        if (!parent) {
            /* unknown synthetic kind: oracle types it as Exception subclass */
            return !strcmp(cls, "Exception") || !strcmp(cls, "BaseException");
        }
        cur = parent;
    }
    return false;
}

/* -- string helpers (Python str.strip / split(None) / int / isdigit) -- */

static bool gf4_isspace_ascii(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == '\v' || c == '\f' || (c >= 0x1c && c <= 0x1f);
}
/* Python str.strip() over the ASCII White_Space set; returns new string. */
static char *gf4_strip(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && gf4_isspace_ascii((unsigned char)s[a])) a++;
    while (b > a && gf4_isspace_ascii((unsigned char)s[b - 1])) b--;
    char *r = malloc(b - a + 1);
    if (!r) return strdup("");
    memcpy(r, s + a, b - a);
    r[b - a] = '\0';
    return r;
}
/* split on ASCII whitespace, no empty tokens (Python str.split(None)).
   returns malloc'd char*[] (caller frees each + array); *out_n set. */
static char **gf4_split_ws(const char *s, size_t *out_n) {
    *out_n = 0;
    if (!s) return NULL;
    char **toks = NULL;
    size_t cap = 0, n = 0;
    const char *p = s;
    while (*p) {
        while (*p && gf4_isspace_ascii((unsigned char)*p)) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && !gf4_isspace_ascii((unsigned char)*p)) p++;
        if (n == cap) {
            cap = cap ? cap * 2 : 8;
            char **nt = realloc(toks, cap * sizeof(char *));
            if (!nt) break;
            toks = nt;
        }
        size_t len = (size_t)(p - start);
        toks[n] = malloc(len + 1);
        if (toks[n]) { memcpy(toks[n], start, len); toks[n][len] = '\0'; n++; }
    }
    *out_n = n;
    return toks;
}
static void gf4_toks_free(char **t, size_t n) {
    if (!t) return;
    for (size_t i = 0; i < n; i++) free(t[i]);
    free(t);
}
/* Python int() on ASCII decimal with optional sign + between-digit underscores.
   Whitespace on both ends is consumed. Returns true and sets *out. */
static bool gf4_int_parse(const char *s, long long *out) {
    if (!s) return false;
    const char *p = s;
    while (*p && gf4_isspace_ascii((unsigned char)*p)) p++;
    if (!*p) return false;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    if (!*p) return false;
    gf4_buf b; gf4_binit(&b);
    bool prev_digit = false, ok = true;
    while (*p) {
        if (*p >= '0' && *p <= '9') { gf4_bput(&b, p, 1); prev_digit = true; p++; }
        else if (*p == '_' && prev_digit && p[1] >= '0' && p[1] <= '9') {
            prev_digit = false; p++;   /* underscore between digits only */
        } else break;
    }
    if (!prev_digit) ok = false;
    while (*p && gf4_isspace_ascii((unsigned char)*p)) p++;
    if (*p != '\0') ok = false;
    if (ok && b.len) {
        long long v = 0;
        for (size_t i = 0; i < b.len; i++) {
            if (v > (LLONG_MAX - (b.p[i] - '0')) / 10) { ok = false; break; }
            v = v * 10 + (b.p[i] - '0');
        }
        if (ok) *out = neg ? -v : v;
    }
    gf4_bfree(&b);
    return ok;
}
static bool gf4_isdigit_ascii(const char *s) {
    if (!s || !*s) return false;
    for (const char *p = s; *p; p++) if (*p < '0' || *p > '9') return false;
    return true;
}

/* ========================================================================
 * FLOW cores (all static gf4_*)
 * ======================================================================== */


/* -- extra helpers for gateway_flow4 cores -- */
static json_t *gf4_callkw(const gf4_seams_t *s, const char *name, json_t *a) {
    json_t *r = gf4_call(s, name, a); json_free(a); return r;
}
#define GF4_JARG(dst,key,src,dflt) do { const json_t *_v = json_obj_get(src,key); \
    json_set(dst,key,json_copy(_v ? _v : (dflt))); } while(0)
static const char *gf4_cstr(const json_t *v, const char *d) {
    const char *s = gf4_jstr(v); return s ? s : d;
}
static double gf4_num(const json_t *v, double d) { return (v && v->type==JSON_NUMBER)?v->num_val:d; }
static void gf4_append_unique(json_t *a, const json_t *v) {
    if (!a || !v) return;
    for (size_t i = 0; i < json_len(a); i++) {
        const json_t *e = json_get(a, i);
        if ((v->type == JSON_NUMBER && e->type == JSON_NUMBER && gf4_num(e,0)==gf4_num(v,0)) ||
            (v->type == JSON_STRING && e->type == JSON_STRING && gf4_jstr(e) && gf4_jstr(v) && !strcmp(gf4_jstr(e), gf4_jstr(v))))
            return;
    }
    json_append(a, json_copy(v));
}
static char *gf4_numstr(const json_t *v) {
    if (v && v->type == JSON_NUMBER) {
        double d = v->num_val; long long i = (long long)d;
        if ((double)i == d) return gf4_xsf("%lld", i);
        return gf4_xsf("%g", d);
    }
    return strdup(gf4_cstr(v, ""));
}
static int gf4_cmp_str(const void *a, const void *b) {
    const json_t *x = *(const json_t * const *)a, *y = *(const json_t * const *)b;
    const char *xs = gf4_jstr(x) ? gf4_jstr(x) : "", *ys = gf4_jstr(y) ? gf4_jstr(y) : "";
    return strcmp(xs, ys);
}
static int gf4_cmp_num(const void *a, const void *b) {
    double x = gf4_num(*(const json_t * const *)a, 0), y = gf4_num(*(const json_t * const *)b, 0);
    return (x < y) ? -1 : (x > y) ? 1 : 0;
}
static json_t *gf4_sorted(json_t *a, int (*cmp)(const void *, const void *)) {
    if (!a || a->type != JSON_ARRAY) return a;
    size_t n = json_len(a);
    if (n < 2) return a;
    const json_t **items = malloc(n * sizeof(*items));
    if (!items) return a;
    for (size_t i = 0; i < n; i++) items[i] = json_get(a, i);
    qsort(items, n, sizeof(*items), cmp);
    json_t *out = json_array();
    for (size_t i = 0; i < n; i++) json_append(out, json_copy(items[i]));
    free(items); json_free(a);
    return out;
}
static json_t *gf4_sorted_strings(json_t *a) { return gf4_sorted(a, gf4_cmp_str); }
static json_t *gf4_sorted_numbers(json_t *a) { return gf4_sorted(a, gf4_cmp_num); }
/* path join: "/" of two POSIX path strings */
static char *gf4_pathjoin(const char *a, const char *b) {
    size_t la = a ? strlen(a) : 0, lb = b ? strlen(b) : 0;
    /* PurePath join: no extra separator when 'a' already ends with '/'
     * (root "/" therefore joins to "/b", never "//b"). */
    size_t sep = (la && a[la - 1] == '/') ? 0 : 1;
    char *r = malloc(la + sep + lb + 1);
    if (!r) return strdup("");
    if (la) memcpy(r, a, la);
    if (sep) r[la] = '/';
    if (lb) memcpy(r + la + sep, b, lb);
    r[la + sep + lb] = '\0';
    return r;
}
/* parent dir of a POSIX path (no trailing slash handling needed for fixtures) */
static char *gf4_pathparent(const char *s) {
    if (!s) return strdup("");
    const char *slash = strrchr(s, '/');
    if (!slash) return strdup("");
    size_t n = (size_t)(slash - s);
    char *r = malloc(n + 1); if (!r) return strdup("");
    memcpy(r, s, n); r[n] = '\0'; return r;
}

/* PoP: gf4_cmd_run @ hermes_cli/gateway.py:_cmd_run */
static json_t *gf4_cmd_run(const json_t *in, const gf4_seams_t *s) {
    json_t *r = gf4_call0(s, "redirect_s6");
    if (gf4_is_raise(r)) return r;
    bool redir = gf4_jtruth(r); json_free(r);
    if (redir) return NULL;
    const json_t *args = json_obj_get(in, "args");
    if (gf4_jtruth(json_obj_get(args, "external_supervisor"))) {
        json_t *a = json_object();
        json_set(a, "name", json_string("HERMES_EXTERNAL_GATEWAY_SUPERVISOR"));
        json_set(a, "value", json_string("1"));
        r = gf4_callkw(s, "setenv", a);
        if (gf4_is_raise(r)) return r;
        json_free(r);
    }
    json_t *a = json_object();
    GF4_JARG(a,"verbose",args,json_number(0));
    GF4_JARG(a,"quiet",args,json_bool(false));
    GF4_JARG(a,"replace",args,json_bool(false));
    GF4_JARG(a,"force",args,json_bool(false));
    GF4_JARG(a,"idle_exit",args,json_bool(false));
    return gf4_callkw(s, "run_gateway", a);
}

/* PoP: gf4_wait_for_api_server_port_free @ hermes_cli/gateway.py:_wait_for_api_server_port_free */
static json_t *gf4_wait_for_api_server_port_free(const json_t *in, const gf4_seams_t *s) {
    json_t *cfg = gf4_call0(s, "load_config");
    if (gf4_is_raise(cfg)) return cfg;
    const json_t *pc = json_obj_get(json_obj_get(cfg, "platforms"), "api_server");
    bool none_pc = (!pc || pc->type == JSON_NULL);
    bool enabled = pc && gf4_jtruth(json_obj_get(pc, "enabled"));
    json_t *extra = (pc && !none_pc) ? json_copy(json_obj_get(pc, "extra")) : NULL;
    json_free(cfg);
    if (none_pc || !enabled) { if (extra) json_free(extra); return json_bool(true); }
    json_t *a = json_object();
    json_set(a, "extra", extra ? extra : json_object());
    json_t *la = gf4_callkw(s, "listen_address", a);
    if (gf4_is_raise(la)) return la;
    /* Live: the listen_address shim does d["host"], d["port"] on the seam
     * result. A non-object result raises TypeError HERE, before wait_port. */
    if (!la || la->type != JSON_OBJECT) {
        const char *msg = (la && la->type == JSON_STRING) ? "string indices must be integers, not 'str'"
                        : (la && la->type == JSON_ARRAY) ? "list indices must be integers or slices, not str"
                        : (la && la->type == JSON_NUMBER) ? "'int' object is not subscriptable"
                        : "'NoneType' object is not subscriptable";
        json_t *err = gf4_raise_spec("TypeError", msg);
        if (la) json_free(la);
        return err;
    }
    char *host = strdup(gf4_cstr(json_obj_get(la, "host"), ""));
    json_t *portv = json_copy(json_obj_get(la, "port"));
    json_free(la);
    double timeout = gf4_num(json_obj_get(in, "timeout"), 10.0);
    json_t *wa = json_object();
    json_set(wa, "host", json_string(host));
    json_set(wa, "port", portv ? portv : json_null());
    json_set(wa, "timeout", json_number(timeout));
    json_t *freed = gf4_callkw(s, "wait_port", wa);
    if (gf4_is_raise(freed)) { free(host); return freed; }
    if (!gf4_jtruth(freed)) {
        char *portstr = gf4_numstr(portv);
        char *txt = gf4_xsf("\xe2\x9a\xa0 %s:%s still accepting connections \xe2\x80\x94 new api_server may fail to bind",
                            host, portstr);
        free(portstr);
        json_t *pa = json_object(); json_set(pa, "text", json_string(txt)); free(txt);
        json_t *pr = gf4_callkw(s, "print", pa);
        if (gf4_is_raise(pr)) { json_free(freed); free(host); return pr; }
        json_free(pr);
    }
    json_t *ret = json_copy(freed);
    json_free(freed); free(host);
    return ret;
}

/* PoP: gf4_pm_managed_node_dirs @ hermes_cli/gateway.py:_pm_managed_node_dirs */
static json_t *gf4_pm_managed_node_dirs(const json_t *in, const gf4_seams_t *s) {
    const char *home = gf4_cstr(json_obj_get(in, "home"), "");
    char *store = gf4_pathjoin(home, "tools");
    char *facts_path = gf4_pathjoin(store, "facts.json");
    json_t *fa = json_object(); json_set(fa, "path", json_string(facts_path));
    json_t *fr = gf4_callkw(s, "Facts_new", fa);
    if (gf4_is_raise(fr)) { free(store); free(facts_path); return fr; }
    json_free(fr);
    json_t *out = json_array();
    const char *names[3] = {"node","npm","npx"};
    for (int i = 0; i < 3; i++) {
        json_t *ea = json_object();
        json_set(ea, "name", json_string(names[i]));
        json_set(ea, "store", json_string(store));
        json_t *env = gf4_callkw(s, "env_for", ea);
        if (gf4_is_raise(env)) { json_free(out); free(store); free(facts_path); return env; }
        const json_t *path_list = json_obj_get(env, "PATH");
        if (path_list && path_list->type == JSON_ARRAY) {
            for (size_t j = 0; j < json_len(path_list); j++) {
                const json_t *v = json_get(path_list, j);
                const char *val = gf4_jstr(v);
                if (!val || !*val) continue; /* Python `if value` */
                json_t *ia = json_object(); json_set(ia, "path", json_string(val));
                json_t *ir = gf4_callkw(s, "is_dir", ia);
                if (gf4_is_raise(ir)) { json_free(env); json_free(out); free(store); free(facts_path); return ir; }
                bool isd = gf4_jtruth(ir); json_free(ir);
                if (isd) json_append(out, json_string(val));
            }
        }
        json_free(env);
    }
    free(store); free(facts_path);
    return gf4_sorted_strings(out);
}

/* PoP: gf4_native_service_homes @ hermes_cli/gateway.py:_native_service_homes */
static json_t *gf4_native_service_homes(const json_t *in, const gf4_seams_t *s) {
    (void)in;
    json_t *a = gf4_call0(s, "account_home");
    if (gf4_is_raise(a)) return a;
    const char *home = gf4_cstr(a, ""); char *hcopy = strdup(home); json_free(a);
    json_t *su = gf4_call0(s, "sudo_home");
    if (gf4_is_raise(su)) { free(hcopy); return su; }
    json_t *out = json_array();
    json_append(out, json_string(hcopy));
    if (!gf4_is_null(su)) {
        json_append(out, json_string(gf4_cstr(su, "")));
    }
    json_free(su); free(hcopy);
    return gf4_sorted_strings(out);
}

/* PoP: gf4_windows_process_listing @ hermes_cli/gateway.py:_windows_process_listing */
static json_t *gf4_windows_process_listing(const json_t *in, const gf4_seams_t *s) {
    (void)in;
    json_t *a1 = json_object(); json_set(a1, "name", json_string("wmic"));
    json_t *wmic = gf4_callkw(s, "which", a1);
    if (gf4_is_raise(wmic)) return wmic;
    bool have_wmic = !gf4_is_null(wmic);
    json_free(wmic);
    json_t *result = NULL;
    if (have_wmic) {
        json_t *ba = json_object();
        json_set(ba, "kind", json_string("wmic"));
        json_set(ba, "timeout", json_number(10));
        json_set(ba, "errors", json_string("ignore"));
        result = gf4_callkw(s, "bounded_probe_run", ba);
        if (gf4_is_raise(result)) return result;
    }
    bool bad = (!result || gf4_is_null(result) ||
                gf4_num(json_obj_get(result, "returncode"), -1) != 0 ||
                !gf4_jtruth(json_obj_get(result, "stdout")));
    if (bad) {
        if (result) json_free(result);
        json_t *pa = json_object(); json_set(pa, "name", json_string("powershell"));
        json_t *ps = gf4_callkw(s, "which", pa);
        if (gf4_is_raise(ps)) return ps;
        bool have_ps = gf4_jtruth(ps);
        json_free(ps);
        if (!have_ps) {
            json_t *pa2 = json_object(); json_set(pa2, "name", json_string("pwsh"));
            json_t *pw = gf4_callkw(s, "which", pa2);
            if (gf4_is_raise(pw)) return pw;
            bool have_pw = gf4_jtruth(pw);
            json_free(pw);
            if (!have_pw) return NULL;
        }
        json_t *ba = json_object(); json_set(ba,"kind",json_string("powershell"));
        json_set(ba,"timeout",json_number(15)); json_set(ba,"errors",json_string("ignore"));
        result = gf4_callkw(s, "bounded_probe_run", ba);
        if (gf4_is_raise(result)) return result;
        if (!result || gf4_is_null(result)) return NULL;
    }
    double rc = gf4_num(json_obj_get(result, "returncode"), -1);
    const json_t *so = json_obj_get(result, "stdout");
    bool so_null = !so || so->type == JSON_NULL;
    json_t *ret = (rc != 0 || so_null) ? NULL : json_copy(so);
    json_free(result);
    return ret;
}


/* PoP: gf4_s6_gateway_snapshot @ hermes_cli/gateway.py:_s6_gateway_snapshot */
static json_t *gf4_s6_gateway_snapshot(const json_t *in, const gf4_seams_t *s) {
    json_t *dm = gf4_call0(s, "detect_manager");
    if (gf4_is_raise(dm)) return dm;
    const char *kind = gf4_jstr(dm); bool is_s6 = kind && !strcmp(kind, "s6"); json_free(dm);
    if (!is_s6) return NULL;
    json_t *pn = gf4_call0(s, "profile_name");
    if (gf4_is_raise(pn)) return pn;
    char *svc = gf4_xsf("gateway-%s", gf4_cstr(pn, "")); json_free(pn);
    json_t *mgr = gf4_call0(s, "get_manager");
    if (gf4_is_raise(mgr)) { free(svc); return mgr; }
    json_free(mgr);
    bool installed = false, running = false;
    json_t *sd = gf4_call0(s, "mgr_scandir");
    if (gf4_is_raise(sd)) {
        if (gf4_caught(sd, "Exception")) json_free(sd);
        else { free(svc); return sd; }
    } else {
        if (!gf4_is_null(sd)) {
            char *pd = gf4_pathjoin(gf4_cstr(sd, ""), svc);
            json_t *da = json_object(); json_set(da, "path", json_string(pd));
            json_t *ir = gf4_callkw(s, "dir_isdir", da);
            if (gf4_is_raise(ir)) {
                if (gf4_caught(ir, "Exception")) json_free(ir);
                else { free(svc); free(pd); json_free(sd); return ir; }
            } else { installed = gf4_jtruth(ir); json_free(ir); }
            free(pd);
        }
        json_free(sd);
    }
    if (installed) {
        json_t *ra = json_object(); json_set(ra, "service", json_string(svc));
        json_t *rr = gf4_callkw(s, "mgr_is_running", ra);
        if (gf4_is_raise(rr)) {
            if (gf4_caught(rr, "Exception")) json_free(rr);
            else { free(svc); return rr; }
        } else { running = gf4_jtruth(rr); json_free(rr); }
    }
    json_t *out = json_object();
    json_set(out, "manager", json_string("s6 (container supervisor)"));
    json_set(out, "service_installed", json_bool(installed));
    json_set(out, "service_running", json_bool(running));
    json_set(out, "gateway_pids", json_copy(json_obj_get(in, "gateway_pids")));
    json_set(out, "service_scope", json_string("s6"));
    free(svc);
    return out;
}

/* PoP: gf4_reaper_exclusion_pids @ hermes_cli/gateway.py:_reaper_exclusion_pids */
static json_t *gf4_reaper_exclusion_pids(const json_t *in, const gf4_seams_t *s) {
    json_t *out = json_array();
    json_t *gp = gf4_call0(s, "getpid");
    if (gf4_is_raise(gp)) return gp;
    gf4_append_unique(out, gp); json_free(gp);
    const json_t *extra = json_obj_get(in, "extra_exclude");
    if (extra && extra->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(extra); i++) gf4_append_unique(out, json_get(extra, i));
    /* contextlib.suppress(Exception): get_service_pids */
    json_t *spa = json_object(); json_set(spa, "all_profiles", json_bool(true));
    json_t *sp = gf4_callkw(s, "service_pids", spa);
    if (gf4_is_raise(sp)) {
        if (gf4_caught(sp, "Exception")) json_free(sp);
        else { json_free(out); return sp; }
    } else {
        if (sp && sp->type == JSON_ARRAY)
            for (size_t i = 0; i < json_len(sp); i++) gf4_append_unique(out, json_get(sp, i));
        json_free(sp);
    }
    /* gateway.status block (outer try) */
    json_t *r1 = gf4_call0(s, "read_pid_record");
    if (gf4_is_raise(r1)) {
        if (gf4_caught(r1, "Exception")) { json_free(r1); return gf4_sorted_numbers(out); }
        json_free(out); return r1;
    }
    json_t *r2 = gf4_call0(s, "read_lock_record");
    if (gf4_is_raise(r2)) {
        if (gf4_caught(r2, "Exception")) { json_free(r2); return gf4_sorted_numbers(out); }
        json_free(r1); json_free(out); return r2;
    }
    const json_t *recs[2] = {r1, r2};
    json_t *pids[3] = {NULL, NULL, NULL};
    for (int i = 0; i < 2; i++) {
        json_t *pa = json_object(); json_set(pa, "rec", json_copy(recs[i]));
        json_t *pv = gf4_callkw(s, "pid_from_record", pa);
        if (gf4_is_raise(pv)) {
            if (gf4_caught(pv, "Exception")) {
                json_free(pv); json_free(r1); json_free(r2); return gf4_sorted_numbers(out);
            }
            json_free(r1); json_free(r2); json_free(out); return pv;
        }
        pids[i] = pv;
    }
    json_t *ra = json_object(); json_set(ra, "cleanup_stale", json_bool(false));
    json_t *rp = gf4_callkw(s, "running_pid", ra);
    if (gf4_is_raise(rp)) {
        /* Live: get_running_pid sits inside the outer try; a caught exception
         * aborts the whole recorded-pid block (no adds, no parent walks). */
        if (gf4_caught(rp, "Exception")) {
            json_free(rp); json_free(r1); json_free(r2);
            json_free(pids[0]); json_free(pids[1]);
            return gf4_sorted_numbers(out);
        }
        json_free(r1); json_free(r2); json_free(pids[0]); json_free(pids[1]); json_free(out); return rp;
    } else pids[2] = rp;
    json_free(r1); json_free(r2);
    /* Live collects recorded pids into a SET: duplicate values (e.g. the pid
     * record and lock record naming the same pid) are walked only once. */
    double seen[3]; int nseen = 0;
    for (int i = 0; i < 3; i++) {
        json_t *v = pids[i]; if (!v || v->type == JSON_NULL) continue;
        double pid = gf4_num(v, 0);
        if (!gf4_jtruth(v) || pid <= 0) continue;
        bool dup = false;
        for (int k = 0; k < nseen; k++) if (seen[k] == pid) { dup = true; break; }
        if (dup) continue;
        seen[nseen++] = pid;
        gf4_append_unique(out, v);
        double cur = pid;
        for (int depth = 0; depth < 64; depth++) {
            json_t *pa = json_object(); json_set(pa, "pid", json_number(cur));
            json_t *pr = gf4_callkw(s, "psutil_parent", pa);
            if (gf4_is_raise(pr)) {
                if (gf4_caught(pr, "Exception")) { json_free(pr); break; }
                json_free(pids[0]); json_free(pids[1]); json_free(pids[2]); json_free(out); return pr;
            }
            if (!pr || pr->type == JSON_NULL) { if (pr) json_free(pr); break; }
            double np = gf4_num(pr, 0); json_free(pr);
            json_t *nv = json_number(np); gf4_append_unique(out, nv); json_free(nv); cur = np;
        }
    }
    json_free(pids[0]); json_free(pids[1]); json_free(pids[2]);
    return gf4_sorted_numbers(out);
}

/* PoP: gf4_apply_startup_watchdog_config @ hermes_cli/gateway.py:_apply_startup_watchdog_config */
static json_t *gf4_apply_startup_watchdog_config(const json_t *in, const gf4_seams_t *s) {
    (void)in;
    bool bridged = false;
    json_t *cfg = gf4_call0(s, "load_config");
    if (gf4_is_raise(cfg)) { if (gf4_caught(cfg, "Exception")) json_free(cfg); else return cfg; }
    else {
        const json_t *gw = json_obj_get(cfg, "gateway");
        bool has_env_w = json_has(json_obj_get(in, "env"), "HERMES_STARTUP_WATCHDOG");
        bool has_env_t = json_has(json_obj_get(in, "env"), "HERMES_STARTUP_WATCHDOG_TIMEOUT_S");
        const json_t *swv = json_obj_get(gw, "startup_watchdog");
        bool sw_enabled = true;
        if (swv && swv->type == JSON_BOOL) sw_enabled = swv->bool_val;
        if (!has_env_w && !sw_enabled) {
            json_t *a = json_object(); json_set(a,"name",json_string("HERMES_STARTUP_WATCHDOG"));
            json_set(a,"value",json_string("0"));
            json_t *r = gf4_callkw(s, "setenv", a);
            if (gf4_is_raise(r)) {
                if (!gf4_caught(r,"Exception")) { json_free(cfg); return r; }
                json_free(r); goto arm_phase;
            }
            json_free(r);
        }
        const json_t *tmo = json_obj_get(gw, "startup_watchdog_timeout_seconds");
        if (!has_env_t && tmo && tmo->type != JSON_NULL) {
            char *tv = gf4_numstr(tmo);
            json_t *a = json_object(); json_set(a,"name",json_string("HERMES_STARTUP_WATCHDOG_TIMEOUT_S"));
            json_set(a,"value",json_string(tv)); free(tv);
            json_t *r = gf4_callkw(s, "setenv", a);
            if (gf4_is_raise(r)) {
                if (!gf4_caught(r,"Exception")) { json_free(cfg); return r; }
                json_free(r); goto arm_phase;
            }
            json_free(r); bridged = true;
        }
        json_free(cfg);
    }
arm_phase:
    json_t *d = gf4_call0(s, "sw_disabled");
    if (gf4_is_raise(d)) {
        if (gf4_caught(d, "Exception")) { json_free(d); return NULL; }
        return d;
    }
    bool disabled = gf4_jtruth(d); json_free(d);
    if (disabled) {
        json_t *r = gf4_call0(s, "sw_disarm");
        if (gf4_is_raise(r)) { if (gf4_caught(r,"Exception")) { json_free(r); return NULL; } return r; }
        json_free(r);
    } else {
        if (bridged) {
            json_t *r = gf4_call0(s, "sw_disarm");
            if (gf4_is_raise(r)) { if (gf4_caught(r,"Exception")) { json_free(r); return NULL; } return r; }
            json_free(r);
        }
        json_t *r = gf4_call0(s, "sw_arm");
        if (gf4_is_raise(r)) { if (gf4_caught(r,"Exception")) { json_free(r); return NULL; } return r; }
        json_free(r);
    }
    return NULL;
}

/* PoP: gf4_exit_diag @ hermes_cli/gateway.py:_make_exit_diag.<locals>._exit_diag */
static json_t *gf4_exit_diag(const json_t *in, const gf4_seams_t *s) {
    const json_t *env = json_obj_get(in, "env");
    const char *flag = json_get_str(env, "HERMES_GATEWAY_EXIT_DIAG", "1");
    if (strcmp(flag, "1") != 0) return NULL;
    json_t *h = gf4_call0(s, "get_hermes_home");
    if (gf4_is_raise(h)) { if (gf4_caught(h,"Exception")) { json_free(h); return NULL; } return h; }
    char *logdir = gf4_pathjoin(gf4_cstr(h, ""), "logs"); json_free(h);
    json_t *ma = json_object(); json_set(ma,"path",json_string(logdir));
    json_set(ma,"parents",json_bool(true)); json_set(ma,"exist_ok",json_bool(true));
    free(logdir);
    json_t *mr = gf4_callkw(s, "mkdir", ma);
    if (gf4_is_raise(mr)) { if (gf4_caught(mr,"Exception")) { json_free(mr); return NULL; } return mr; }
    json_free(mr);
    json_t *line = json_object();
    json_t *ts = gf4_call0(s, "timestamp");
    if (gf4_is_raise(ts)) { if (gf4_caught(ts,"Exception")) { json_free(ts); json_free(line); return NULL; } json_free(line); return ts; }
    json_set(line, "ts", json_copy(ts)); json_free(ts);
    json_set(line, "tag", json_copy(json_obj_get(in, "tag")));
    json_t *pid = gf4_call0(s, "getpid");
    if (gf4_is_raise(pid)) { if (gf4_caught(pid,"Exception")) { json_free(pid); json_free(line); return NULL; } json_free(line); return pid; }
    json_set(line, "pid", json_copy(pid)); json_free(pid);
    json_set(line, "python", json_copy(json_obj_get(in, "python")));
    json_set(line, "platform", json_copy(json_obj_get(in, "platform")));
    const json_t *extra = json_obj_get(in, "extra");
    if (extra && extra->type == JSON_OBJECT)
        for (size_t i = 0; i < extra->c.count; i++) json_set(line, extra->c.keys[i], json_copy(extra->c.items[i]));
    json_t *da = json_object(); json_set(da, "line", json_copy(line));
    json_free(line);
    json_t *txt = gf4_callkw(s, "json_dumps", da);
    if (gf4_is_raise(txt)) { if (gf4_caught(txt,"Exception")) { json_free(txt); return NULL; } return txt; }
    char *full = gf4_xsf("%s\n", gf4_cstr(txt, ""));
    json_free(txt);
    json_t *wa = json_object(); json_set(wa, "text", json_string(full)); free(full);
    json_t *wr = gf4_callkw(s, "write", wa);
    if (gf4_is_raise(wr)) { if (gf4_caught(wr,"Exception")) { json_free(wr); return NULL; } return wr; }
    json_free(wr);
    return NULL;
}


static bool gf4_float_parse(const char *s, double *out) {
    if (!s || !*s) return false;
    char *end = NULL; errno = 0;
    double d = strtod(s, &end);
    if (end == s) return false;
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end) return false;
    *out = d; return true;
}

/* PoP: gf4_respawn_storm_backoff @ hermes_cli/gateway.py:_respawn_storm_backoff */
static json_t *gf4_respawn_storm_backoff(const json_t *in, const gf4_seams_t *s) {
    long long max_starts = 5; double win = 120.0;
    json_t *cfg = gf4_call0(s, "load_config");
    if (gf4_is_raise(cfg)) {
        if (!gf4_caught(cfg,"Exception")) return cfg;
        json_free(cfg);
    } else {
        const json_t *gw = json_obj_get(cfg, "gateway");
        const json_t *rs = json_obj_get(gw, "respawn_storm");
        if (rs && rs->type == JSON_OBJECT) {
            const json_t *m = json_obj_get(rs, "max_starts");
            if (m && m->type == JSON_NUMBER) max_starts = (long long)m->num_val;
            const json_t *w = json_obj_get(rs, "window_seconds");
            if (w && w->type == JSON_NUMBER) win = w->num_val;
        }
        json_free(cfg);
    }
    const json_t *env = json_obj_get(in, "env");
    const json_t *mv = json_obj_get(env, "HERMES_GATEWAY_MAX_STARTS");
    if (mv && mv->type == JSON_STRING) {
        long long v; if (gf4_int_parse(gf4_jstr(mv), &v)) max_starts = v;
    }
    const json_t *wv = json_obj_get(env, "HERMES_GATEWAY_START_WINDOW_S");
    if (wv && wv->type == JSON_STRING) {
        double v; if (gf4_float_parse(gf4_jstr(wv), &v)) win = v;
    }
    json_t *ret = NULL;
    if (max_starts > 0) {
        json_t *a = json_object();
        json_set(a,"max_starts",json_number((double)max_starts));
        json_set(a,"window_s",json_number(win));
        json_t *storm = gf4_callkw(s, "record_storm", a);
        if (gf4_is_raise(storm)) {
            if (gf4_caught(storm,"Exception")) {
                char *msg = gf4_xsf("%s", gf4_cstr(json_obj_get(storm,"msg"), ""));
                json_t *da = json_object();
                json_set(da,"fmt",json_string("respawn-storm breaker check failed (non-fatal): %s"));
                json_t *args = json_array(); json_append(args, json_string(msg)); free(msg);
                json_set(da,"args",args);
                json_t *dr = gf4_callkw(s, "logger_debug", da);
                if (gf4_is_raise(dr)) { ret = dr; json_free(storm); goto done_respawn; }
                if (dr) json_free(dr);
                json_free(storm);
            } else ret = storm;
        } else if (!gf4_is_null(storm)) {
            double cnt = gf4_num(json_obj_get(storm,"count"),0);
            double ws  = gf4_num(json_obj_get(storm,"window_s"),0);
            double bs  = gf4_num(json_obj_get(storm,"backoff_s"),0);
            json_t *wa = json_object();
            json_set(wa,"fmt",json_string("Gateway (re)started %d times in %.0fs \xe2\x80\x94 backing off %.0fs to break a respawn storm."));
            json_t *args = json_array();
            json_append(args, json_number(cnt)); json_append(args, json_number(ws)); json_append(args, json_number(bs));
            json_set(wa,"args",args);
            json_t *wr = gf4_callkw(s, "logger_warning", wa);
            if (gf4_is_raise(wr)) { if (!gf4_caught(wr,"Exception")) { json_free(storm); ret = wr; goto done_respawn; } json_free(wr); }
            else json_free(wr);
            json_t *ka = json_object(); json_set(ka,"extra_s",json_number(bs));
            json_t *kr = gf4_callkw(s, "kick_watchdog", ka);
            if (gf4_is_raise(kr)) { if (!gf4_caught(kr,"Exception")) { json_free(storm); ret = kr; goto done_respawn; } json_free(kr); }
            else json_free(kr);
            json_t *sa = json_object(); json_set(sa,"seconds",json_number(bs));
            json_t *sr = gf4_callkw(s, "sleep", sa);
            if (gf4_is_raise(sr)) { if (!gf4_caught(sr,"Exception")) { json_free(storm); ret = sr; goto done_respawn; } json_free(sr); }
            else json_free(sr);
            json_free(storm);
        } else if (storm) json_free(storm);
    }
done_respawn:
    return ret;
}

/* PoP: gf4_install_systemd_from_cli @ hermes_cli/gateway.py:_install_systemd_from_cli */
static json_t *gf4_install_systemd_from_cli(const json_t *in, const gf4_seams_t *s) {
    json_t *wr = gf4_call0(s, "is_wsl");
    if (gf4_is_raise(wr)) return wr;
    bool wsl = gf4_jtruth(wr); json_free(wr);
    if (wsl) {
        json_t *a = json_object();
        json_set(a,"text",json_string("WSL detected \xe2\x80\x94 systemd services may not survive WSL restarts."));
        json_t *r = gf4_callkw(s,"print_warning",a);
        if (gf4_is_raise(r)) return r; json_free(r);
        json_t *ia = json_object();
        json_t *lines = json_array();
        json_append(lines, json_string("  Consider running in foreground instead: hermes gateway run"));
        json_append(lines, json_string("  Or use tmux/screen for persistence: tmux new -s hermes 'hermes gateway run'"));
        json_set(ia,"lines",lines);
        r = gf4_callkw(s,"info_lines",ia);
        if (gf4_is_raise(r)) return r; json_free(r);
        json_t *pa2 = json_object(); json_set(pa2,"text",json_string(""));
        r = gf4_callkw(s,"print",pa2);
        if (gf4_is_raise(r)) return r; json_free(r);
    }
    const char *kind = gf4_cstr(json_obj_get(in,"stdin_kind"), "none");
    bool non_interactive = true;
    if (!strcmp(kind,"tty")) non_interactive = !gf4_jtruth(json_obj_get(in,"stdin_isatty"));
    const json_t *args = json_obj_get(in, "args");
    /* nested _flag */
    json_t *start_now = NULL, *start_on_login = NULL;
    const json_t *v = json_obj_get(args,"start_now");
    if (v && v->type != JSON_NULL) start_now = json_copy(v);
    else if (non_interactive) start_now = json_bool(true);
    else {
        json_t *pa = json_object(); json_set(pa,"question",json_string("Start the gateway now after installing the service?"));
        json_set(pa,"default",json_bool(true));
        start_now = gf4_callkw(s,"prompt_yes_no",pa);
        if (gf4_is_raise(start_now)) return start_now;
    }
    v = json_obj_get(args,"start_on_login");
    if (v && v->type != JSON_NULL) start_on_login = json_copy(v);
    else if (non_interactive) start_on_login = json_bool(true);
    else {
        json_t *pa = json_object(); json_set(pa,"question",json_string("Start the gateway automatically on login/boot with systemd?"));
        json_set(pa,"default",json_bool(true));
        start_on_login = gf4_callkw(s,"prompt_yes_no",pa);
        if (gf4_is_raise(start_on_login)) { json_free(start_now); return start_on_login; }
    }
    bool start_now_bool = gf4_jtruth(start_now);
    json_t *ia = json_object();
    GF4_JARG(ia,"force",in,json_bool(false));
    GF4_JARG(ia,"system",in,json_bool(false));
    json_set(ia,"run_as_user",json_copy(json_obj_get(in,"run_as_user")));
    json_set(ia,"enable_on_startup",json_copy(start_on_login));
    json_set(ia,"non_interactive",json_bool(non_interactive));
    GF4_JARG(ia,"force_unit_path",args,json_bool(false));
    json_t *ir = gf4_callkw(s,"systemd_install",ia);
    json_free(start_now); json_free(start_on_login);
    if (gf4_is_raise(ir)) return ir; json_free(ir);
    if (start_now_bool) {
        json_t *sa = json_object(); json_set(sa,"system",json_copy(json_obj_get(in,"system")));
        json_t *sr = gf4_callkw(s,"systemd_start",sa);
        if (gf4_is_raise(sr)) return sr; json_free(sr);
    }
    return NULL;
}


/* PoP: gf4_legacy_launchd_labels_for_install @ hermes_cli/gateway.py:legacy_launchd_labels_for_install */
static json_t *gf4_legacy_launchd_labels_for_install(const json_t *in, const gf4_seams_t *s) {
    json_t *hd = gf4_call0(s, "pw_dir");
    if (gf4_is_raise(hd)) {
        if (gf4_caught(hd,"Exception")) { json_free(hd); return json_array(); }
        return hd;
    }
    char *home = strdup(gf4_cstr(hd, "")); json_free(hd);
    json_t *rd = gf4_call0(s, "root");
    if (gf4_is_raise(rd)) {
        if (gf4_caught(rd,"Exception")) { json_free(rd); free(home); return json_array(); }
        free(home); return rd;
    }
    char *root = strdup(gf4_cstr(rd, "")); json_free(rd);
    char *a1 = gf4_pathjoin(home, "Library");
    char *agents = gf4_pathjoin(a1, "LaunchAgents"); free(a1);
    json_t *ia = json_object(); json_set(ia,"path",json_string(agents));
    json_t *ir = gf4_callkw(s,"is_dir",ia);
    if (gf4_is_raise(ir)) { free(agents); free(root); free(home); return ir; }
    bool isdir = gf4_jtruth(ir); json_free(ir);
    if (!isdir) { free(agents); free(root); free(home); return json_array(); }
    json_t *ga = json_object(); json_set(ga,"path",json_string(agents));
    json_set(ga,"pattern",json_string("ai.hermes.gateway*.plist"));
    json_t *gl = gf4_callkw(s,"glob",ga);
    if (gf4_is_raise(gl)) { free(agents); free(root); free(home); return gl; }
    json_t *paths = gf4_sorted_strings(json_copy(json_obj_get(gl,"paths")));
    json_free(gl);
    json_t *out = json_array();
    const json_t *excl = json_obj_get(in,"exclude");
    size_t n = (paths && paths->type==JSON_ARRAY) ? json_len(paths) : 0;
    for (size_t i = 0; i < n; i++) {
        const char *path = gf4_jstr(json_get(paths,i)); if (!path) continue;
        json_t *pa = json_object(); json_set(pa,"path",json_string(path));
        json_t *data = gf4_callkw(s,"read_plist",pa);
        if (gf4_is_raise(data)) {
            if (gf4_caught(data,"Exception")) { json_free(data); continue; }
            json_free(paths); free(agents); free(root); free(home); return data;
        }
        const json_t *label = json_obj_get(data,"Label");
        const json_t *envv = json_obj_get(json_obj_get(data,"EnvironmentVariables"),"HERMES_HOME");
        bool have = label && envv;
        char *pinned = have ? gf4_numstr(envv) : NULL;
        json_t *rel = NULL;
        if (have) {
            json_t *ra = json_object(); json_set(ra,"pinned",json_string(pinned));
            json_set(ra,"root",json_string(root));
            json_t *rv = gf4_callkw(s,"relative_to",ra);
            if (gf4_is_raise(rv)) {
                rel = rv;
            } else {
                rel = json_copy(json_obj_get(rv,"parts")); json_free(rv);
            }
            if (gf4_is_raise(rel)) {
                if (gf4_caught(rel,"Exception")) { json_free(rel); json_free(data); free(pinned); continue; }
                json_free(rel); json_free(data); free(pinned);
                json_free(paths); free(agents); free(root); free(home); return rel;
            }
        }
        if (have && label->type == JSON_STRING && rel && rel->type == JSON_ARRAY) {
            const char *lab = label->str_val;
            bool excluded = false;
            if (excl && excl->type == JSON_ARRAY)
                for (size_t k=0;k<json_len(excl);k++) {
                    const char *e = gf4_jstr(json_get(excl,k));
                    if (e && !strcmp(lab, e)) { excluded = true; break; }
                }
            bool keep = !excluded && !strncmp(lab, "ai.hermes.gateway", 17);
            size_t rn = json_len(rel);
            const char *p0 = rn ? gf4_jstr(json_get(rel,0)) : NULL;
            bool okrel = (rn == 0) || (rn == 2 && p0 && !strcmp(p0,"profiles"));
            if (keep && okrel) json_append(out, json_copy(label));
        }
        if (rel) json_free(rel); if (pinned) free(pinned);
        json_free(data);
    }
    json_free(paths); free(agents); free(root); free(home);
    return gf4_sorted_strings(out);
}

/* PoP: gf4_prepare_service_launcher @ hermes_cli/gateway.py:_prepare_service_launcher */
static json_t *gf4_prepare_service_launcher(const json_t *in, const gf4_seams_t *s) {
    const char *root0 = "/proj";
    char *root = strdup(root0);
    json_t *hm = gf4_call0(s,"get_hermes_home");
    if (gf4_is_raise(hm)) { free(root); return hm; }
    char *home = strdup(gf4_cstr(hm, "")); json_free(hm);
    char *owner_uid = NULL, *owner_user = NULL;
    bool system = gf4_jtruth(json_obj_get(in,"system"));
    if (system) {
        json_t *ia = json_object(); json_set(ia,"run_as_user",json_copy(json_obj_get(in,"run_as_user")));
        json_t *ident = gf4_callkw(s,"sys_identity",ia);
        if (gf4_is_raise(ident)) { free(root); free(home); return ident; }
        const json_t *vals = json_obj_get(ident,"values");
        owner_user = strdup(gf4_cstr(json_get(vals,0), ""));
        char *home_dir = strdup(gf4_cstr(json_get(vals,2), ""));
        owner_uid = gf4_numstr(json_get(vals,3));
        json_free(ident);
        json_t *ra = json_object(); json_set(ra,"root",json_string(root)); json_set(ra,"home_dir",json_string(home_dir));
        json_t *rr = gf4_callkw(s,"remap_path",ra);
        if (gf4_is_raise(rr)) { free(home_dir); free(owner_user); free(owner_uid); free(root); free(home); return rr; }
        free(root); root = strdup(gf4_cstr(rr, "")); json_free(rr);
        json_t *ta = json_object(); json_set(ta,"home_dir",json_string(home_dir));
        json_t *tr = gf4_callkw(s,"target_home",ta);
        if (gf4_is_raise(tr)) { free(home_dir); free(owner_user); free(owner_uid); free(root); free(home); return tr; }
        free(home); home = strdup(gf4_cstr(tr, "")); json_free(tr);
        free(home_dir);
    }
    json_t *oa = json_object(); json_set(oa,"home",json_string(home));
    json_t *token = gf4_callkw(s,"set_override",oa);
    if (gf4_is_raise(token)) { free(owner_user); free(owner_uid); free(root); free(home); return token; }
    json_t *ret = NULL;
    json_t *pa = json_object(); json_set(pa,"root",json_string(root));
    json_t *pub = gf4_callkw(s,"publishes",pa);
    if (gf4_is_raise(pub)) {
        ret = pub; goto fin_override;
    }
    bool publish = gf4_jtruth(pub); json_free(pub);
    if (!publish) goto fin_override;
    char *a1 = gf4_pathjoin(root, ".hermes");
    char *local = gf4_pathjoin(a1, "bin"); free(a1);
    json_t *ea = json_object(); json_set(ea,"root",json_string(root)); json_set(ea,"local",json_string(local));
    json_t *el = gf4_callkw(s,"ensure_launchers",ea);
    if (gf4_is_raise(el)) { ret = el; free(local); goto fin_override; }
    json_t *paths = json_copy(json_obj_get(el,"paths")); json_free(el);
    size_t np = (paths && paths->type==JSON_ARRAY) ? json_len(paths) : 0;
    if (np != 2) {
        json_free(paths); free(local);
        ret = gf4_raise_spec("RuntimeError", "Could not publish the gateway installation launcher");
        goto fin_override;
    }
    if (owner_user) {
        json_t *ga = json_object(); json_set(ga,"username",json_string(owner_user));
        json_t *gidv = gf4_callkw(s,"pwnam_gid",ga);
        if (gf4_is_raise(gidv)) { ret = gidv; json_free(paths); free(local); goto fin_override; }
        char *gidc = gf4_numstr(gidv); json_free(gidv);
        char *parent = gf4_pathparent(local);
        const char *seq[4]; size_t nseq = 0;
        seq[nseq++] = parent; seq[nseq++] = local;
        for (size_t i=0;i<np && nseq<4;i++) seq[nseq++] = gf4_cstr(json_get(paths,i),"");
        for (size_t i=0;i<nseq;i++) {
            json_t *ca = json_object(); json_set(ca,"path",json_string(seq[i]));
            json_set(ca,"uid",json_number(owner_uid ? atoll(owner_uid) : 0));
            json_set(ca,"gid",json_number(atoll(gidc)));
            json_t *cr = gf4_callkw(s,"chown",ca);
            if (gf4_is_raise(cr)) { ret = cr; break; }
            json_free(cr);
        }
        free(gidc); free(parent);
        json_free(paths); free(local);
    } else { json_free(paths); free(local); }
fin_override:
    {
        json_t *ra = json_object(); json_set(ra,"token",json_copy(token));
        json_t *rr = gf4_callkw(s,"reset_override",ra);
        if (gf4_is_raise(rr)) { if (!ret) ret = rr; else json_free(rr); }
        else json_free(rr);
    }
    json_free(token); free(owner_user); free(owner_uid); free(root); free(home);
    return ret;
}
