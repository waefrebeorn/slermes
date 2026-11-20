/*
 * gateway_multiplex_flow2.c — the UNCLAIMED remainder of the gateway
 * multiplex flow family (LIVE authority:
 * C:/Users/eman5/hermes-agent/hermes_cli/gateway_multiplex_served.py and
 * C:/Users/eman5/hermes-agent/hermes_cli/gateway_runtime_multiplex.py).
 *
 * TARGET NOTE: the task named hermes_cli/gateway_multiplex_mode.py's
 * "remainder", but a CRLF-aware PoP census shows ALL 20 of that module's
 * top-level defs are already claimed AND genuinely ported by
 * src/cli/gateway_enroll_flow.c (ge2_flow_*). Its remainder is EMPTY. The
 * real unclaimed multiplex surface is the two sibling modules the mode flow
 * consults: gateway_multiplex_served.py (7 defs, the live-served record
 * readers) and gateway_runtime_multiplex.py (3 defs, the named-profile
 * start-target deciders). 10 unclaimed functions ported here. PoP credits
 * the DEFINING module per audit_annotations doctrine.
 *
 * SEAM DOCTRINE (universal seam, same shape as gateway_enroll_flow):
 * every live effect — foreign-module calls, the runtime-status readers, the
 * control-socket rescan, logger.debug, and the CLAIMED sibling
 * gateway_multiplex_mode.host_serves_named_profile — enters through ONE seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` borrowed JSON object
 * (null-valued keys dropped on BOTH sides); result NEW json_t* (NULL == None,
 * never a JSON_NULL node); {"__raise__": cls, "msg": msg} == a raised
 * exception. The harness records {"ev","args","ret"} per call in order; the
 * oracle's recording stubs emit the identical shape driving LIVE Python.
 *
 * RETURN CONTRACT: every gm2_flow_* returns a NEW json_t* (caller frees):
 * NULL == Python None; a {"__raise__","msg"} object == a raised exception,
 * propagated or caught exactly as the live def does. Path results project to
 * their PurePosixPath posix string. BaseException (SystemExit /
 * KeyboardInterrupt / GeneratorExit) passes through every except Exception
 * site (notify_multiplexer_profiles_changed). Scripted raise specs propagate
 * VERBATIM.
 *
 * INTERNAL COMPOSITION runs real on both sides: ported targets call each
 * other directly (C) / via the shared extracted namespace (oracle); only
 * external effects + the claimed host_serves_named_profile sibling are seamed.
 *
 * STATE: none of the 10 defs mutate module globals or caller objects; every
 * case echoes state {}.
 *
 * Path model: PurePosixPath over POSIX strings. Parts split on '/', dropping
 * '' and '.' components, keeping '..'; an exactly-two-slash leading root is
 * preserved ('//h' -> '//h', '///h' -> '/h'). .parent drops the last part
 * (a root path is its own parent); .name is the last part ('' for a root);
 * equality compares root-kind + parts. The oracle's PathDouble implements the
 * identical model, so the differential domain is self-consistent.
 *
 * Fixture domain (kept identical on both sides, documented exclusions):
 * - path params (home / default_root) are POSIX strings or null; a null home
 *   raises the Path(None) TypeError; root scripts (get_default_hermes_root)
 *   are str|null only;
 * - str(p) comprehensions (served_profiles elements, error_message,
 *   ingress_url) take SCALAR elements only — str / int / bool / null. Floats
 *   are excluded because libjson numbers carry no int/float tag (integral
 *   floats would str() as ints, diverging from Python str(1.0) == "1.0");
 *   integral floats elsewhere (timeout) compare equal via the oracle canon;
 * - served_profiles in served_profile_ingress_urls is iterated WITHOUT an
 *   isinstance guard: list -> elements, str -> ASCII chars, dict -> keys,
 *   falsy -> [], other truthy scalar -> TypeError not iterable;
 * - error_code / normalize_profile_name results that are dict/list raise the
 *   py3.14 unhashable-set TypeError; tuple membership (state) never raises;
 * - profile / profile_name / indent are str|null; urls values are str.
 *
 * Seam name table (name -> live call; args key order is part of the trace):
 *   get_default_hermes_root      hermes_constants.get_default_hermes_root() {}
 *   live_gateway_pid_for_home    gateway.status.live_gateway_pid_for_home(root) {home}
 *   read_runtime_status          gateway.status.read_runtime_status(path) {state_path}
 *   shared_listener_mirror_platforms gateway.status.shared_listener_mirror_platforms(runtime,name) {runtime,name}
 *   rescan_gateway_profiles      gateway.control_socket.rescan_gateway_profiles(root,timeout=) {root,timeout}
 *   named_profile_home           hermes_constants.named_profile_home(home) {home}
 *   normalize_profile_name       hermes_cli.profiles.normalize_profile_name(name) {name}
 *   host_serves_named_profile    gateway_multiplex_mode.host_serves_named_profile(root,home) {default_home,home}  (CLAIMED sibling, seamed)
 *   logger_debug                 logger.debug(msg,*args,exc_info=True) {msg,args,exc_info}
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>

#include "json.h"

/* ── JSON / Python-value helpers ── */

static bool gm2_jtruth(const json_t *v) {
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
static bool gm2_jpresent(const json_t *v) { return v && v->type != JSON_NULL; }
static bool gm2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool gm2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool gm2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool gm2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool gm2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static const char *gm2_jstr(const json_t *v) {
    return (gm2_is_str(v) && v->str_val) ? v->str_val : NULL;
}

static char *gm2_xsf(const char *fmt, ...) {
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

static const char *gm2_py_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_BOOL:   return "bool";
        case JSON_NUMBER: return (v->num_val == (double)(long long)v->num_val) ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY:  return "list";
        case JSON_OBJECT: return "dict";
        default:          return "object";
    }
}

/* Python == over the JSON value domain (bool==int numeric equality). */
static bool gm2_py_eq(const json_t *a, const json_t *b) {
    bool ap = gm2_jpresent(a), bp = gm2_jpresent(b);
    if (!ap || !bp) return ap == bp;
    if (gm2_is_bool(a) && gm2_is_bool(b)) return a->bool_val == b->bool_val;
    if ((gm2_is_bool(a) || gm2_is_num(a)) && (gm2_is_bool(b) || gm2_is_num(b))) {
        double av = gm2_is_bool(a) ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = gm2_is_bool(b) ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av == bv;
    }
    if (gm2_is_str(a) && gm2_is_str(b)) return strcmp(a->str_val, b->str_val) == 0;
    if (gm2_is_arr(a) && gm2_is_arr(b)) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++)
            if (!gm2_py_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (gm2_is_obj(a) && gm2_is_obj(b)) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !gm2_py_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

static char *gm2_py_float_str(double d) {
    if (d == (double)(long long)d) return gm2_xsf("%.0f", d);
    char buf[64];
    for (int prec = 15; prec <= 17; prec++) {
        snprintf(buf, sizeof(buf), "%.*g", prec, d);
        if (strtod(buf, NULL) == d) break;
    }
    return strdup(buf);
}

/* repr() over the JSON domain (ASCII strings): Python str(list)/str(dict)
 * equal repr(list)/repr(dict); scalars use their str() form. */
static char *gm2_py_repr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: return gm2_py_float_str(v->num_val);
        case JSON_STRING: {
            const char *s = v->str_val ? v->str_val : "";
            bool has_sq = strchr(s, '\'') != NULL;
            bool has_dq = strchr(s, '"') != NULL;
            char q = (has_sq && !has_dq) ? '"' : '\'';
            size_t n = strlen(s);
            char *out = malloc(n * 4 + 3);
            if (!out) return strdup("''");
            size_t k = 0;
            out[k++] = q;
            for (size_t i = 0; i < n; i++) {
                unsigned char c = (unsigned char)s[i];
                if (c == '\\' || c == (unsigned char)q) { out[k++] = '\\'; out[k++] = (char)c; }
                else if (c == '\n') { out[k++] = '\\'; out[k++] = 'n'; }
                else if (c == '\r') { out[k++] = '\\'; out[k++] = 'r'; }
                else if (c == '\t') { out[k++] = '\\'; out[k++] = 't'; }
                else if (c < 0x20) { k += (size_t)sprintf(out + k, "\\x%02x", c); }
                else out[k++] = (char)c;
            }
            out[k++] = q; out[k] = '\0';
            return out;
        }
        case JSON_ARRAY: {
            char *out = strdup("[");
            for (size_t i = 0; i < v->c.count; i++) {
                char *item = gm2_py_repr(v->c.items[i]);
                char *next = gm2_xsf("%s%s%s", out, i ? ", " : "", item);
                free(out); free(item); out = next;
            }
            char *next = gm2_xsf("%s]", out); free(out); return next;
        }
        case JSON_OBJECT: {
            char *out = strdup("{");
            for (size_t i = 0; i < v->c.count; i++) {
                json_t *kn = json_string(v->c.keys[i]);
                char *key = gm2_py_repr(kn); json_free(kn);
                char *item = gm2_py_repr(v->c.items[i]);
                char *next = gm2_xsf("%s%s%s: %s", out, i ? ", " : "", key, item);
                free(out); free(key); free(item); out = next;
            }
            char *next = gm2_xsf("%s}", out); free(out); return next;
        }
    }
    return strdup("");
}

/* str() over the JSON domain: scalars verbatim, containers via repr. */
static char *gm2_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: return gm2_py_float_str(v->num_val);
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        default:          return gm2_py_repr(v);
    }
}

/* ── PurePosixPath model ── */

typedef struct gm2_path {
    int kind;          /* 0 relative, 1 single '/' root, 2 double '//' root */
    char **parts;
    size_t n;
} gm2_path_t;

static gm2_path_t *gm2_path_parse(const char *s) {
    gm2_path_t *p = calloc(1, sizeof(*p));
    if (!p) return NULL;
    if (!s) s = "";
    if (s[0] == '/') {
        p->kind = (s[1] == '/' && s[2] != '/') ? 2 : 1;
    }
    size_t cap = 8;
    p->parts = malloc(cap * sizeof(char *));
    p->n = 0;
    const char *cur = s;
    while (*cur) {
        const char *slash = strchr(cur, '/');
        size_t len = slash ? (size_t)(slash - cur) : strlen(cur);
        if (len > 0 && !(len == 1 && cur[0] == '.')) {
            if (p->n == cap) { cap *= 2; p->parts = realloc(p->parts, cap * sizeof(char *)); }
            p->parts[p->n] = malloc(len + 1);
            memcpy(p->parts[p->n], cur, len);
            p->parts[p->n][len] = '\0';
            p->n++;
        }
        if (!slash) break;
        cur = slash + 1;
    }
    return p;
}
static void gm2_path_free(gm2_path_t *p) {
    if (!p) return;
    for (size_t i = 0; i < p->n; i++) free(p->parts[i]);
    free(p->parts); free(p);
}
static char *gm2_path_render(const gm2_path_t *p) {
    const char *root = p->kind == 2 ? "//" : (p->kind == 1 ? "/" : "");
    if (p->n == 0) return strdup(p->kind ? root : ".");
    size_t total = strlen(root) + 1;
    for (size_t i = 0; i < p->n; i++) total += strlen(p->parts[i]) + 1;
    char *out = malloc(total);
    if (!out) return strdup("");
    strcpy(out, root);
    for (size_t i = 0; i < p->n; i++) {
        if (i > 0) strcat(out, "/");
        strcat(out, p->parts[i]);
    }
    return out;
}
static gm2_path_t *gm2_path_parent(const gm2_path_t *p) {
    gm2_path_t *q = calloc(1, sizeof(*q));
    q->kind = p->kind;
    if (p->n == 0) { q->parts = malloc(sizeof(char *)); q->n = 0; return q; }
    if (p->kind != 0 && p->n == 1) { q->parts = malloc(sizeof(char *)); q->n = 0; return q; }
    q->n = p->n - 1;
    q->parts = malloc((q->n ? q->n : 1) * sizeof(char *));
    for (size_t i = 0; i < q->n; i++) q->parts[i] = strdup(p->parts[i]);
    return q;
}
static char *gm2_path_name(const gm2_path_t *p) {
    return strdup(p->n ? p->parts[p->n - 1] : "");
}
static bool gm2_path_eq(const gm2_path_t *a, const gm2_path_t *b) {
    if (a->kind != b->kind || a->n != b->n) return false;
    for (size_t i = 0; i < a->n; i++)
        if (strcmp(a->parts[i], b->parts[i]) != 0) return false;
    return true;
}
/* base / name (name a single clean component): keep base kind, append part. */
static char *gm2_path_join(const gm2_path_t *base, const char *name) {
    gm2_path_t j;
    j.kind = base->kind;
    j.n = base->n + 1;
    j.parts = malloc(j.n * sizeof(char *));
    for (size_t i = 0; i < base->n; i++) j.parts[i] = base->parts[i];
    j.parts[base->n] = (char *)name;
    char *r = gm2_path_render(&j);
    free(j.parts);
    return r;
}

/* ── trace / seam / raise plumbing ── */

typedef struct gm2_trace { json_t *events; } gm2_trace_t;
typedef struct gm2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} gm2_seams_t;

static void gm2_trace_add(gm2_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static const char *gm2_raise_class(const json_t *r) {
    if (gm2_is_obj(r)) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (gm2_is_str(c)) return c->str_val;
    }
    return NULL;
}
static bool gm2_is_raise(const json_t *r) { return gm2_raise_class(r) != NULL; }

/* Seam call that CONSUMES (frees) the freshly built args object. */
static json_t *gm2_scall(const gm2_seams_t *s, const char *name, json_t *args) {
    json_t *r = s->call(s->ctx, name, args);
    json_free(args);
    return r;
}

static json_t *gm2_raise_obj(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
static json_t *gm2_raise_attr(const char *type_name, const char *attr) {
    char *m = gm2_xsf("'%s' object has no attribute '%s'", type_name, attr);
    json_t *r = gm2_raise_obj("AttributeError", m);
    free(m);
    return r;
}
static json_t *gm2_raise_unhashable(const json_t *v) {
    const char *tn = gm2_py_type_name(v);
    char *m = gm2_xsf("cannot use '%s' as a set element (unhashable type: '%s')", tn, tn);
    json_t *r = gm2_raise_obj("TypeError", m);
    free(m);
    return r;
}
static json_t *gm2_raise_not_iterable(const json_t *v) {
    char *m = gm2_xsf("'%s' object is not iterable", gm2_py_type_name(v));
    json_t *r = gm2_raise_obj("TypeError", m);
    free(m);
    return r;
}
static json_t *gm2_raise_truediv_none(void) {
    return gm2_raise_obj("TypeError",
        "unsupported operand type(s) for /: 'NoneType' and 'str'");
}
static json_t *gm2_raise_path_none(void) {
    return gm2_raise_obj("TypeError",
        "argument should be a str or an os.PathLike object where __fspath__ returns a str, not 'NoneType'");
}
#define GM2_PROP(r) do { if (gm2_is_raise(r)) return (r); } while (0)

/* ── FULL builtin exception hierarchy (child -> parent) ── */

typedef struct { const char *child, *parent; } gm2_exc_rel_t;
static const gm2_exc_rel_t GM2_EXC_PARENTS[] = {
    {"Exception", "BaseException"},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
    {"BaseExceptionGroup", "BaseException"},
    /* ExceptionGroup inherits (BaseExceptionGroup, Exception) — `except
     * Exception` DOES catch it, so model its Exception lineage directly. */
    {"ExceptionGroup", "Exception"},
    {"PythonFinalizationError", "RuntimeError"},
    {"_IncompleteInputError", "SyntaxError"},
    {"EnvironmentError", "Exception"},
    {"IOError", "Exception"},
    {"WindowsError", "Exception"},
    {"StopIteration", "Exception"},
    {"StopAsyncIteration", "Exception"},
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
    {"SyntaxError", "Exception"},
    {"SystemError", "Exception"},
    {"TypeError", "Exception"},
    {"ValueError", "Exception"},
    {"Warning", "Exception"},
    {"FloatingPointError", "ArithmeticError"},
    {"OverflowError", "ArithmeticError"},
    {"ZeroDivisionError", "ArithmeticError"},
    {"ModuleNotFoundError", "ImportError"},
    {"IndexError", "LookupError"},
    {"KeyError", "LookupError"},
    {"UnboundLocalError", "NameError"},
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
    {"RecursionError", "RuntimeError"},
    {"IndentationError", "SyntaxError"},
    {"TabError", "IndentationError"},
    {"UnicodeError", "ValueError"},
    {"UnicodeDecodeError", "UnicodeError"},
    {"UnicodeEncodeError", "UnicodeError"},
    {"UnicodeTranslateError", "UnicodeError"},
    {"DeprecationWarning", "Warning"},
    {"PendingDeprecationWarning", "Warning"},
    {"RuntimeWarning", "Warning"},
    {"SyntaxWarning", "Warning"},
    {"UserWarning", "Warning"},
    {"FutureWarning", "Warning"},
    {"ImportWarning", "Warning"},
    {"UnicodeWarning", "Warning"},
    {"BytesWarning", "Warning"},
    {"ResourceWarning", "Warning"},
    {"EncodingWarning", "Warning"},
};
static const char *gm2_exc_parent(const char *kind) {
    for (size_t i = 0; i < sizeof(GM2_EXC_PARENTS) / sizeof(GM2_EXC_PARENTS[0]); i++)
        if (!strcmp(kind, GM2_EXC_PARENTS[i].child)) return GM2_EXC_PARENTS[i].parent;
    return NULL;
}
/* except Exception: true for every Exception descendant; the BaseException
 * trio (and BaseException itself) pass through; unknown -> custom Exception. */
static bool gm2_is_exception(const char *kind) {
    if (!kind) return false;
    if (!strcmp(kind, "BaseException") || !strcmp(kind, "SystemExit")
        || !strcmp(kind, "KeyboardInterrupt") || !strcmp(kind, "GeneratorExit"))
        return false;
    const char *k = kind;
    for (int depth = 0; depth < 32; depth++) {
        if (!strcmp(k, "Exception")) return true;
        if (!strcmp(k, "BaseException")) return false;
        const char *p = gm2_exc_parent(k);
        if (!p) return true;
        k = p;
    }
    return true;
}

/* ── args builders (null-valued keys dropped on BOTH sides) ── */

static json_t *gm2_args0(void) { return json_object(); }
static void gm2_aset_j(json_t *a, const char *k, const json_t *v) {
    if (gm2_jpresent(v)) json_set(a, k, json_copy(v));
}
static void gm2_aset_s(json_t *a, const char *k, const char *v) {
    if (v) json_set(a, k, json_string(v));
}
static void gm2_aset_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }

/* ── shared sub-helpers ── */

/* [str(p) for p in arr] — scalar element domain (no truthiness filter). */
static json_t *gm2_str_list(const json_t *arr) {
    json_t *out = json_array();
    for (size_t i = 0; i < arr->c.count; i++) {
        char *s = gm2_py_str(arr->c.items[i]);
        json_append(out, json_string(s));
        free(s);
    }
    return out;
}

/* root = get_default_hermes_root(); returns a NEW rendered posix string in
 * *out (NULL when the seam returns None). Propagates a scripted raise. */
static json_t *gm2_default_root_str(const gm2_seams_t *s, char **out) {
    *out = NULL;
    json_t *r = gm2_scall(s, "get_default_hermes_root", gm2_args0());
    GM2_PROP(r);
    if (gm2_jpresent(r)) {
        gm2_path_t *p = gm2_path_parse(gm2_jstr(r) ? gm2_jstr(r) : "");
        *out = gm2_path_render(p);
        gm2_path_free(p);
    }
    json_free(r);
    return NULL;
}

/* read_runtime_status(root / "gateway_state.json"); root NULL -> truediv
 * TypeError. Returns the seam result (caller frees); raise propagates. */
static json_t *gm2_read_state(const gm2_seams_t *s, const char *root) {
    if (!root) return gm2_raise_truediv_none();
    gm2_path_t *base = gm2_path_parse(root);
    char *joined = gm2_path_join(base, "gateway_state.json");
    gm2_path_free(base);
    json_t *a = gm2_args0();
    gm2_aset_s(a, "state_path", joined);
    free(joined);
    return gm2_scall(s, "read_runtime_status", a);
}

/* (runtime or {}) then .get(key): falsy -> empty dict; truthy non-dict ->
 * AttributeError 'get'. Returns a BORROWED node (NULL when absent/falsy). */
static json_t *gm2_runtime_get(const json_t *runtime, const char *key, json_t **err) {
    *err = NULL;
    if (!gm2_jtruth(runtime)) return NULL;
    if (!gm2_is_obj(runtime)) { *err = gm2_raise_attr(gm2_py_type_name(runtime), "get"); return NULL; }
    return json_obj_get(runtime, key);
}

/* ════════════════════════ ported functions ════════════════════════ */

/* PoP: gm2_flow_live_default_gateway_pid @ hermes_cli/gateway_multiplex_served.py:live_default_gateway_pid */
static json_t *gm2_flow_live_default_gateway_pid(const gm2_seams_t *s) {
    char *root = NULL;
    json_t *e = gm2_default_root_str(s, &root);
    GM2_PROP(e);
    json_t *a = gm2_args0();
    gm2_aset_s(a, "home", root);
    free(root);
    return gm2_scall(s, "live_gateway_pid_for_home", a);
}

/* PoP: gm2_flow_recorded_served_profiles @ hermes_cli/gateway_multiplex_served.py:recorded_served_profiles */
static json_t *gm2_flow_recorded_served_profiles(const char *default_root, const gm2_seams_t *s) {
    json_t *pid = gm2_flow_live_default_gateway_pid(s);
    GM2_PROP(pid);
    bool live = gm2_jpresent(pid);
    json_free(pid);
    if (!live) return NULL;
    /* (default_root or get_default_hermes_root()) / "gateway_state.json" */
    char *root = NULL;
    if (default_root && default_root[0]) {
        gm2_path_t *p = gm2_path_parse(default_root);
        root = gm2_path_render(p);
        gm2_path_free(p);
    } else {
        json_t *e = gm2_default_root_str(s, &root);
        if (gm2_is_raise(e)) { free(root); return e; }
    }
    json_t *runtime = gm2_read_state(s, root);
    free(root);
    GM2_PROP(runtime);
    json_t *err = NULL;
    json_t *served = gm2_runtime_get(runtime, "served_profiles", &err);
    if (err) { json_free(runtime); return err; }
    json_t *ret = gm2_is_arr(served) ? gm2_str_list(served) : NULL;
    json_free(runtime);
    return ret;
}

/* PoP: gm2_flow_multiplexer_served_secondaries @ hermes_cli/gateway_multiplex_served.py:multiplexer_served_secondaries */
static json_t *gm2_flow_multiplexer_served_secondaries(const gm2_seams_t *s) {
    json_t *rec = gm2_flow_recorded_served_profiles(NULL, s);
    GM2_PROP(rec);
    json_t *out = json_array();
    if (gm2_is_arr(rec)) {
        for (size_t i = 0; i < rec->c.count; i++) {
            const json_t *p = rec->c.items[i];
            const char *ps = gm2_jstr(p);
            if (ps && ps[0] && strcmp(ps, "default") != 0)
                json_append(out, json_string(ps));
        }
    }
    json_free(rec);
    return out;
}

/* PoP: gm2_flow_served_profile_unserved_platforms @ hermes_cli/gateway_multiplex_served.py:served_profile_unserved_platforms */
static json_t *gm2_flow_served_profile_unserved_platforms(const char *profile, const gm2_seams_t *s) {
    if (!profile || !profile[0]) return json_object();   /* not profile */
    json_t *pid = gm2_flow_live_default_gateway_pid(s);
    GM2_PROP(pid);
    bool live = gm2_jpresent(pid);
    json_free(pid);
    if (!live) return json_object();
    char *root = NULL;
    json_t *e = gm2_default_root_str(s, &root);
    if (gm2_is_raise(e)) { free(root); return e; }
    json_t *runtime = gm2_read_state(s, root);
    free(root);
    GM2_PROP(runtime);
    /* runtime or {} ; .get("platforms") ; isinstance dict */
    json_t *err = NULL;
    json_t *platforms = gm2_runtime_get(runtime, "platforms", &err);
    if (err) { json_free(runtime); return err; }
    if (!gm2_is_obj(platforms)) { json_free(runtime); return json_object(); }
    char *prefix = gm2_xsf("%s:", profile);
    size_t plen = strlen(prefix);
    json_t *out = json_object();
    for (size_t i = 0; i < platforms->c.count; i++) {
        const char *key = platforms->c.keys[i];
        const json_t *entry = platforms->c.items[i];
        if (strncmp(key, prefix, plen) != 0) continue;
        if (!gm2_is_obj(entry)) continue;
        const json_t *code = json_obj_get(entry, "error_code");
        if (gm2_is_obj(code) || gm2_is_arr(code)) {
            json_free(out); free(prefix); json_free(runtime);
            return gm2_raise_unhashable(code);
        }
        bool match = gm2_is_str(code)
            && (!strcmp(code->str_val, "multiplex_shared_ingress")
                || !strcmp(code->str_val, "whatsapp_unpaired"));
        if (!match) continue;
        const json_t *msg = json_obj_get(entry, "error_message");
        char *val = gm2_jtruth(msg) ? gm2_py_str(msg) : strdup("not served under multiplex");
        json_set(out, key + plen, json_string(val));
        free(val);
    }
    free(prefix);
    json_free(runtime);
    return out;
}

/* served = [str(p) for p in (runtime.get("served_profiles") or []) if p and p != "default"]
 * The `or []` iterable has NO isinstance guard: list -> elements, str -> ASCII
 * chars, dict -> keys, falsy -> [], other truthy scalar -> TypeError. */
static json_t *gm2_ingress_served(const json_t *runtime, json_t **err) {
    *err = NULL;
    const json_t *sp = gm2_is_obj(runtime) ? json_obj_get(runtime, "served_profiles") : NULL;
    json_t *out = json_array();
    if (!gm2_jtruth(sp)) return out;                 /* falsy -> [] */
    if (gm2_is_arr(sp)) {
        for (size_t i = 0; i < sp->c.count; i++) {
            const json_t *p = sp->c.items[i];
            if (!gm2_jtruth(p)) continue;
            char *s = gm2_py_str(p);
            if (strcmp(s, "default") != 0) json_append(out, json_string(s));
            free(s);
        }
        return out;
    }
    if (gm2_is_str(sp)) {
        const char *s = sp->str_val ? sp->str_val : "";
        for (size_t i = 0; s[i]; i++) {
            char ch[2] = { s[i], '\0' };
            if (strcmp(ch, "default") != 0) json_append(out, json_string(ch));
        }
        return out;
    }
    if (gm2_is_obj(sp)) {
        for (size_t i = 0; i < sp->c.count; i++) {
            const char *k = sp->c.keys[i];
            if (k[0] && strcmp(k, "default") != 0) json_append(out, json_string(k));
        }
        return out;
    }
    json_free(out);
    *err = gm2_raise_not_iterable(sp);               /* int / bool truthy */
    return NULL;
}

/* urls.setdefault(name, {})[platform] = str(url) */
static void gm2_urls_set(json_t *urls, const char *name, const char *platform, const json_t *url) {
    json_t *sub = json_obj_get(urls, name);
    if (!gm2_is_obj(sub)) { sub = json_object(); json_set(urls, name, sub); }
    char *s = gm2_py_str(url);
    json_set(sub, platform, json_string(s));
    free(s);
}

/* PoP: gm2_flow_served_profile_ingress_urls @ hermes_cli/gateway_multiplex_served.py:served_profile_ingress_urls */
static json_t *gm2_flow_served_profile_ingress_urls(const char *profile, const gm2_seams_t *s) {
    json_t *pid = gm2_flow_live_default_gateway_pid(s);
    GM2_PROP(pid);
    bool live = gm2_jpresent(pid);
    json_free(pid);
    if (!live) return json_object();
    char *root = NULL;
    json_t *e = gm2_default_root_str(s, &root);
    if (gm2_is_raise(e)) { free(root); return e; }
    json_t *runtime = gm2_read_state(s, root);
    free(root);
    GM2_PROP(runtime);
    json_t *err = NULL;
    json_t *platforms = gm2_runtime_get(runtime, "platforms", &err);
    if (err) { json_free(runtime); return err; }
    json_t *urls = json_object();
    if (gm2_is_obj(platforms)) {
        for (size_t i = 0; i < platforms->c.count; i++) {
            const char *key = platforms->c.keys[i];
            const json_t *entry = platforms->c.items[i];
            const char *colon = strchr(key, ':');
            if (!colon || !gm2_is_obj(entry)) continue;
            const json_t *url = json_obj_get(entry, "ingress_url");
            if (!gm2_jtruth(url)) continue;
            const json_t *state = json_obj_get(entry, "state");
            if (gm2_is_str(state)
                && (!strcmp(state->str_val, "fatal")
                    || !strcmp(state->str_val, "disconnected")
                    || !strcmp(state->str_val, "stopped"))) continue;
            size_t nlen = (size_t)(colon - key);
            char *name = malloc(nlen + 1);
            memcpy(name, key, nlen); name[nlen] = '\0';
            const char *platform = colon + 1;
            if (profile && profile[0] && strcmp(name, profile) != 0) { free(name); continue; }
            gm2_urls_set(urls, name, platform, url);
            free(name);
        }
    }
    /* served comprehension over runtime (the `or {}` result, not raw). */
    json_t *rt_eff = gm2_jtruth(runtime) ? runtime : NULL;
    json_t *served = gm2_ingress_served(rt_eff, &err);
    if (err) { json_free(urls); json_free(runtime); return err; }
    for (size_t i = 0; i < served->c.count; i++) {
        const char *name = served->c.items[i]->str_val;
        if (profile && profile[0] && strcmp(name, profile) != 0) continue;
        json_t *a = gm2_args0();
        gm2_aset_j(a, "runtime", rt_eff);
        gm2_aset_s(a, "name", name);
        json_t *mirror = gm2_scall(s, "shared_listener_mirror_platforms", a);
        if (gm2_is_raise(mirror)) { json_free(served); json_free(urls); json_free(runtime); return mirror; }
        if (!gm2_is_obj(mirror)) {
            json_t *ae = gm2_raise_attr(gm2_py_type_name(mirror), "items");
            json_free(mirror); json_free(served); json_free(urls); json_free(runtime);
            return ae;
        }
        for (size_t j = 0; j < mirror->c.count; j++) {
            const char *platform = mirror->c.keys[j];
            const json_t *entry = mirror->c.items[j];
            if (!gm2_is_obj(entry)) {
                json_t *ae = gm2_raise_attr(gm2_py_type_name(entry), "get");
                json_free(mirror); json_free(served); json_free(urls); json_free(runtime);
                return ae;
            }
            const json_t *iu = json_obj_get(entry, "ingress_url");
            if (gm2_jtruth(iu)) gm2_urls_set(urls, name, platform, iu);
        }
        json_free(mirror);
    }
    json_free(served);
    json_free(runtime);
    return urls;
}

/* PoP: gm2_flow_format_ingress_url_lines @ hermes_cli/gateway_multiplex_served.py:format_ingress_url_lines */
static json_t *gm2_flow_format_ingress_url_lines(const json_t *urls, const json_t *indent) {
    if (!gm2_is_obj(urls)) return gm2_raise_attr(gm2_py_type_name(urls), "items");
    char *ind = gm2_py_str(indent);   /* absent -> caller passes "  " default */
    /* sorted(urls.items()): unique str keys -> byte order (== codepoint order). */
    size_t n = urls->c.count;
    size_t *idx = malloc((n ? n : 1) * sizeof(size_t));
    for (size_t i = 0; i < n; i++) idx[i] = i;
    for (size_t i = 1; i < n; i++) {
        size_t cur = idx[i]; size_t j = i;
        while (j > 0 && strcmp(urls->c.keys[idx[j - 1]], urls->c.keys[cur]) > 0) {
            idx[j] = idx[j - 1]; j--;
        }
        idx[j] = cur;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < n; i++) {
        const char *platform = urls->c.keys[idx[i]];
        char *url = gm2_py_str(urls->c.items[idx[i]]);
        char *line = gm2_xsf("%s%s: %s", ind, platform, url);
        json_append(out, json_string(line));
        free(line); free(url);
    }
    free(idx); free(ind);
    return out;
}

/* PoP: gm2_flow_notify_multiplexer_profiles_changed @ hermes_cli/gateway_multiplex_served.py:notify_multiplexer_profiles_changed */
static json_t *gm2_flow_notify_multiplexer_profiles_changed(const char *profile_name,
                                                            const json_t *timeout,
                                                            const gm2_seams_t *s) {
    /* try: live pid check + rescan; except Exception -> debug log + None. */
    json_t *answer = NULL;
    json_t *caught = NULL;
    json_t *pid = gm2_flow_live_default_gateway_pid(s);
    if (gm2_is_raise(pid)) {
        if (gm2_is_exception(gm2_raise_class(pid))) caught = pid;
        else return pid;                       /* BaseException passes through */
    } else if (!gm2_jpresent(pid)) {
        json_free(pid);
        return NULL;                           /* no live gateway */
    } else {
        json_free(pid);
        char *root = NULL;
        json_t *e = gm2_default_root_str(s, &root);
        if (gm2_is_raise(e)) {
            if (gm2_is_exception(gm2_raise_class(e))) caught = e;
            else { free(root); return e; }
        } else {
            json_t *a = gm2_args0();
            gm2_aset_s(a, "root", root);
            gm2_aset_n(a, "timeout", timeout ? timeout->num_val : 8.0);
            answer = gm2_scall(s, "rescan_gateway_profiles", a);
            if (gm2_is_raise(answer)) {
                if (gm2_is_exception(gm2_raise_class(answer))) { caught = answer; answer = NULL; }
                else { free(root); return answer; }
            }
        }
        free(root);
    }
    if (caught) {
        json_free(caught);
        json_t *a = gm2_args0();
        gm2_aset_s(a, "msg", "multiplexer rescan notification failed for %r");
        json_t *args = json_array();
        json_append(args, json_string(profile_name ? profile_name : ""));
        json_set(a, "args", args);
        json_set(a, "exc_info", json_bool(true));
        json_t *lg = gm2_scall(s, "logger_debug", a);
        GM2_PROP(lg);                          /* a raise in the handler escapes */
        json_free(lg);
        return NULL;
    }
    /* if not isinstance(answer, dict) or answer.get("multiplex") is False */
    if (!gm2_is_obj(answer)) { json_free(answer); return NULL; }
    const json_t *mx = json_obj_get(answer, "multiplex");
    if (gm2_is_bool(mx) && !mx->bool_val) { json_free(answer); return NULL; }
    const json_t *served = json_obj_get(answer, "served_profiles");
    json_t *ret = gm2_is_arr(served) ? gm2_str_list(served) : NULL;
    json_free(answer);
    return ret;
}

/* PoP: gm2_flow_multiplexer_root_for @ hermes_cli/gateway_runtime_multiplex.py:multiplexer_root_for */
static json_t *gm2_flow_multiplexer_root_for(const char *home, const gm2_seams_t *s) {
    if (!home) return gm2_raise_path_none();   /* Path(None) */
    gm2_path_t *hp = gm2_path_parse(home);
    char *hrender = gm2_path_render(hp);
    json_t *a = gm2_args0();
    gm2_aset_s(a, "home", hrender);
    free(hrender);
    json_t *np = gm2_scall(s, "named_profile_home", a);
    GM2_PROP(np);
    bool named = gm2_jpresent(np);             /* is None -> only null */
    json_free(np);
    if (!named) { gm2_path_free(hp); return NULL; }
    gm2_path_t *p1 = gm2_path_parent(hp);
    gm2_path_t *root = gm2_path_parent(p1);
    gm2_path_free(p1);
    json_t *ret = NULL;
    if (!gm2_path_eq(root, hp)) {
        char *rs = gm2_path_render(root);
        ret = json_string(rs);
        free(rs);
    }
    gm2_path_free(root);
    gm2_path_free(hp);
    return ret;
}

/* PoP: gm2_flow_implied_host_root @ hermes_cli/gateway_runtime_multiplex.py:implied_host_root */
static json_t *gm2_flow_implied_host_root(const char *home, const gm2_seams_t *s) {
    json_t *root = gm2_flow_multiplexer_root_for(home, s);
    GM2_PROP(root);
    if (!gm2_jpresent(root)) { json_free(root); return NULL; }
    gm2_path_t *hp = gm2_path_parse(home);
    char *hrender = gm2_path_render(hp);
    gm2_path_free(hp);
    json_t *a = gm2_args0();
    gm2_aset_s(a, "default_home", root->str_val);
    gm2_aset_s(a, "home", hrender);
    free(hrender);
    json_t *verdict = gm2_scall(s, "host_serves_named_profile", a);
    GM2_PROP(verdict);
    /* None if verdict is False else root — `is False` identity (bool only). */
    bool is_false = gm2_is_bool(verdict) && !verdict->bool_val;
    json_free(verdict);
    if (is_false) { json_free(root); return NULL; }
    return root;
}

/* PoP: gm2_flow_multiplexer_serves_home @ hermes_cli/gateway_runtime_multiplex.py:multiplexer_serves_home */
static json_t *gm2_flow_multiplexer_serves_home(const char *home, const gm2_seams_t *s) {
    json_t *root = gm2_flow_multiplexer_root_for(home, s);
    GM2_PROP(root);
    if (!gm2_jpresent(root)) { json_free(root); return NULL; }
    gm2_path_t *hp = gm2_path_parse(home);
    char *hrender = gm2_path_render(hp);
    json_t *a = gm2_args0();
    gm2_aset_s(a, "default_home", root->str_val);
    gm2_aset_s(a, "home", hrender);
    json_t *verdict = gm2_scall(s, "host_serves_named_profile", a);
    if (gm2_is_raise(verdict)) { free(hrender); gm2_path_free(hp); json_free(root); return verdict; }
    if (gm2_jpresent(verdict)) {
        bool truth = gm2_jtruth(verdict);
        json_free(verdict); free(hrender); gm2_path_free(hp);
        if (truth) return root;
        json_free(root); return NULL;
    }
    json_free(verdict);
    /* verdict None: fall back to the recorded served set. */
    json_t *runtime = gm2_read_state(s, root->str_val);
    if (gm2_is_raise(runtime)) { free(hrender); gm2_path_free(hp); json_free(root); return runtime; }
    json_t *err = NULL;
    json_t *served = gm2_runtime_get(runtime, "served_profiles", &err);
    if (err) { json_free(runtime); free(hrender); gm2_path_free(hp); json_free(root); return err; }
    if (!gm2_is_arr(served)) { json_free(runtime); free(hrender); gm2_path_free(hp); json_free(root); return NULL; }
    /* names = {normalize_profile_name(str(p)) for p in served if p} */
    json_t *names = json_array();   /* set modelled as an ordered membership list */
    for (size_t i = 0; i < served->c.count; i++) {
        const json_t *p = served->c.items[i];
        if (!gm2_jtruth(p)) continue;
        char *ps = gm2_py_str(p);
        json_t *na = gm2_args0();
        gm2_aset_s(na, "name", ps);
        free(ps);
        json_t *nm = gm2_scall(s, "normalize_profile_name", na);
        if (gm2_is_raise(nm)) { json_free(names); json_free(runtime); free(hrender); gm2_path_free(hp); json_free(root); return nm; }
        if (gm2_is_obj(nm) || gm2_is_arr(nm)) {
            json_t *ue = gm2_raise_unhashable(nm);
            json_free(nm); json_free(names); json_free(runtime); free(hrender); gm2_path_free(hp); json_free(root);
            return ue;
        }
        json_append(names, nm);
    }
    /* normalize_profile_name(home.name) in names */
    char *hname = gm2_path_name(hp);
    json_t *fa = gm2_args0();
    gm2_aset_s(fa, "name", hname);
    free(hname);
    json_t *fn = gm2_scall(s, "normalize_profile_name", fa);
    if (gm2_is_raise(fn)) { json_free(names); json_free(runtime); free(hrender); gm2_path_free(hp); json_free(root); return fn; }
    if (gm2_is_obj(fn) || gm2_is_arr(fn)) {
        json_t *ue = gm2_raise_unhashable(fn);
        json_free(fn); json_free(names); json_free(runtime); free(hrender); gm2_path_free(hp); json_free(root);
        return ue;
    }
    bool member = false;
    for (size_t i = 0; i < names->c.count; i++)
        if (gm2_py_eq(fn, names->c.items[i])) { member = true; break; }
    json_free(fn); json_free(names); json_free(runtime); free(hrender); gm2_path_free(hp);
    if (member) return root;
    json_free(root);
    return NULL;
}