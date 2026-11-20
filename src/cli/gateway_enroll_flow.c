/*
 * gateway_enroll_flow.c — hermes_cli/gateway_enroll.py FLOW + the UNCLAIMED
 * hermes_cli/gateway_multiplex_mode.py leaf cluster (LIVE authority:
 * C:/Users/eman5/hermes-agent/hermes_cli/gateway_enroll.py and
 * C:/Users/eman5/hermes-agent/hermes_cli/gateway_multiplex_mode.py).
 *
 * gateway_enroll.py's six top-level defs are ALL PoP-claimed already
 * (src/hermes_cli/gateway_enroll.c + src/hermes_cli/main_wrappers.c), so the
 * enroll flow's claimed siblings are SEAMED, not re-ported. The unclaimed
 * surface is `_fail` plus the entire gateway_multiplex_mode.py module (20
 * defs) — the multiplex verdict cluster that `_warn_if_secondary_multiplex_profile`
 * (claimed) consults via default_gateway_multiplexes(), and the boot-time
 * decision flow around it. 21 functions ported here.
 *
 * SEAM DOCTRINE (universal seam, same shape as gateway_migrate_flow2):
 * every live effect — env/config reads, foreign-module calls, print,
 * sys.exit, logger verbs, Path marker ops, the claimed enroll siblings'
 * collaborators — enters through ONE seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` borrowed JSON object
 * (null-valued keys dropped on BOTH sides); result NEW json_t* (NULL == None,
 * never a JSON_NULL node); {"__raise__": cls, "msg": msg} == a raised
 * exception. The harness records {"ev","args","ret"} per call in order; the
 * oracle's recording stubs emit the identical shape driving LIVE Python.
 *
 * RETURN CONTRACT: every ge2_flow_* returns a NEW json_t* (caller frees):
 * NULL == Python None; a {"__raise__","msg"} object == a raised exception,
 * propagated or caught exactly as the live def does. MultiplexDecision is
 * projected to {"enabled","source","reason"} (dataclass field order).
 * BaseException (SystemExit / KeyboardInterrupt / GeneratorExit) passes
 * through every except Exception site. Scripted raise specs propagate
 * VERBATIM; str(KeyError(k)) is repr(k) at catch-and-stringify sites.
 *
 * INTERNAL COMPOSITION runs real on both sides: ported targets call each
 * other directly (C) / via the shared extracted namespace (oracle); only
 * external effects are seamed.
 *
 * STATE: record/publish/log_multiplex_decision mutate the module-global
 * _pending_decision list — modelled as a caller-owned state object
 * {"pending":[decision,...]}. standalone_launcher_decision /
 * resolve_multiplex_mode mutate config.multiplex_profiles in place — the
 * caller-owned config object is echoed as state.
 *
 * Fixture domain (kept identical on both sides, documented exclusions):
 * - paths are non-empty absolute POSIX strings (PathDouble normalizes
 *   backslashes/double-slashes/trailing slashes; "."/"" bases out of domain);
 * - config doubles are dict|null; decision dicts carry enabled+source and
 *   reason defaults to ""; plan dicts carry standalone_secondaries (list of
 *   {name,pid}), blocked, blockers — missing keys raise AttributeError on
 *   the SimpleNamespace double exactly as live;
 * - pids are int literals or null (integral floats read as ints);
 * - profiles_to_serve returns [[name,home],...] or null;
 * - _box bodies are lists of str (the fixed notice constants carry the
 *   non-ASCII warning sign U+26A0 and box-drawing chars; char lengths are
 *   counted as codepoints, not bytes);
 * - logger args project exception instances to str(exc); repr(in_file) is
 *   modelled for null/bool/int/simple-ASCII str only.
 *
 * Seam name table (name -> live call; args key order is part of the trace):
 *   print                     print(line)                       {line}
 *   sys_exit                  sys.exit(code)                    {code}
 *   env_multiplex_override    gateway.config._env_multiplex_profiles_override() {}
 *   config_exists             hermes_cli.config_backend.config_exists(path) {path}
 *   read_user_config_raw      hermes_cli.config.read_user_config_raw(path) {path}
 *   bool_token                gateway.config._bool_token(value) {value}
 *   profile_is_standalone     hermes_cli.profiles.profile_is_standalone(home) {home}
 *   get_default_hermes_root   hermes_constants.get_default_hermes_root() {}
 *   recorded_served_profiles  gateway_multiplex_served.recorded_served_profiles(root) {default_home}
 *   get_hermes_home           hermes_constants.get_hermes_home() {}
 *   profile_name_for_home     hermes_constants.profile_name_for_home(home) {home}
 *   profiles_to_serve         profiles.profiles_to_serve(multiplex=True, include_parked=True) {multiplex, include_parked}
 *   host_supports_migration   gateway_migrate._host_supports_migration() {}
 *   build_migration_plan      gateway_migrate.build_migration_plan() {}
 *   pg_service_label          pg.service_label()                {name}
 *   write_multiplex_flag      gateway_migrate._write_multiplex_flag(home, value) {default_home, value}
 *   marker_write_text         (home/MARKER).write_text(text, encoding="utf-8") {path, text, encoding}
 *   marker_exists             (home/MARKER).exists()            {path}
 *   marker_unlink             (home/MARKER).unlink()            {path}
 *   owns_gateway_runtime_lock gateway.status.owns_gateway_runtime_lock() {}
 *   write_runtime_status      gateway.status.write_runtime_status(multiplex_standalone_reason=...) {multiplex_standalone_reason?}
 *   read_runtime_status       gateway.status.read_runtime_status(state_path) {state_path?}
 *   runtime_status_pid_is_live gateway.status.runtime_status_pid_is_live(record) {record}
 *   logger_debug              logger.debug(msg, exc_info=True)  {msg, exc_info}
 *   logger_info               logger.info(msg, *args)           {msg, args}
 *   logger_warning            logger.warning(msg, *args[, exc_info=True]) {msg, args, exc_info?}
 * MIGRATE_COMMAND is a fixed constant ("hermes gateway migrate --multiplex")
 * imported from hermes_cli.gateway_migrate at call time — bound verbatim on
 * both sides, not a seam.
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

#define GE2_SINGLE_PROFILE_REASON "only one profile exists (nothing to multiplex)"
#define GE2_STANDALONE_PROFILE_REASON "this profile is standalone (gateway.standalone: true); it serves only itself"
#define GE2_RETIRED_OPT_OUT_REASON "gateway.multiplex_profiles: false is retired and was rewritten to true; one gateway per host serves every profile. A per-profile gateway is `gateway.standalone: true` in that profile's config (temporary shim) or `--force`."
#define GE2_REWRITTEN_MARKER_NAME ".multiplex_opt_out_rewritten"
#define GE2_MIGRATE_COMMAND "hermes gateway migrate --multiplex"
#define GE2_DEFAULT_APPLIES_REASON "gateway.multiplex_profiles unset; default applies"

/* UTF-8 literals for the fixed live constants (box drawing + warning sign). */
#define GE2_BOX_TL "\xe2\x94\x8c"
#define GE2_BOX_H  "\xe2\x94\x80"
#define GE2_BOX_TR "\xe2\x94\x90"
#define GE2_BOX_V  "\xe2\x94\x82"
#define GE2_BOX_BL "\xe2\x94\x94"
#define GE2_BOX_BR "\xe2\x94\x98"
#define GE2_WARN   "\xe2\x9a\xa0"

/* ── JSON / Python-value helpers ── */

static bool ge2_jtruth(const json_t *v) {
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
static bool ge2_jpresent(const json_t *v) { return v && v->type != JSON_NULL; }
static bool ge2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool ge2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool ge2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool ge2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool ge2_is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static const char *ge2_jstr(const json_t *v) {
    return (ge2_is_str(v) && v->str_val) ? v->str_val : NULL;
}

static char *ge2_xsf(const char *fmt, ...) {
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

static const char *ge2_py_type_name(const json_t *v) {
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
static bool ge2_py_eq(const json_t *a, const json_t *b) {
    bool ap = ge2_jpresent(a), bp = ge2_jpresent(b);
    if (!ap || !bp) return ap == bp;
    if (ge2_is_bool(a) && ge2_is_bool(b)) return a->bool_val == b->bool_val;
    if ((ge2_is_bool(a) || ge2_is_num(a)) && (ge2_is_bool(b) || ge2_is_num(b))) {
        double av = ge2_is_bool(a) ? (a->bool_val ? 1.0 : 0.0) : a->num_val;
        double bv = ge2_is_bool(b) ? (b->bool_val ? 1.0 : 0.0) : b->num_val;
        return av == bv;
    }
    if (ge2_is_str(a) && ge2_is_str(b)) return strcmp(a->str_val, b->str_val) == 0;
    if (ge2_is_arr(a) && ge2_is_arr(b)) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++)
            if (!ge2_py_eq(a->c.items[i], b->c.items[i])) return false;
        return true;
    }
    if (ge2_is_obj(a) && ge2_is_obj(b)) {
        if (a->c.count != b->c.count) return false;
        for (size_t i = 0; i < a->c.count; i++) {
            const json_t *bv = json_obj_get(b, a->c.keys[i]);
            if (!bv || !ge2_py_eq(a->c.items[i], bv)) return false;
        }
        return true;
    }
    return false;
}

static char *ge2_py_float_str(double d) {
    if (d == (double)(long long)d) return ge2_xsf("%.0f", d);
    char buf[64];
    for (int prec = 15; prec <= 17; prec++) {
        snprintf(buf, sizeof(buf), "%.*g", prec, d);
        if (strtod(buf, NULL) == d) break;
    }
    return strdup(buf);
}

static char *ge2_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_NULL:   return strdup("None");
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: return ge2_py_float_str(v->num_val);
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_ARRAY: {
            char *out = strdup("[");
            for (size_t i = 0; i < v->c.count; i++) {
                char *item = ge2_py_str(v->c.items[i]);
                char *next = ge2_xsf("%s%s%s", out, i ? ", " : "", item);
                free(out); free(item); out = next;
            }
            char *next = ge2_xsf("%s]", out); free(out); return next;
        }
        case JSON_OBJECT: {
            char *out = strdup("{");
            for (size_t i = 0; i < v->c.count; i++) {
                char *item = ge2_py_str(v->c.items[i]);
                char *next = ge2_xsf("%s%s'%s': %s", out, i ? ", " : "", v->c.keys[i], item);
                free(out); free(item); out = next;
            }
            char *next = ge2_xsf("%s}", out); free(out); return next;
        }
    }
    return strdup("");
}

/* repr() for the fixture domain: null/bool/int/simple-ASCII str. A string
 * holding a single quote but no double quote is rendered with double quotes
 * (CPython delimiter choice); otherwise single quotes with escapes. */
static char *ge2_py_repr(const json_t *v) {
    if (!ge2_is_str(v)) return ge2_py_str(v);
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
        char c = s[i];
        if (c == '\\' || c == q) { out[k++] = '\\'; out[k++] = c; }
        else if (c == '\n') { out[k++] = '\\'; out[k++] = 'n'; }
        else if (c == '\r') { out[k++] = '\\'; out[k++] = 'r'; }
        else if (c == '\t') { out[k++] = '\\'; out[k++] = 't'; }
        else out[k++] = c;
    }
    out[k++] = q;
    out[k] = '\0';
    return out;
}

/* str(exc) for a raise spec: KeyError stringifies as repr(key). */
static char *ge2_raise_str(const json_t *spec) {
    const char *cls = NULL, *msg = "";
    if (ge2_is_obj(spec)) {
        const json_t *c = json_obj_get(spec, "__raise__");
        const json_t *m = json_obj_get(spec, "msg");
        if (ge2_is_str(c)) cls = c->str_val;
        if (ge2_is_str(m)) msg = m->str_val;
    }
    if (cls && !strcmp(cls, "KeyError")) {
        json_t *tmp = json_string(msg);
        char *r = ge2_py_repr(tmp);
        json_free(tmp);
        return r;
    }
    return strdup(msg);
}

/* Codepoint count (Python len(str)); the fixture corpus is ASCII except the
 * fixed U+26A0 / box-drawing constants, all valid UTF-8. */
static size_t ge2_utf8_len(const char *s) {
    size_t n = 0;
    if (!s) return 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}

/* ── path helpers (Path.as_posix semantics over the absolute-POSIX domain) ── */

static char *ge2_path_norm(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s);
    char *out = malloc(n + 2);
    if (!out) return strdup("");
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        char ch = (s[i] == '\\') ? '/' : s[i];
        if (ch == '/' && k > 0 && out[k - 1] == '/') continue;
        out[k++] = ch;
    }
    out[k] = '\0';
    while (k > 1 && out[k - 1] == '/') out[--k] = '\0';
    return out;
}
static char *ge2_path_join(const char *base, const char *name) {
    char *b = ge2_path_norm(base);
    size_t bl = strlen(b);
    char *out;
    if (bl == 0) out = strdup(name);              /* Path("") / x == Path(x) */
    else out = ge2_xsf("%s%s%s", b, (b[bl - 1] == '/') ? "" : "/", name);
    free(b);
    return out;
}

/* ── trace / seam / raise plumbing ── */

typedef struct ge2_trace { json_t *events; } ge2_trace_t;
typedef struct ge2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} ge2_seams_t;

static void ge2_trace_add(ge2_trace_t *t, json_t *ev) {
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static const char *ge2_raise_class(const json_t *r) {
    if (ge2_is_obj(r)) {
        const json_t *c = json_obj_get(r, "__raise__");
        if (ge2_is_str(c)) return c->str_val;
    }
    return NULL;
}
static bool ge2_is_raise(const json_t *r) { return ge2_raise_class(r) != NULL; }

/* Seam call that CONSUMES (frees) the freshly built args object. */
static json_t *ge2_scall(const ge2_seams_t *s, const char *name, json_t *args) {
    json_t *r = s->call(s->ctx, name, args);
    json_free(args);
    return r;
}

static json_t *ge2_raise_obj(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
static json_t *ge2_raise_attr(const char *type_name, const char *attr) {
    char *m = ge2_xsf("'%s' object has no attribute '%s'", type_name, attr);
    json_t *r = ge2_raise_obj("AttributeError", m);
    free(m);
    return r;
}
/* Attribute error on a materialized record double: live carriers are
 * SimpleNamespace, whose py3.14 attribute errors are module-qualified. */
static json_t *ge2_raise_attr_carrier(const json_t *v, const char *attr) {
    const char *tn = ge2_is_obj(v) ? "types.SimpleNamespace" : ge2_py_type_name(v);
    return ge2_raise_attr(tn, attr);
}
#define GE2_PROP(r) do { if (ge2_is_raise(r)) return (r); } while (0)

/* ── FULL builtin exception hierarchy (child -> parent) ── */

typedef struct { const char *child, *parent; } ge2_exc_rel_t;
static const ge2_exc_rel_t GE2_EXC_PARENTS[] = {
    {"Exception", "BaseException"},
    {"SystemExit", "BaseException"},
    {"KeyboardInterrupt", "BaseException"},
    {"GeneratorExit", "BaseException"},
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
    /* JSONDecodeError is NOT a builtin: the oracle's _exc_class fallback
     * synthesizes non-builtin kinds as custom Exception subclasses, so it
     * must not appear in this builtin table (unknown -> custom Exception). */
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
static const char *ge2_exc_parent(const char *kind) {
    for (size_t i = 0; i < sizeof(GE2_EXC_PARENTS) / sizeof(GE2_EXC_PARENTS[0]); i++)
        if (!strcmp(kind, GE2_EXC_PARENTS[i].child)) return GE2_EXC_PARENTS[i].parent;
    return NULL;
}
/* except Exception: true for every Exception descendant. The BaseException
 * trio (and BaseException itself) pass through. An unknown kind is a custom
 * Exception subclass (the oracle's _exc_class fallback). */
static bool ge2_is_exception(const char *kind) {
    if (!kind) return false;
    if (!strcmp(kind, "BaseException") || !strcmp(kind, "SystemExit")
        || !strcmp(kind, "KeyboardInterrupt") || !strcmp(kind, "GeneratorExit"))
        return false;
    const char *k = kind;
    for (int depth = 0; depth < 32; depth++) {
        if (!strcmp(k, "Exception")) return true;
        if (!strcmp(k, "BaseException")) return false;
        const char *p = ge2_exc_parent(k);
        if (!p) return true;  /* unknown -> custom Exception subclass */
        k = p;
    }
    return true;
}
/* except (OSError, ValueError, TypeError, AttributeError, OverflowError):
 * recorded_standalone_reason's handler tuple. */
static bool ge2_exc_in_record_tuple(const char *kind) {
    static const char *tuple[] = {
        "OSError", "ValueError", "TypeError", "AttributeError", "OverflowError",
    };
    if (!kind) return false;
    const char *k = kind;
    for (int depth = 0; depth < 32; depth++) {
        for (size_t i = 0; i < sizeof(tuple) / sizeof(tuple[0]); i++)
            if (!strcmp(k, tuple[i])) return true;
        if (!strcmp(k, "BaseException")) return false;
        const char *p = ge2_exc_parent(k);
        if (!p) return false;  /* unknown custom class: not in the tuple */
        k = p;
    }
    return false;
}

/* ── args builders (null-valued keys dropped on BOTH sides) ── */

static json_t *ge2_args0(void) { return json_object(); }
static void ge2_aset_j(json_t *a, const char *k, const json_t *v) {
    if (ge2_jpresent(v)) json_set(a, k, json_copy(v));
}
static void ge2_aset_s(json_t *a, const char *k, const char *v) {
    if (v) json_set(a, k, json_string(v));
}
static void ge2_aset_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void ge2_aset_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }

/* ── MultiplexDecision projection (dataclass field order) ── */

static json_t *ge2_decision(bool enabled, const char *source, const char *reason) {
    json_t *d = json_object();
    json_set(d, "enabled", json_bool(enabled));
    json_set(d, "source", json_string(source));
    json_set(d, "reason", json_string(reason ? reason : ""));
    return d;
}
static const char *ge2_dec_source(const json_t *d) {
    const char *s = ge2_jstr(json_obj_get(d, "source"));
    return s ? s : "";
}
/* decision.reason with the dataclass default "" when absent. */
static const json_t *ge2_dec_reason_node(const json_t *d) {
    const json_t *r = json_obj_get(d, "reason");
    return r ? r : NULL;
}

/* ── string helpers ── */

/* ", ".join with the live TypeError on a non-str item. Returns NULL and
 * parks the raise spec in *err on failure. */
static char *ge2_join_strs_checked(const json_t *arr, const char *sep, json_t **err) {
    *err = NULL;
    if (!ge2_is_arr(arr)) {
        *err = ge2_raise_obj("TypeError", "can only join an iterable");
        return NULL;
    }
    char *out = strdup("");
    for (size_t i = 0; i < arr->c.count; i++) {
        const json_t *e = arr->c.items[i];
        if (!ge2_is_str(e)) {
            char *m = ge2_xsf("sequence item %zu: expected str instance, %s found",
                              i, ge2_py_type_name(e));
            *err = ge2_raise_obj("TypeError", m);
            free(m); free(out);
            return NULL;
        }
        char *next = ge2_xsf("%s%s%s", out, i ? sep : "", e->str_val);
        free(out); out = next;
    }
    return out;
}

/* ════════════════════════ ported functions ════════════════════════ */

json_t *ge2_flow_box(const json_t *body);  /* defined below; notice lines use it */
json_t *ge2_flow_standalone_warning_lines(const json_t *decision, const json_t *unserved,
                                          const ge2_seams_t *s);  /* log uses it */

/* PoP: ge2_flow_fail @ hermes_cli/gateway_enroll.py:_fail */
json_t *ge2_flow_fail(const json_t *lines, const ge2_seams_t *s) {
    if (!ge2_is_arr(lines)) {
        /* for line in lines: None / non-iterable raises before any print. */
        char *m = ge2_xsf("'%s' object is not iterable", ge2_py_type_name(lines));
        json_t *e = ge2_raise_obj("TypeError", m);
        free(m);
        return e;
    }
    for (size_t i = 0; i < lines->c.count; i++) {
        json_t *a = ge2_args0();
        ge2_aset_j(a, "line", lines->c.items[i]);
        json_t *r = ge2_scall(s, "print", a);
        GE2_PROP(r);
        json_free(r);
    }
    json_t *a = ge2_args0();
    ge2_aset_n(a, "code", 1);
    json_t *r = ge2_scall(s, "sys_exit", a);
    if (ge2_is_raise(r)) return r;   /* scripted override propagates */
    json_free(r);
    /* sys.exit(1) raises SystemExit(1); str(SystemExit(1)) == "1". */
    return ge2_raise_obj("SystemExit", "1");
}

/* PoP: ge2_flow_explicit_multiplex_flag @ hermes_cli/gateway_multiplex_mode.py:explicit_multiplex_flag */
json_t *ge2_flow_explicit_multiplex_flag(const char *default_home, const ge2_seams_t *s) {
    json_t *env = ge2_scall(s, "env_multiplex_override", ge2_args0());
    GE2_PROP(env);
    if (ge2_jpresent(env)) return env;   /* env is not None (False returns False) */
    json_free(env);
    if (!default_home) {
        /* Path(None) — probed CPython 3.14.9 message. */
        return ge2_raise_obj("TypeError",
            "argument should be a str or an os.PathLike object where __fspath__ returns a str, not 'NoneType'");
    }
    char *cfg_path = ge2_path_join(default_home, "config.yaml");
    json_t *a = ge2_args0();
    ge2_aset_s(a, "path", cfg_path);
    json_t *ex = ge2_scall(s, "config_exists", a);
    if (ge2_is_raise(ex)) { free(cfg_path); return ex; }
    bool exists = ge2_jtruth(ex);
    json_free(ex);
    if (!exists) { free(cfg_path); return NULL; }
    a = ge2_args0();
    ge2_aset_s(a, "path", cfg_path);
    json_t *cfg = ge2_scall(s, "read_user_config_raw", a);
    free(cfg_path);
    if (ge2_is_raise(cfg)) return cfg;
    if (!ge2_jtruth(cfg)) { json_free(cfg); cfg = json_object(); }
    else if (!ge2_is_obj(cfg)) {
        json_t *e = ge2_raise_attr(ge2_py_type_name(cfg), "get");
        json_free(cfg);
        return e;
    }
    /* gateway_section = cfg.get("gateway") if isinstance(..., dict) else {} */
    json_t *gw = json_obj_get(cfg, "gateway");
    json_t *section = ge2_is_obj(gw) ? gw : NULL;
    json_t *v = json_obj_get(cfg, "multiplex_profiles");
    if (!ge2_jpresent(v) && section) v = json_obj_get(section, "multiplex_profiles");
    json_t *value = ge2_jpresent(v) ? json_copy(v) : NULL;
    json_free(cfg);
    if (!value) return NULL;
    if (ge2_is_str(value)) {
        a = ge2_args0();
        ge2_aset_s(a, "value", value->str_val);
        json_t *parsed = ge2_scall(s, "bool_token", a);
        json_free(value);
        GE2_PROP(parsed);
        if (!ge2_jpresent(parsed)) { json_free(parsed); return json_bool(true); }
        return parsed;   /* True if parsed is None else parsed */
    }
    json_t *ret = json_bool(ge2_jtruth(value));
    json_free(value);
    return ret;
}

/* PoP: ge2_flow_settled_explicit_flag @ hermes_cli/gateway_multiplex_mode.py:settled_explicit_flag */
json_t *ge2_flow_settled_explicit_flag(const json_t *value) {
    /* return True if value else None */
    return ge2_jtruth(value) ? json_bool(true) : NULL;
}

/* PoP: ge2_flow_host_serves_named_profile @ hermes_cli/gateway_multiplex_mode.py:host_serves_named_profile */
json_t *ge2_flow_host_serves_named_profile(const char *default_home, const char *home,
                                           const ge2_seams_t *s) {
    json_t *a = ge2_args0();
    ge2_aset_s(a, "home", home);
    json_t *r = ge2_scall(s, "profile_is_standalone", a);
    GE2_PROP(r);
    bool standalone = ge2_jtruth(r);
    json_free(r);
    if (standalone) return json_bool(false);
    json_t *flag = ge2_flow_explicit_multiplex_flag(default_home, s);
    GE2_PROP(flag);
    json_t *ret = ge2_flow_settled_explicit_flag(flag);
    json_free(flag);
    return ret;
}

/* PoP: ge2_flow_default_gateway_multiplexes @ hermes_cli/gateway_multiplex_mode.py:default_gateway_multiplexes */
json_t *ge2_flow_default_gateway_multiplexes(const char *default_home, const ge2_seams_t *s) {
    char *root = NULL;
    if (default_home) {
        root = ge2_path_norm(default_home);   /* Path(default_home) */
    } else {
        json_t *r = ge2_scall(s, "get_default_hermes_root", ge2_args0());
        GE2_PROP(r);
        if (ge2_jpresent(r)) {
            /* live root is a Path — normalize the scripted str. */
            char *n = ge2_path_norm(ge2_jstr(r) ? ge2_jstr(r) : "");
            root = n;
        }
        json_free(r);
    }
    json_t *a = ge2_args0();
    ge2_aset_s(a, "default_home", root);
    json_t *rec = ge2_scall(s, "recorded_served_profiles", a);
    if (ge2_is_raise(rec)) { free(root); return rec; }
    if (ge2_jpresent(rec)) {
        bool b = ge2_jtruth(rec);   /* bool(recorded): [] -> False */
        json_free(rec); free(root);
        return json_bool(b);
    }
    json_free(rec);
    json_t *flag = ge2_flow_explicit_multiplex_flag(root, s);
    if (ge2_is_raise(flag)) { free(root); return flag; }
    bool ret = ge2_jpresent(flag);   /* False if flag is None else True */
    json_free(flag); free(root);
    return json_bool(ret);
}

/* PoP: ge2_flow_standalone_launcher @ hermes_cli/gateway_multiplex_mode.py:_standalone_launcher */
json_t *ge2_flow_standalone_launcher(const ge2_seams_t *s) {
    json_t *home = ge2_scall(s, "get_hermes_home", ge2_args0());
    GE2_PROP(home);
    char *hs = ge2_jstr(home) ? strdup(ge2_jstr(home)) : NULL;
    json_free(home);
    json_t *a = ge2_args0();
    ge2_aset_s(a, "home", hs);
    json_t *nm = ge2_scall(s, "profile_name_for_home", a);
    if (ge2_is_raise(nm)) { free(hs); return nm; }
    /* profile_name_for_home(home) not in (None, "default") */
    bool named = ge2_jpresent(nm)
        && !(ge2_is_str(nm) && !strcmp(nm->str_val, "default"));
    json_free(nm);
    if (!named) { free(hs); return json_bool(false); }
    a = ge2_args0();
    ge2_aset_s(a, "home", hs);
    free(hs);
    /* `X and Y` returns Y verbatim (a scripted raise propagates). */
    return ge2_scall(s, "profile_is_standalone", a);
}

/* PoP: ge2_flow_standalone_launcher_decision @ hermes_cli/gateway_multiplex_mode.py:standalone_launcher_decision */
json_t *ge2_flow_standalone_launcher_decision(json_t *config, const ge2_seams_t *s) {
    json_t *sl = ge2_flow_standalone_launcher(s);
    GE2_PROP(sl);
    bool fire = ge2_jtruth(sl);
    json_free(sl);
    if (!fire) return NULL;
    if (!config) {
        /* py3.14 setattr-on-None message (probed). */
        return ge2_raise_obj("AttributeError",
            "'NoneType' object has no attribute 'multiplex_profiles' and no __dict__ for setting new attributes");
    }
    json_set(config, "multiplex_profiles", json_bool(false));
    return ge2_decision(false, "guard", GE2_STANDALONE_PROFILE_REASON);
}

/* PoP: ge2_flow_implicit_multiplex_blocker @ hermes_cli/gateway_multiplex_mode.py:implicit_multiplex_blocker */
json_t *ge2_flow_implicit_multiplex_blocker(const ge2_seams_t *s) {
    json_t *sl = ge2_flow_standalone_launcher(s);
    GE2_PROP(sl);
    bool fire = ge2_jtruth(sl);
    json_free(sl);
    if (fire) return json_string(GE2_STANDALONE_PROFILE_REASON);

    json_t *a = ge2_args0();
    ge2_aset_b(a, "multiplex", true);
    ge2_aset_b(a, "include_parked", true);
    json_t *ps = ge2_scall(s, "profiles_to_serve", a);
    GE2_PROP(ps);
    size_t n = 0;
    if (!ge2_jpresent(ps)) {
        json_free(ps);
        return ge2_raise_obj("TypeError", "object of type 'NoneType' has no len()");
    }
    if (ge2_is_arr(ps) || ge2_is_obj(ps)) n = ps->c.count;
    else if (ge2_is_str(ps)) n = ge2_utf8_len(ps->str_val);
    else {
        char *m = ge2_xsf("object of type '%s' has no len()", ge2_py_type_name(ps));
        json_t *e = ge2_raise_obj("TypeError", m);
        free(m); json_free(ps);
        return e;
    }
    json_free(ps);
    if (n < 2) return json_string(GE2_SINGLE_PROFILE_REASON);

    json_t *hr = ge2_scall(s, "host_supports_migration", ge2_args0());
    GE2_PROP(hr);
    if (ge2_jtruth(hr)) return hr;   /* return host_reason verbatim */
    json_free(hr);

    json_t *plan = ge2_scall(s, "build_migration_plan", ge2_args0());
    GE2_PROP(plan);
    if (!ge2_jpresent(plan)) {
        json_free(plan);
        return ge2_raise_attr("NoneType", "standalone_secondaries");
    }
    if (!ge2_is_obj(plan)) {
        json_t *e = ge2_raise_attr(ge2_py_type_name(plan), "standalone_secondaries");
        json_free(plan);
        return e;
    }
    json_t *ss = json_obj_get(plan, "standalone_secondaries");
    if (!ss) {
        json_t *e = ge2_raise_attr_carrier(plan, "standalone_secondaries");
        json_free(plan);
        return e;
    }
    if (ge2_jtruth(ss)) {
        if (!ge2_is_arr(ss)) {
            char *m = ge2_xsf("'%s' object is not iterable", ge2_py_type_name(ss));
            json_t *e = ge2_raise_obj("TypeError", m);
            free(m); json_free(plan);
            return e;
        }
        char *owned = strdup("");
        for (size_t i = 0; i < ss->c.count; i++) {
            const json_t *pg = ss->c.items[i];
            /* f"'{p.name}' (...)" reads p.name FIRST. */
            const json_t *nm = ge2_is_obj(pg) ? json_obj_get(pg, "name") : NULL;
            if (!ge2_is_obj(pg) || !nm) {
                json_t *e = ge2_raise_attr_carrier(pg, "name");
                free(owned); json_free(plan);
                return e;
            }
            char *name_s = ge2_py_str(nm);
            const json_t *pid = json_obj_get(pg, "pid");
            if (!pid) {
                json_t *e = ge2_raise_attr_carrier(pg, "pid");
                free(name_s); free(owned); json_free(plan);
                return e;
            }
            char *inner = NULL;
            if (ge2_jtruth(pid)) {
                char *ps2 = ge2_py_str(pid);
                inner = ge2_xsf("pid %s", ps2);
                free(ps2);
            } else {
                a = ge2_args0();
                ge2_aset_s(a, "name", ge2_jstr(nm));
                json_t *lbl = ge2_scall(s, "pg_service_label", a);
                if (ge2_is_raise(lbl)) {
                    free(name_s); free(owned); json_free(plan);
                    return lbl;
                }
                inner = strdup(ge2_jstr(lbl) ? ge2_jstr(lbl) : "");
                json_free(lbl);
            }
            char *frag = ge2_xsf("'%s' (%s)", name_s, inner);
            char *next = ge2_xsf("%s%s%s", owned, i ? ", " : "", frag);
            free(owned); free(frag); free(inner); free(name_s);
            owned = next;
        }
        char *m = ge2_xsf("profile(s) %s still run their own gateway; fold them with `%s`",
                          owned, GE2_MIGRATE_COMMAND);
        json_t *ret = json_string(m);
        free(m); free(owned); json_free(plan);
        return ret;
    }
    json_t *blocked = json_obj_get(plan, "blocked");
    if (!blocked) {
        json_t *e = ge2_raise_attr_carrier(plan, "blocked");
        json_free(plan);
        return e;
    }
    if (ge2_jtruth(blocked)) {
        json_t *blockers = json_obj_get(plan, "blockers");
        if (!blockers) {
            json_t *e = ge2_raise_attr_carrier(plan, "blockers");
            json_free(plan);
            return e;
        }
        json_t *jerr = NULL;
        char *joined = ge2_join_strs_checked(blockers, "; ", &jerr);
        if (jerr) { json_free(plan); return jerr; }
        json_t *ret = json_string(joined);
        free(joined); json_free(plan);
        return ret;
    }
    json_free(plan);
    return NULL;
}

/* PoP: ge2_flow_default_profile_home @ hermes_cli/gateway_multiplex_mode.py:_default_profile_home */
json_t *ge2_flow_default_profile_home(const ge2_seams_t *s) {
    json_t *r = ge2_scall(s, "get_default_hermes_root", ge2_args0());
    if (ge2_is_raise(r)) return r;
    /* the live fn returns a Path: normalize like Path.as_posix. */
    if (ge2_is_str(r)) {
        char *n = ge2_path_norm(r->str_val);
        json_t *ret = json_string(n);
        free(n); json_free(r);
        return ret;
    }
    return r;
}

/* PoP: ge2_flow_persist_resolved_default @ hermes_cli/gateway_multiplex_mode.py:persist_resolved_default */
json_t *ge2_flow_persist_resolved_default(const json_t *decision, const char *default_home,
                                          const ge2_seams_t *s) {
    const json_t *enabled = json_obj_get(decision, "enabled");
    const char *source = ge2_dec_source(decision);
    if (!ge2_jtruth(enabled) || !strcmp(source, "guard")) return json_bool(false);

    char *dh = NULL;
    if (default_home) {
        dh = ge2_path_norm(default_home);
    } else {
        json_t *r = ge2_flow_default_profile_home(s);
        GE2_PROP(r);
        if (ge2_jpresent(r)) dh = strdup(ge2_jstr(r) ? ge2_jstr(r) : "");
        json_free(r);
    }
    if (!dh) {
        /* None / "config.yaml" — truediv TypeError (probed shape). */
        return ge2_raise_obj("TypeError", "unsupported operand type(s) for /: 'NoneType' and 'str'");
    }
    char *cfg_path = ge2_path_join(dh, "config.yaml");

    json_t *result = NULL;
    json_t *cfg = NULL;
    json_t *in_file = NULL;
    json_t *err = NULL;
    json_t *r = NULL;

    /* try region: config_exists FIRST (conditional-expression condition),
     * then read_user_config_raw only when it is truthy. */
    json_t *a = ge2_args0();
    ge2_aset_s(a, "path", cfg_path);
    json_t *ex = ge2_scall(s, "config_exists", a);
    if (ge2_is_raise(ex)) { err = ex; goto caught; }
    bool exists = ge2_jtruth(ex);
    json_free(ex);
    if (exists) {
        a = ge2_args0();
        ge2_aset_s(a, "path", cfg_path);
        cfg = ge2_scall(s, "read_user_config_raw", a);
        if (ge2_is_raise(cfg)) { err = cfg; cfg = NULL; goto caught; }
        if (!ge2_jtruth(cfg)) { json_free(cfg); cfg = json_object(); }
        else if (!ge2_is_obj(cfg)) { err = ge2_raise_attr(ge2_py_type_name(cfg), "get"); json_free(cfg); cfg = NULL; goto caught; }
    } else {
        cfg = json_object();
    }
    {
        /* in_file = cfg.get("multiplex_profiles", section.get("multiplex_profiles"))
         * — dict.get returns a PRESENT null value; the default applies only
         * when the key is ABSENT (unlike explicit_multiplex_flag's fallback). */
        json_t *gw = json_obj_get(cfg, "gateway");
        json_t *section = ge2_is_obj(gw) ? gw : NULL;
        json_t *v = NULL;
        if (json_has(cfg, "multiplex_profiles")) v = json_obj_get(cfg, "multiplex_profiles");
        else if (section) v = json_obj_get(section, "multiplex_profiles");
        if (ge2_jpresent(v)) in_file = json_copy(v);
    }
    if (in_file && ge2_is_bool(in_file) && in_file->bool_val) {
        result = json_bool(false);   /* in_file is True — identity, not 1 */
        goto done;
    }
    a = ge2_args0();
    ge2_aset_s(a, "default_home", dh);
    ge2_aset_b(a, "value", true);
    r = ge2_scall(s, "write_multiplex_flag", a);
    if (ge2_is_raise(r)) { err = r; goto caught; }
    json_free(r);
    if (!strcmp(source, "retired-opt-out")) {
        char *marker = ge2_path_join(dh, GE2_REWRITTEN_MARKER_NAME);
        char *text = ge2_xsf("%s\n", GE2_RETIRED_OPT_OUT_REASON);
        a = ge2_args0();
        ge2_aset_s(a, "path", marker);
        ge2_aset_s(a, "text", text);
        ge2_aset_s(a, "encoding", "utf-8");
        r = ge2_scall(s, "marker_write_text", a);
        free(marker); free(text);
        if (ge2_is_raise(r)) { err = r; goto caught; }
        json_free(r);
    }
    goto info;

caught:
    if (!ge2_is_exception(ge2_raise_class(err))) {
        result = err;   /* BaseException passes through the except Exception */
        goto done;
    }
    json_free(err);
    a = ge2_args0();
    ge2_aset_s(a, "msg", "could not persist gateway.multiplex_profiles: true");
    ge2_aset_b(a, "exc_info", true);
    r = ge2_scall(s, "logger_debug", a);
    if (ge2_is_raise(r)) { result = r; goto done; }
    json_free(r);
    result = json_bool(false);
    goto done;

info:
    /* logger.info sits OUTSIDE the try; args carry the raw values: the posix
     * cfg_path and "unset" / repr(in_file). */
    {
        char *was = (!in_file || !ge2_jpresent(in_file)) ? strdup("unset") : ge2_py_repr(in_file);
        a = ge2_args0();
        ge2_aset_s(a, "msg", "Wrote gateway.multiplex_profiles: true to %s (was %s).");
        json_t *args = json_array();
        json_append(args, json_string(cfg_path));
        json_append(args, json_string(was));
        json_set(a, "args", args);
        free(was);
        r = ge2_scall(s, "logger_info", a);
        if (ge2_is_raise(r)) { result = r; goto done; }
        json_free(r);
    }
    result = json_bool(true);

done:
    json_free(cfg);
    json_free(in_file);
    free(cfg_path);
    free(dh);
    return result;
}

/* PoP: ge2_flow_retired_opt_out_notice_lines @ hermes_cli/gateway_multiplex_mode.py:retired_opt_out_notice_lines */
json_t *ge2_flow_retired_opt_out_notice_lines(void) {
    json_t *body = json_array();
    json_append(body, json_string(GE2_WARN " gateway.multiplex_profiles: false is retired and was rewritten to true;"));
    json_append(body, json_string("one gateway per host serves every profile."));
    json_append(body, json_string("A per-profile gateway is `gateway.standalone: true` in that profile's config"));
    json_append(body, json_string("(temporary shim) or `--force`."));
    json_t *r = ge2_flow_box(body);
    json_free(body);
    return r;
}

/* PoP: ge2_flow_consume_rewritten_notice @ hermes_cli/gateway_multiplex_mode.py:consume_rewritten_notice */
json_t *ge2_flow_consume_rewritten_notice(const char *default_home, const ge2_seams_t *s) {
    char *dh = NULL;
    if (default_home) {
        dh = ge2_path_norm(default_home);
    } else {
        json_t *r = ge2_flow_default_profile_home(s);
        GE2_PROP(r);
        if (ge2_jpresent(r)) dh = strdup(ge2_jstr(r) ? ge2_jstr(r) : "");
        json_free(r);
    }
    if (!dh) {
        return ge2_raise_obj("TypeError", "unsupported operand type(s) for /: 'NoneType' and 'str'");
    }
    char *marker = ge2_path_join(dh, GE2_REWRITTEN_MARKER_NAME);
    free(dh);
    json_t *a = ge2_args0();
    ge2_aset_s(a, "path", marker);
    json_t *ex = ge2_scall(s, "marker_exists", a);
    if (ge2_is_raise(ex)) { free(marker); return ex; }
    bool exists = ge2_jtruth(ex);
    json_free(ex);
    if (!exists) { free(marker); return json_array(); }
    a = ge2_args0();
    ge2_aset_s(a, "path", marker);
    json_t *r = ge2_scall(s, "marker_unlink", a);
    free(marker);
    if (ge2_is_raise(r)) {
        const char *k = ge2_raise_class(r);
        /* except OSError only — everything else (incl. BaseException) propagates. */
        bool oserr = false;
        const char *kk = k;
        for (int depth = 0; kk && depth < 32; depth++) {
            if (!strcmp(kk, "OSError")) { oserr = true; break; }
            if (!strcmp(kk, "BaseException")) break;
            kk = ge2_exc_parent(kk);
            if (!kk) break;   /* unknown custom class: not an OSError */
        }
        if (oserr) { json_free(r); return json_array(); }
        return r;
    }
    json_free(r);
    return ge2_flow_retired_opt_out_notice_lines();
}

/* PoP: ge2_flow_box @ hermes_cli/gateway_multiplex_mode.py:_box */
json_t *ge2_flow_box(const json_t *body) {
    if (!ge2_is_arr(body)) {
        char *m = ge2_xsf("'%s' object is not iterable", ge2_py_type_name(body));
        json_t *e = ge2_raise_obj("TypeError", m);
        free(m);
        return e;
    }
    if (body->c.count == 0) {
        /* max() over an empty iterable — probed CPython 3.14.9 message. */
        return ge2_raise_obj("ValueError", "max() iterable argument is empty");
    }
    size_t width = 0;
    for (size_t i = 0; i < body->c.count; i++) {
        const json_t *line = body->c.items[i];
        size_t l;
        if (ge2_is_str(line)) l = ge2_utf8_len(line->str_val);
        else if (ge2_is_arr(line) || ge2_is_obj(line)) l = line->c.count;
        else {
            char *m = ge2_xsf("object of type '%s' has no len()", ge2_py_type_name(line));
            json_t *e = ge2_raise_obj("TypeError", m);
            free(m);
            return e;
        }
        if (l > width) width = l;
    }
    width += 2;
    json_t *out = json_array();
    {
        char *bars = strdup("");
        for (size_t i = 0; i < width; i++) {
            char *next = ge2_xsf("%s%s", bars, GE2_BOX_H);
            free(bars); bars = next;
        }
        char *top = ge2_xsf("%s%s%s", GE2_BOX_TL, bars, GE2_BOX_TR);
        char *bot = ge2_xsf("%s%s%s", GE2_BOX_BL, bars, GE2_BOX_BR);
        json_append(out, json_string(top));
        free(top);
        for (size_t i = 0; i < body->c.count; i++) {
            const json_t *line = body->c.items[i];
            if (!ge2_is_str(line)) {
                /* len() succeeded above; ljust fails on non-str. */
                json_t *e = ge2_raise_attr(ge2_py_type_name(line), "ljust");
                json_free(out); free(bars); free(bot);
                return e;
            }
            size_t l = ge2_utf8_len(line->str_val);
            size_t pad = (width - 1) > l ? (width - 1) - l : 0;
            char *spaces = malloc(pad + 1);
            if (spaces) { memset(spaces, ' ', pad); spaces[pad] = '\0'; }
            char *row = ge2_xsf("%s %s%s%s", GE2_BOX_V, line->str_val,
                                spaces ? spaces : "", GE2_BOX_V);
            free(spaces);
            json_append(out, json_string(row));
            free(row);
        }
        json_append(out, json_string(bot));
        free(bot); free(bars);
    }
    return out;
}/* PoP: ge2_flow_resolve_multiplex_mode @ hermes_cli/gateway_multiplex_mode.py:resolve_multiplex_mode */
json_t *ge2_flow_resolve_multiplex_mode(json_t *config, const ge2_seams_t *s) {
    /* current = getattr(config, "multiplex_profiles", None) — a default
     * getattr never raises, even on a None carrier. */
    const json_t *cur_borrow = ge2_is_obj(config) ? json_obj_get(config, "multiplex_profiles") : NULL;
    json_t *current = cur_borrow ? json_copy(cur_borrow) : NULL;  /* own: the key may be re-set below */
    json_t *standalone = ge2_flow_standalone_launcher_decision(config, s);
    if (ge2_is_raise(standalone)) { json_free(current); return standalone; }
    if (ge2_jpresent(standalone)) { json_free(current); return standalone; }
    json_free(standalone);
    json_t *settled = ge2_flow_settled_explicit_flag(current);
    if (ge2_jtruth(settled)) {
        json_free(settled); json_free(current);
        return ge2_decision(true, "config", "");
    }
    json_free(settled);
    bool retired_opt_out = ge2_is_bool(current) && !current->bool_val;  /* current is False */
    json_free(current);

    json_t *blocker = ge2_flow_implicit_multiplex_blocker(s);
    if (ge2_is_raise(blocker)) {
        if (!ge2_is_exception(ge2_raise_class(blocker))) return blocker;
        char *emsg = ge2_raise_str(blocker);   /* str(exc); KeyError -> repr */
        json_free(blocker);
        json_t *a = ge2_args0();
        ge2_aset_s(a, "msg", "Multiplex preflight failed; starting standalone: %s");
        json_t *args = json_array();
        json_append(args, json_string(emsg));
        json_set(a, "args", args);
        ge2_aset_b(a, "exc_info", true);
        json_t *r = ge2_scall(s, "logger_warning", a);
        if (ge2_is_raise(r)) { free(emsg); return r; }
        json_free(r);
        char *m = ge2_xsf("preflight failed (%s)", emsg);
        free(emsg);
        blocker = json_string(m);
        free(m);
    }
    json_t *dec;
    if (ge2_jtruth(blocker)) {
        char *bs = ge2_py_str(blocker);
        dec = ge2_decision(false, "guard", bs);
        free(bs);
    } else if (retired_opt_out) {
        dec = ge2_decision(true, "retired-opt-out", GE2_RETIRED_OPT_OUT_REASON);
    } else {
        dec = ge2_decision(true, "default", GE2_DEFAULT_APPLIES_REASON);
    }
    json_free(blocker);
    if (!config) {
        json_free(dec);
        return ge2_raise_obj("AttributeError",
            "'NoneType' object has no attribute 'multiplex_profiles' and no __dict__ for setting new attributes");
    }
    const json_t *en = json_obj_get(dec, "enabled");
    json_set(config, "multiplex_profiles", json_bool(en && en->bool_val));
    return dec;
}

/* PoP: ge2_flow_record_multiplex_decision @ hermes_cli/gateway_multiplex_mode.py:record_multiplex_decision */
json_t *ge2_flow_record_multiplex_decision(const json_t *decision, json_t *state,
                                           const ge2_seams_t *s) {
    json_t *err = NULL;
    json_t *r = ge2_scall(s, "owns_gateway_runtime_lock", ge2_args0());
    if (ge2_is_raise(r)) { err = r; goto caught; }
    bool owns = ge2_jtruth(r);
    json_free(r);
    if (!owns) {
        /* _pending_decision[:] = [decision] */
        json_t *pend = json_array();
        json_append(pend, json_copy(decision));
        json_set(state, "pending", pend);
        return NULL;
    }
    /* _pending_decision.clear() BEFORE the write seam. */
    json_set(state, "pending", json_array());
    {
        const char *source = ge2_dec_source(decision);
        const json_t *reason = ge2_dec_reason_node(decision);
        json_t *a = ge2_args0();
        if (!strcmp(source, "guard")) {
            /* reason defaults to "" (dataclass default); a null reason is
             * None and the kwarg drops on both sides. */
            if (!reason) ge2_aset_s(a, "multiplex_standalone_reason", "");
            else ge2_aset_j(a, "multiplex_standalone_reason", reason);
        }
        r = ge2_scall(s, "write_runtime_status", a);
        if (ge2_is_raise(r)) { err = r; goto caught; }
        json_free(r);
    }
    return NULL;

caught:
    if (!ge2_is_exception(ge2_raise_class(err))) return err;
    json_free(err);
    {
        json_t *a = ge2_args0();
        ge2_aset_s(a, "msg", "could not record the multiplex decision");
        ge2_aset_b(a, "exc_info", true);
        r = ge2_scall(s, "logger_debug", a);
        if (ge2_is_raise(r)) return r;
        json_free(r);
    }
    return NULL;
}

/* PoP: ge2_flow_publish_pending_multiplex_decision @ hermes_cli/gateway_multiplex_mode.py:publish_pending_multiplex_decision */
json_t *ge2_flow_publish_pending_multiplex_decision(json_t *state, const ge2_seams_t *s) {
    const json_t *pend = json_obj_get(state, "pending");
    if (ge2_is_arr(pend) && pend->c.count > 0) {
        /* record's json_set(state,"pending",...) frees the old array — copy
         * the decision out before delegating. */
        json_t *last = json_copy(json_get(pend, pend->c.count - 1));
        json_t *r = ge2_flow_record_multiplex_decision(last, state, s);
        json_free(last);
        return r;
    }
    return NULL;
}

/* PoP: ge2_flow_log_multiplex_decision @ hermes_cli/gateway_multiplex_mode.py:log_multiplex_decision */
json_t *ge2_flow_log_multiplex_decision(const json_t *decision, json_t *state,
                                        const ge2_seams_t *s) {
    json_t *r = ge2_flow_record_multiplex_decision(decision, state, s);
    GE2_PROP(r);
    json_free(r);
    const char *source = ge2_dec_source(decision);
    const json_t *reason = ge2_dec_reason_node(decision);
    const char *reason_s = ge2_jstr(reason);
    if (!reason_s) reason_s = "";   /* dataclass default when absent */

    if (!strcmp(source, "retired-opt-out")) {
        json_t *a = ge2_args0();
        ge2_aset_s(a, "msg", "%s");
        json_t *args = json_array();
        json_append(args, json_string(GE2_RETIRED_OPT_OUT_REASON));
        json_set(a, "args", args);
        r = ge2_scall(s, "logger_warning", a);
        GE2_PROP(r);
        json_free(r);
        r = ge2_flow_persist_resolved_default(decision, NULL, s);
        GE2_PROP(r);
        json_free(r);
        json_t *lines = ge2_flow_retired_opt_out_notice_lines();
        GE2_PROP(lines);
        for (size_t i = 0; i < lines->c.count; i++) {
            a = ge2_args0();
            ge2_aset_j(a, "line", lines->c.items[i]);
            r = ge2_scall(s, "print", a);
            if (ge2_is_raise(r)) { json_free(lines); return r; }
            json_free(r);
        }
        json_free(lines);
    } else if (!strcmp(source, "guard") && !strcmp(reason_s, GE2_SINGLE_PROFILE_REASON)) {
        json_t *a = ge2_args0();
        ge2_aset_s(a, "msg", "Single-profile install: gateway.multiplex_profiles unset, serving the default profile only.");
        json_set(a, "args", json_array());
        r = ge2_scall(s, "logger_info", a);
        GE2_PROP(r);
        json_free(r);
    } else if (!strcmp(source, "guard")) {
        json_t *a = ge2_args0();
        ge2_aset_s(a, "msg", "This gateway stays standalone: %s. It serves only the launching profile.");
        json_t *args = json_array();
        json_append(args, reason ? json_copy(reason) : json_string(""));
        json_set(a, "args", args);
        r = ge2_scall(s, "logger_warning", a);
        GE2_PROP(r);
        json_free(r);
        json_t *lines = ge2_flow_standalone_warning_lines(decision, NULL, s);
        GE2_PROP(lines);
        for (size_t i = 0; i < lines->c.count; i++) {
            a = ge2_args0();
            ge2_aset_j(a, "line", lines->c.items[i]);
            r = ge2_scall(s, "print", a);
            if (ge2_is_raise(r)) { json_free(lines); return r; }
            json_free(r);
        }
        json_free(lines);
    } else if (!strcmp(source, "default")) {
        json_t *a = ge2_args0();
        ge2_aset_s(a, "msg", "Serving every profile on this host (gateway.multiplex_profiles unset; default on).");
        json_set(a, "args", json_array());
        r = ge2_scall(s, "logger_info", a);
        GE2_PROP(r);
        json_free(r);
        r = ge2_flow_persist_resolved_default(decision, NULL, s);
        GE2_PROP(r);
        json_free(r);
    }
    return NULL;
}

/* PoP: ge2_flow_unserved_profiles @ hermes_cli/gateway_multiplex_mode.py:unserved_profiles */
json_t *ge2_flow_unserved_profiles(const ge2_seams_t *s) {
    json_t *home = ge2_scall(s, "get_hermes_home", ge2_args0());
    GE2_PROP(home);
    char *hs = ge2_jstr(home) ? strdup(ge2_jstr(home)) : NULL;
    json_free(home);
    json_t *a = ge2_args0();
    ge2_aset_s(a, "home", hs);
    json_t *nm = ge2_scall(s, "profile_name_for_home", a);
    free(hs);
    GE2_PROP(nm);
    /* me = profile_name_for_home(...) or "default" */
    char *me = ge2_jtruth(nm) ? ge2_py_str(nm) : strdup("default");
    json_free(nm);
    json_t *me_node = json_string(me);
    free(me);

    a = ge2_args0();
    ge2_aset_b(a, "multiplex", true);
    ge2_aset_b(a, "include_parked", true);
    json_t *ps = ge2_scall(s, "profiles_to_serve", a);
    if (ge2_is_raise(ps)) { json_free(me_node); return ps; }
    if (!ge2_jpresent(ps) || !ge2_is_arr(ps)) {
        char *m = ge2_xsf("'%s' object is not iterable", ge2_py_type_name(ps));
        json_t *e = ge2_raise_obj("TypeError", m);
        free(m); json_free(ps); json_free(me_node);
        return e;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < ps->c.count; i++) {
        const json_t *pair = ps->c.items[i];
        /* for name, _home in ... — unpack each item. */
        if (!ge2_is_arr(pair)) {
            char *m = ge2_xsf("cannot unpack non-iterable %s object", ge2_py_type_name(pair));
            json_t *e = ge2_raise_obj("TypeError", m);
            free(m); json_free(out); json_free(ps); json_free(me_node);
            return e;
        }
        if (pair->c.count != 2) {
            char *m = pair->c.count > 2
                ? ge2_xsf("too many values to unpack (expected 2, got %zu)", pair->c.count)
                : ge2_xsf("not enough values to unpack (expected 2, got %zu)", pair->c.count);
            json_t *e = ge2_raise_obj("ValueError", m);
            free(m); json_free(out); json_free(ps); json_free(me_node);
            return e;
        }
        const json_t *name = json_get(pair, 0);
        if (!ge2_py_eq(name, me_node)) json_append(out, json_copy(name));
    }
    json_free(ps);
    json_free(me_node);
    return out;
}

/* PoP: ge2_flow_standalone_warning_lines @ hermes_cli/gateway_multiplex_mode.py:standalone_warning_lines */
json_t *ge2_flow_standalone_warning_lines(const json_t *decision, const json_t *unserved,
                                          const ge2_seams_t *s) {
    const json_t *source = json_obj_get(decision, "source");
    const json_t *reason = ge2_dec_reason_node(decision);
    const char *reason_s = ge2_jstr(reason);
    if (!reason_s) reason_s = "";
    if (!ge2_is_str(source) || strcmp(source->str_val, "guard")
        || !strcmp(reason_s, GE2_SINGLE_PROFILE_REASON)) {
        return json_array();
    }
    json_t *us = NULL;
    if (!ge2_jpresent(unserved)) {
        us = ge2_flow_unserved_profiles(s);
        if (ge2_is_raise(us)) {
            if (!ge2_is_exception(ge2_raise_class(us))) return us;
            json_free(us);
            us = json_array();   /* except Exception: unserved = [] */
        }
    } else {
        us = json_copy(unserved);
    }
    if (!ge2_jtruth(us)) { json_free(us); return json_array(); }
    json_t *jerr = NULL;
    char *joined = ge2_join_strs_checked(us, ", ", &jerr);
    if (jerr) { json_free(us); return jerr; }
    char *reason_py = reason ? ge2_py_str(reason) : strdup("");  /* f"Why: {decision.reason}" */
    json_t *body = json_array();
    json_append(body, json_string(GE2_WARN " This gateway is STANDALONE: it serves only its own profile."));
    char *l2 = ge2_xsf("Profiles NOT served (their bots stay silent): %s", joined);
    json_append(body, json_string(l2));
    char *l3 = ge2_xsf("Why: %s", reason_py);
    json_append(body, json_string(l3));
    char *l4 = ge2_xsf("Fix: %s", GE2_MIGRATE_COMMAND);
    json_append(body, json_string(l4));
    free(l2); free(l3); free(l4); free(joined); free(reason_py);
    json_t *r = ge2_flow_box(body);
    json_free(body);
    json_free(us);
    return r;
}

/* PoP: ge2_flow_recorded_standalone_reason @ hermes_cli/gateway_multiplex_mode.py:recorded_standalone_reason */
json_t *ge2_flow_recorded_standalone_reason(const char *state_path, const ge2_seams_t *s) {
    json_t *a = ge2_args0();
    ge2_aset_s(a, "state_path", state_path);
    json_t *record = ge2_scall(s, "read_runtime_status", a);
    if (ge2_is_raise(record)) {
        if (ge2_exc_in_record_tuple(ge2_raise_class(record))) { json_free(record); return NULL; }
        return record;
    }
    if (!ge2_jtruth(record)) { json_free(record); record = json_object(); }
    else if (!ge2_is_obj(record)) {
        /* record.get on a non-dict: AttributeError, caught by the tuple. */
        json_free(record);
        return NULL;
    }
    const json_t *gs = json_obj_get(record, "gateway_state");
    /* record.get("gateway_state") in (None, "stopped", "startup_failed") */
    if (!ge2_jpresent(gs)
        || (ge2_is_str(gs) && (!strcmp(gs->str_val, "stopped") || !strcmp(gs->str_val, "startup_failed")))) {
        json_free(record);
        return NULL;
    }
    a = ge2_args0();
    ge2_aset_j(a, "record", record);
    json_t *live = ge2_scall(s, "runtime_status_pid_is_live", a);
    if (ge2_is_raise(live)) {
        json_free(record);
        if (ge2_exc_in_record_tuple(ge2_raise_class(live))) { json_free(live); return NULL; }
        return live;
    }
    bool islive = ge2_jtruth(live);
    json_free(live);
    if (!islive) { json_free(record); return NULL; }
    const json_t *reason = json_obj_get(record, "multiplex_standalone_reason");
    json_t *ret = (ge2_is_str(reason) && reason->str_val && reason->str_val[0])
        ? json_string(reason->str_val) : NULL;
    json_free(record);
    return ret;
}

/* PoP: ge2_flow_recorded_standalone_warning_lines @ hermes_cli/gateway_multiplex_mode.py:recorded_standalone_warning_lines */
json_t *ge2_flow_recorded_standalone_warning_lines(const ge2_seams_t *s) {
    json_t *reason = ge2_flow_recorded_standalone_reason(NULL, s);
    GE2_PROP(reason);
    if (!ge2_jtruth(reason)) { json_free(reason); return json_array(); }
    json_t *dec = ge2_decision(false, "guard", ge2_jstr(reason));
    json_free(reason);
    json_t *r = ge2_flow_standalone_warning_lines(dec, NULL, s);
    json_free(dec);
    return r;
}