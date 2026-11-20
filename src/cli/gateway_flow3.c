/*
 * gateway_flow3.c -- FLOW port of the UNCLAIMED-remainder of
 * hermes_cli/gateway.py (LIVE authority) beyond the flow2 PoP wave.
 * Self-contained: libjson only. All functions static gf3_*; the harness
 * (#include "cli/gateway_flow3.c") wires the universal seam to scripted
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
 * caught by gf3_caught(r, "Exception") exactly as the C walk predicts.
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
#include "json.h"

/* -- universal seam + trace -- */

typedef struct gf3_trace { json_t *events; } gf3_trace_t;

typedef struct gf3_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} gf3_seams_t;

static void gf3_trace_add(gf3_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static void gf3_trace_free(gf3_trace_t *t) {
    if (t->events) { json_free(t->events); t->events = NULL; }
}
static json_t *gf3_call(const gf3_seams_t *s, const char *name, const json_t *args) {
    return s->call(s->ctx, name, args);
}
static json_t *gf3_call0(const gf3_seams_t *s, const char *name) {
    json_t *a = json_object();
    json_t *r = gf3_call(s, name, a);
    json_free(a);
    return r;
}
static json_t *gf3_call1(const gf3_seams_t *s, const char *name, const char *k, const json_t *v) {
    json_t *a = json_object();
    json_set(a, k, v ? json_copy(v) : json_null());
    json_t *r = gf3_call(s, name, a);
    json_free(a);
    return r;
}

/* -- JSON value helpers -- */

static bool gf3_jtruth(const json_t *v) {
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
static const char *gf3_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static bool gf3_is_null(const json_t *v) { return !v || v->type == JSON_NULL; }

static char *gf3_xsf(const char *fmt, ...) {
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
typedef struct { char *p; size_t len, cap; } gf3_buf;
static void gf3_binit(gf3_buf *b) { b->p = NULL; b->len = 0; b->cap = 0; }
static void gf3_bfree(gf3_buf *b) { free(b->p); b->p = NULL; b->len = b->cap = 0; }
static void gf3_bput(gf3_buf *b, const char *s, size_t n) {
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
static void gf3_bputs(gf3_buf *b, const char *s) { gf3_bput(b, s, strlen(s)); }
static char *gf3_btake(gf3_buf *b) {
    if (!b->p) return strdup("");
    char *r = b->p; b->p = NULL; b->len = b->cap = 0; return r;
}

/* -- exception model -- */

static const char *gf3_raise_class(const json_t *r) {
    if (r && r->type == JSON_OBJECT) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (c && c->type == JSON_STRING) return c->str_val;
    }
    return NULL;
}
static bool gf3_is_raise(const json_t *r) { return gf3_raise_class(r) != NULL; }

static json_t *gf3_raise_spec(const char *cls, const char *msg) {
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
typedef struct { const char *name; const char *parent; } gf3_exc_parent;
static const gf3_exc_parent GF3_EXC_PARENTS[] = {
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
static bool gf3_base_only(const char *k) {
    return !strcmp(k, "BaseException") || !strcmp(k, "KeyboardInterrupt") ||
           !strcmp(k, "SystemExit") || !strcmp(k, "GeneratorExit") ||
           !strcmp(k, "BaseExceptionGroup");
}
static bool gf3_is_exception(const json_t *r) {
    const char *k = gf3_raise_class(r);
    return k && !gf3_base_only(k);
}
/* caught(r, cls): cls appears in the kind's ancestry walk */
static bool gf3_caught(const json_t *r, const char *cls) {
    const char *k = gf3_raise_class(r);
    if (!k) return false;
    const char *cur = k;
    for (int depth = 0; cur && depth < 64; depth++) {
        if (!strcmp(cur, cls)) return true;
        if (!strcmp(cur, "BaseException")) return false;
        const char *parent = NULL;
        for (size_t i = 0; GF3_EXC_PARENTS[i].name; i++) {
            if (!strcmp(cur, GF3_EXC_PARENTS[i].name)) {
                parent = GF3_EXC_PARENTS[i].parent;
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

static bool gf3_isspace_ascii(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == '\v' || c == '\f' || (c >= 0x1c && c <= 0x1f);
}
/* Python str.strip() over the ASCII White_Space set; returns new string. */
static char *gf3_strip(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && gf3_isspace_ascii((unsigned char)s[a])) a++;
    while (b > a && gf3_isspace_ascii((unsigned char)s[b - 1])) b--;
    char *r = malloc(b - a + 1);
    if (!r) return strdup("");
    memcpy(r, s + a, b - a);
    r[b - a] = '\0';
    return r;
}
/* split on ASCII whitespace, no empty tokens (Python str.split(None)).
   returns malloc'd char*[] (caller frees each + array); *out_n set. */
static char **gf3_split_ws(const char *s, size_t *out_n) {
    *out_n = 0;
    if (!s) return NULL;
    char **toks = NULL;
    size_t cap = 0, n = 0;
    const char *p = s;
    while (*p) {
        while (*p && gf3_isspace_ascii((unsigned char)*p)) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && !gf3_isspace_ascii((unsigned char)*p)) p++;
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
static void gf3_toks_free(char **t, size_t n) {
    if (!t) return;
    for (size_t i = 0; i < n; i++) free(t[i]);
    free(t);
}
/* Python int() on ASCII decimal with optional sign + between-digit underscores.
   Whitespace on both ends is consumed. Returns true and sets *out. */
static bool gf3_int_parse(const char *s, long long *out) {
    if (!s) return false;
    const char *p = s;
    while (*p && gf3_isspace_ascii((unsigned char)*p)) p++;
    if (!*p) return false;
    bool neg = false;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    if (!*p) return false;
    gf3_buf b; gf3_binit(&b);
    bool prev_digit = false, ok = true;
    while (*p) {
        if (*p >= '0' && *p <= '9') { gf3_bput(&b, p, 1); prev_digit = true; p++; }
        else if (*p == '_' && prev_digit && p[1] >= '0' && p[1] <= '9') {
            prev_digit = false; p++;   /* underscore between digits only */
        } else break;
    }
    if (!prev_digit) ok = false;
    while (*p && gf3_isspace_ascii((unsigned char)*p)) p++;
    if (*p != '\0') ok = false;
    if (ok && b.len) {
        long long v = 0;
        for (size_t i = 0; i < b.len; i++) {
            if (v > (LLONG_MAX - (b.p[i] - '0')) / 10) { ok = false; break; }
            v = v * 10 + (b.p[i] - '0');
        }
        if (ok) *out = neg ? -v : v;
    }
    gf3_bfree(&b);
    return ok;
}
static bool gf3_isdigit_ascii(const char *s) {
    if (!s || !*s) return false;
    for (const char *p = s; *p; p++) if (*p < '0' || *p > '9') return false;
    return true;
}

/* ========================================================================
 * FLOW cores (all static gf3_*)
 * ======================================================================== */

/* PoP: gf3_parse_ps_line @ hermes_cli/gateway.py:_parse_ps_line */
static json_t *gf3_parse_ps_line(const json_t *in) {
    const char *line = json_get_str(in, "line", "");
    char *s = gf3_strip(line);
    if (!*s || strstr(s, "grep")) { free(s); return json_null(); }
    /* split(None, 1) */
    size_t a = 0, b = strlen(s);
    while (a < b && gf3_isspace_ascii((unsigned char)s[a])) a++;
    const char *first = s + a;
    size_t flen = 0;
    while (a + flen < b && !gf3_isspace_ascii((unsigned char)s[a + flen])) flen++;
    char *p0 = malloc(flen + 1);
    if (p0) { memcpy(p0, first, flen); p0[flen] = '\0'; }
    const char *rest_start = s + a + flen;
    while (*rest_start && gf3_isspace_ascii((unsigned char)*rest_start)) rest_start++;
    bool has_second = *rest_start != '\0';
    if (has_second) {
        long long pid;
        if (gf3_int_parse(p0, &pid)) {
            json_t *r = json_array();
            json_append(r, json_number((double)pid));
            json_append(r, json_string(rest_start));
            free(p0); free(s);
            return r;
        }
    }
    free(p0);
    /* aux branch: >10 tokens and parts[1].isdigit() */
    size_t n = 0;
    char **aux = gf3_split_ws(s, &n);
    if (n > 10 && aux && gf3_isdigit_ascii(aux[1])) {
        long long pid = 0;
        if (gf3_int_parse(aux[1], &pid)) {
            gf3_buf joined; gf3_binit(&joined);
            for (size_t i = 10; i < n; i++) {
                if (i > 10) gf3_bput(&joined, " ", 1);
                gf3_bputs(&joined, aux[i]);
            }
            json_t *r = json_array();
            json_append(r, json_number((double)pid));
            json_append(r, json_string(joined.p ? joined.p : ""));
            gf3_bfree(&joined);
            gf3_toks_free(aux, n); free(s);
            return r;
        }
    }
    gf3_toks_free(aux, n); free(s);
    return json_null();
}

/* PoP: gf3_iter_windows_list_processes @ hermes_cli/gateway.py:_iter_windows_list_processes */
static json_t *gf3_iter_windows_list_processes(const json_t *in) {
    const char *listing = json_get_str(in, "listing", "");
    json_t *out = json_array();
    char *current = strdup("");
    const char *p = listing ? listing : "";
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        char *raw = malloc(len + 1);
        if (raw) { memcpy(raw, p, len); raw[len] = '\0'; }
        char *line = gf3_strip(raw);
        free(raw);
        if (!strncmp(line, "CommandLine=", 12)) {
            free(current);
            current = strdup(line + 12);
        } else if (!strncmp(line, "ProcessId=", 10)) {
            long long pid;
            if (gf3_int_parse(line + 10, &pid)) {
                json_t *pair = json_array();
                json_append(pair, json_number((double)pid));
                json_append(pair, json_string(current));
                json_append(out, pair);
            }
            free(current);
            current = strdup("");
        }
        free(line);
        if (!nl) break;
        p = nl + 1;
    }
    free(current);
    return out;
}

/* PoP: gf3_scm_service_field @ hermes_cli/gateway.py:_scm_service_field */
static json_t *gf3_scm_service_field(const json_t *in, const gf3_seams_t *s) {
    const char *field = json_get_str(in, "field", "");
    const char *tag = json_get_str(in, "service", "");
    json_t *a = json_object();
    json_set(a, "service", json_string(tag));
    json_set(a, "field", json_string(field));
    json_t *spec = gf3_call(s, "scm_getattr", a);
    json_free(a);
    if (gf3_is_raise(spec)) return spec;   /* no except site: propagate */
    bool callable = false;
    if (spec && spec->type == JSON_OBJECT) {
        const json_t *c = json_obj_get(spec, "callable");
        callable = gf3_jtruth(c);
    }
    json_free(spec);
    if (callable) {
        json_t *aa = json_object();
        json_set(aa, "field", json_string(field));
        json_t *r = gf3_call(s, "scm_call", aa);
        json_free(aa);
        return r;
    }
    /* non-callable getter (including a scripted value) -> service.as_dict().get(field) */
    json_t *d = gf3_call0(s, "scm_as_dict");
    if (gf3_is_raise(d)) return d;
    json_t *r = (d && d->type == JSON_OBJECT) ? json_obj_get(d, field) : NULL;
    json_t *out = r ? json_copy(r) : json_null();
    json_free(d);
    return out;
}

/* PoP: gf3_gateway_detached_env @ hermes_cli/gateway.py:_gateway_detached_env */
static json_t *gf3_gateway_detached_env(const gf3_seams_t *s) {
    json_t *a = json_object();
    json_set(a, "name", json_string("HERMES_GATEWAY_DETACHED"));
    json_t *v = gf3_call(s, "getenv", a);
    json_free(a);
    if (gf3_is_raise(v)) return v;
    json_t *b = json_object();
    json_set(b, "value", v ? json_copy(v) : json_null());
    json_free(v);
    json_t *r = gf3_call(s, "truthy_env", b);
    json_free(b);
    return r;
}

/* PoP: gf3_stdin_is_tty @ hermes_cli/gateway.py:_stdin_is_tty */
static json_t *gf3_stdin_is_tty(const json_t *in, const gf3_seams_t *s) {
    const json_t *present = json_obj_get(in, "stdin_present");
    if (present && !gf3_jtruth(present)) return json_bool(false);
    json_t *r = gf3_call0(s, "stdin_isatty");
    if (gf3_is_raise(r)) {
        if (gf3_caught(r, "ValueError") || gf3_caught(r, "OSError")) {
            json_free(r);
            return json_null();
        }
        return r;  /* BaseException passthrough */
    }
    return json_bool(gf3_jtruth(r));
}

/* PoP: gf3_home_owns_bare_service_name @ hermes_cli/gateway.py:_home_owns_bare_service_name */
static json_t *gf3_home_owns_bare_service_name(const json_t *in, const gf3_seams_t *s) {
    const char *home = json_get_str(in, "home", "");
    json_t *homes = gf3_call0(s, "native_service_homes");
    if (gf3_is_raise(homes)) return homes;
    bool found = false;
    if (gf3_is_null(homes)) {
        json_free(homes);
        return gf3_raise_spec("TypeError",
            "argument of type 'NoneType' is not a container or iterable");
    }
    if (homes && homes->type == JSON_ARRAY) {
        for (size_t i = 0; i < homes->c.count; i++) {
            const char *h = gf3_jstr(json_get(homes, i));
            if (h && !strcmp(h, home)) { found = true; break; }
        }
    }
    json_free(homes);
    if (found) return json_bool(true);   /* `or` short circuit */
    json_t *pin = gf3_call0(s, "bare_unit_pinned_home");
    if (gf3_is_raise(pin)) return pin;
    const char *pv = gf3_jstr(pin);
    bool eq = pv && !strcmp(pv, home);
    json_free(pin);
    return json_bool(eq);
}

/* PoP: gf3_cmd_setup @ hermes_cli/gateway.py:_cmd_setup */
static json_t *gf3_cmd_setup(const gf3_seams_t *s) {
    json_t *r = gf3_call0(s, "gateway_setup");
    if (gf3_is_raise(r)) return r;   /* no except site: propagate */
    json_free(r);
    return json_null();             /* live returns None */
}

/* PoP: gf3_cmd_list @ hermes_cli/gateway.py:_cmd_list */
static json_t *gf3_cmd_list(const gf3_seams_t *s) {
    json_t *r = gf3_call0(s, "gateway_list");
    if (gf3_is_raise(r)) return r;   /* no except site: propagate */
    json_free(r);
    return json_null();             /* live returns None */
}

/* PoP: gf3_cmd_migrate_legacy @ hermes_cli/gateway.py:_cmd_migrate_legacy */
static json_t *gf3_cmd_migrate_legacy(const json_t *in, const gf3_seams_t *s) {
    const json_t *args = json_obj_get(in, "args");
    bool dry = args ? gf3_jtruth(json_obj_get(args, "dry_run")) : false;
    bool yes = args ? gf3_jtruth(json_obj_get(args, "yes")) : false;
    json_t *sup = gf3_call0(s, "supports_systemd_services");
    if (gf3_is_raise(sup)) return sup;
    bool go_remove = gf3_jtruth(sup);
    json_free(sup);
    if (!go_remove) {
        json_t *mac = gf3_call0(s, "is_macos");
        if (gf3_is_raise(mac)) return mac;
        go_remove = gf3_jtruth(mac);
        json_free(mac);
        if (!go_remove) {
            json_t *pa = json_object();
            json_set(pa, "text", json_string("Legacy unit migration only applies to systemd-based Linux hosts."));
            json_t *pr = gf3_call(s, "print", pa);
            json_free(pa); json_free(pr);
            return json_null();
        }
    }
    json_t *ra = json_object();
    json_set(ra, "interactive", json_bool(!yes));
    json_set(ra, "dry_run", json_bool(dry));
    json_t *rr = gf3_call(s, "remove_legacy_hermes_units", ra);
    json_free(ra); json_free(rr);
    return json_null();
}

/* PoP: gf3_print_unserved_shared_ingress @ hermes_cli/gateway.py:_print_unserved_shared_ingress */
static json_t *gf3_print_unserved_shared_ingress(const json_t *in, const gf3_seams_t *s) {
    const char *prof = gf3_jstr(json_obj_get(in, "profile"));
    json_t *a = json_object();
    json_set(a, "profile", json_string(prof && *prof ? prof : ""));
    json_t *un = gf3_call(s, "served_unserved_platforms", a);
    json_free(a);
    if (gf3_is_raise(un)) {
        if (gf3_is_exception(un)) { json_free(un); return json_null(); }
        return un;
    }
    if (!un || un->type != JSON_OBJECT || un->c.count == 0) {
        json_free(un);
        return json_null();
    }
    json_t *pa = json_object();
    json_set(pa, "text", json_string(""));
    json_t *pr = gf3_call(s, "print", pa);
    json_free(pa); json_free(pr);
    /* sorted(unserved.items()) by platform key */
    size_t n = un->c.count;
    const char **keys = malloc(n * sizeof(const char *));
    for (size_t i = 0; i < n; i++) keys[i] = un->c.keys[i] ? un->c.keys[i] : "";
    for (size_t i = 0; i < n; i++) for (size_t j = i + 1; j < n; j++)
        if (strcmp(keys[i], keys[j]) > 0) { const char *t = keys[i]; keys[i] = keys[j]; keys[j] = t; }
    for (size_t i = 0; i < n; i++) {
        const json_t *reason = json_obj_get(un, keys[i]);
        char *txt = gf3_xsf("  ⚠ %s: %s", keys[i], gf3_jstr(reason) ? gf3_jstr(reason) : "");
        json_t *pp = json_object();
        json_set(pp, "text", json_string(txt));
        json_t *ppr = gf3_call(s, "print", pp);
        json_free(pp); json_free(ppr); free(txt);
    }
    free(keys);
    json_free(un);
    json_t *fa = json_object();
    json_set(fa, "text", json_string("  Enable it on the default profile (shared ingress serves every profile), or disable it here."));
    json_t *fr = gf3_call(s, "print", fa);
    json_free(fa); json_free(fr);
    return json_null();
}

/* PoP: gf3_print_duplicate_credential_warnings @ hermes_cli/gateway.py:_print_duplicate_credential_warnings */
static json_t *gf3_print_duplicate_credential_warnings(const gf3_seams_t *s) {
    json_t *lines = gf3_call0(s, "duplicate_credential_findings");
    if (gf3_is_raise(lines)) {
        if (gf3_is_exception(lines)) { json_free(lines); return json_null(); }
        return lines;
    }
    if (lines && lines->type == JSON_ARRAY && lines->c.count > 0) {
        json_t *pa = json_object();
        json_set(pa, "text", json_string(""));
        json_t *pr = gf3_call(s, "print", pa);
        json_free(pa); json_free(pr);
        for (size_t i = 0; i < lines->c.count; i++) {
            char *txt = gf3_xsf("⚠ %s", gf3_jstr(json_get(lines, i)) ? gf3_jstr(json_get(lines, i)) : "");
            json_t *pp = json_object();
            json_set(pp, "text", json_string(txt));
            json_t *ppr = gf3_call(s, "print", pp);
            json_free(pp); json_free(ppr); free(txt);
        }
    }
    json_free(lines);
    return json_null();
}

/* PoP: gf3_absorb_windows_console_controls @ hermes_cli/gateway.py:_absorb_windows_console_controls */
static json_t *gf3_absorb_windows_console_controls(const gf3_seams_t *s) {
    json_t *a = json_object();
    json_set(a, "sig", json_string("SIGINT"));
    json_set(a, "handler", json_string("SIG_IGN"));
    json_t *r = gf3_call(s, "signal", a);
    json_free(a);
    bool first_broken = false;
    if (gf3_is_raise(r)) {
        if (gf3_caught(r, "OSError") || gf3_caught(r, "ValueError")) { first_broken = true; json_free(r); }
        else return r;   /* BaseException passthrough */
    } else json_free(r);
    if (!first_broken) {
        json_t *hb = gf3_call0(s, "hasattr_sigbreak");
        if (gf3_is_raise(hb)) { /* hasattr itself never raises in fixtures */ json_free(hb); }
        else {
            bool has = gf3_jtruth(hb);
            json_free(hb);
            if (has) {
                json_t *b = json_object();
                json_set(b, "sig", json_string("SIGBREAK"));
                json_set(b, "handler", json_string("SIG_IGN"));
                json_t *r2 = gf3_call(s, "signal", b);
                json_free(b);
                if (gf3_is_raise(r2)) {
                    if (gf3_caught(r2, "OSError") || gf3_caught(r2, "ValueError")) json_free(r2);
                    else return r2;
                } else json_free(r2);
            }
        }
    }
    json_t *cr = gf3_call0(s, "set_console_ctrl_handler");
    if (gf3_is_raise(cr)) {
        if (gf3_caught(cr, "OSError") || gf3_caught(cr, "AttributeError")) { json_free(cr); }
        else return cr;
    } else json_free(cr);
    return json_null();
}

/* PoP: gf3_ping_loop_tick_witness @ hermes_cli/gateway.py:_ping_loop_tick_witness */
static json_t *gf3_ping_loop_tick_witness(const json_t *in, const gf3_seams_t *s) {
    long long fam = (long long)json_get_num(in, "family", 0);
    const json_t *address = json_obj_get(in, "address");
    double timeout = json_get_num(in, "timeout", 0.0);
    double tmo = timeout > 0.0 ? timeout : 0.0;
    bool sock = false, result = false, done = false;
    json_t *pending = NULL;   /* BaseException to re-raise AFTER the finally close */
    json_t *r = NULL;
    json_t *a = json_object();
    json_set(a, "family", json_number((double)fam));
    json_set(a, "sock_stream", json_number(1));
    r = gf3_call(s, "socket_new", a);
    json_free(a);
    if (gf3_is_raise(r)) {
        if (!gf3_is_exception(r)) { json_t *p = json_copy(r); json_free(r); return p; }
        json_free(r);
        return json_bool(false);   /* sock is None: finally does nothing */
    }
    json_free(r); sock = true;
    a = json_object();
    json_set(a, "timeout", json_number(tmo));
    r = gf3_call(s, "settimeout", a);
    json_free(a);
    if (gf3_is_raise(r)) {
        if (!gf3_is_exception(r)) pending = json_copy(r);
        done = true;
    }
    json_free(r);
    if (!done) {
        a = json_object();
        json_set(a, "address", address ? json_copy(address) : json_null());
        r = gf3_call(s, "connect", a);
        json_free(a);
        if (gf3_is_raise(r)) {
            if (!gf3_is_exception(r)) pending = json_copy(r);
            done = true;
        }
        json_free(r);
    }
    if (!done) {
        a = json_object();
        json_set(a, "n", json_number(1));
        r = gf3_call(s, "recv", a);
        json_free(a);
        if (gf3_is_raise(r)) {
            if (!gf3_is_exception(r)) pending = json_copy(r);
            done = true;
        } else {
            result = (r && r->type == JSON_ARRAY && r->c.count == 1 &&
                      r->c.items[0] && r->c.items[0]->type == JSON_NUMBER &&
                      r->c.items[0]->num_val == 49.0);
        }
        json_free(r);
    }
#undef GF3_PING_CALL
    /* finally: close with suppress(Exception) -- Exception caught, BaseException passes */
    if (sock) {
        json_t *cl = gf3_call0(s, "close");
        if (gf3_is_raise(cl) && !gf3_is_exception(cl)) {
            json_free(pending);
            return cl;   /* BaseException from close overrides the pending one (Python finally) */
        }
        json_free(cl);
    }
    if (pending) return pending;
    return json_bool(result);
}

/* PoP: gf3_iter_proc_cmdlines @ hermes_cli/gateway.py:_iter_proc_cmdlines */
static json_t *gf3_iter_proc_cmdlines(const json_t *in, const gf3_seams_t *s) {
    json_t *mp = gf3_call0(s, "getpid");
    if (gf3_is_raise(mp)) return mp;
    long long mypid = 0;
    if (mp && mp->type == JSON_NUMBER) mypid = (long long)mp->num_val;
    json_free(mp);
    json_t *la = json_object();
    json_set(la, "path", json_string("/proc"));
    json_t *ents = gf3_call(s, "listdir", la);
    json_free(la);
    if (gf3_is_raise(ents)) return ents;
    json_t *out = json_array();
    const json_t *ex = json_obj_get(in, "exclude_pids");
    if (!ents || ents->type != JSON_ARRAY) { json_free(ents); return out; }
    for (size_t i = 0; i < ents->c.count; i++) {
        const char *entry = gf3_jstr(json_get(ents, i));
        if (!entry || !gf3_isdigit_ascii(entry)) continue;
        long long pid = 0;
        if (!gf3_int_parse(entry, &pid)) continue;
        if (pid == mypid) continue;
        bool excluded = false;
        if (ex && ex->type == JSON_ARRAY) {
            for (size_t j = 0; j < ex->c.count; j++) {
                const json_t *e = json_get(ex, j);
                double v = (e && e->type == JSON_NUMBER) ? e->num_val : NAN;
                if (!isnan(v) && (long long)v == pid) { excluded = true; break; }
            }
        }
        if (excluded) continue;
        json_t *ra = json_object();
        json_set(ra, "pid", json_number((double)pid));
        json_t *rc = gf3_call(s, "read_cmdline", ra);
        json_free(ra);
        if (gf3_is_raise(rc)) {
            if (gf3_caught(rc, "OSError") || gf3_caught(rc, "PermissionError")) { json_free(rc); continue; }
            json_free(ents); return rc;   /* BaseException passthrough */
        }
        const char *cl = gf3_jstr(rc) ? gf3_jstr(rc) : "";
        /* str.replace("\x00", " ") */
        gf3_buf b; gf3_binit(&b);
        for (const char *p = cl; *p; p++) gf3_bput(&b, (*p == '\0') ? " " : p, 1);
        json_t *pair = json_array();
        json_append(pair, json_number((double)pid));
        json_append(pair, json_string(b.p ? b.p : ""));
        gf3_bfree(&b);
        json_append(out, pair);
        json_free(rc);
    }
    json_free(ents);
    return out;
}

/* PoP: gf3_wait_for_tcp_port_free @ hermes_cli/gateway.py:_wait_for_tcp_port_free */
static json_t *gf3_wait_for_tcp_port_free(const json_t *in, const gf3_seams_t *s) {
    const char *host = json_get_str(in, "host", "");
    double port = json_get_num(in, "port", 0);
    double timeout = json_get_num(in, "timeout", 10.0);
    json_t *a = json_object();
    json_set(a, "marker", json_string("_port_free"));
    json_set(a, "host", json_string(host));
    json_set(a, "port", json_number(port));
    json_set(a, "timeout", json_number(timeout));
    json_set(a, "interval", json_number(0.1));
    json_t *r = gf3_call(s, "poll_until", a);
    json_free(a);
    return r;
}

/* PoP: gf3_restart_argv_is_host_gateway @ hermes_cli/gateway.py:_restart_argv_is_host_gateway */
static json_t *gf3_restart_argv_is_host_gateway(const json_t *in, const gf3_seams_t *s) {
    const json_t *argv = json_obj_get(in, "argv");
    size_t n = (argv && argv->type == JSON_ARRAY) ? argv->c.count : 0;
    bool has_gateway = false;
    for (size_t i = 0; i < n; i++) {
        const char *v = gf3_jstr(json_get(argv, i));
        if (v && !strcmp(v, "gateway")) { has_gateway = true; break; }
    }
    if (!n || !has_gateway) return json_bool(false);
    const char *flags[2] = {"--profile", "-p"};
    for (int fi = 0; fi < 2; fi++) {
        long long idx = -1;
        for (size_t i = 0; i < n; i++) {
            const char *v = gf3_jstr(json_get(argv, i));
            if (v && !strcmp(v, flags[fi])) { idx = (long long)i; break; }
        }
        if (idx >= 0) {
            const char *name = "";
            if ((size_t)(idx + 1) < n) {
                const char *nx = gf3_jstr(json_get(argv, (size_t)(idx + 1)));
                if (nx) name = nx;
            }
            return json_bool(!strcmp(name, "default"));
        }
    }
    bool eq_default = false, other_profile = false;
    for (size_t i = 0; i < n; i++) {
        const char *v = gf3_jstr(json_get(argv, i));
        if (!v) continue;
        if (!strcmp(v, "--profile=default")) eq_default = true;
        if (!strncmp(v, "--profile=", 10) && strcmp(v, "--profile=default")) other_profile = true;
    }
    if (eq_default) return json_bool(true);
    if (other_profile) return json_bool(false);
    /* try 1: is_multiplex_active */
    json_t *r = gf3_call0(s, "is_multiplex_active");
    if (gf3_is_raise(r)) {
        if (!gf3_is_exception(r)) return r;
        json_free(r);
    } else {
        bool v = gf3_jtruth(r); json_free(r);
        if (v) return json_bool(true);
    }
    /* try 2: host rendezvous record */
    json_t *a = json_object();
    json_set(a, "role", json_string("gateway"));
    json_t *rec = gf3_call(s, "hr_read_record", a);
    json_free(a);
    if (gf3_is_raise(rec)) {
        if (!gf3_is_exception(rec)) return rec;
        json_free(rec);
    } else {
        if (rec && rec->type == JSON_OBJECT) {
            bool live = gf3_jtruth(json_obj_get(rec, "live_owner"));
            const json_t *profs = json_obj_get(rec, "profiles");
            size_t pn = (profs && profs->type == JSON_ARRAY) ? profs->c.count : 0;
            if (live && pn > 1) { json_free(rec); return json_bool(true); }
        }
        json_free(rec);
    }
    /* try 3: default-root comparison */
    json_t *h = gf3_call0(s, "get_hermes_home");
    if (gf3_is_raise(h)) {
        if (!gf3_is_exception(h)) return h;
        json_free(h); return json_bool(false);
    }
    json_t *d = gf3_call0(s, "get_default_hermes_root");
    if (gf3_is_raise(d)) {
        if (!gf3_is_exception(d)) { json_free(h); return d; }
        json_free(d); json_free(h); return json_bool(false);
    }
    const char *hs = gf3_jstr(h), *ds = gf3_jstr(d);
    bool eq = hs && ds && !strcmp(hs, ds);
    json_free(h); json_free(d);
    return json_bool(eq);
}

/* PoP: gf3_gw_windows @ hermes_cli/gateway.py:_gw_windows */
static json_t *gf3_gw_windows(const gf3_seams_t *s) {
    return gf3_call0(s, "import_gateway_windows");
}
