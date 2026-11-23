/*
 * gateway_cli_flow2.c — hermes_cli/gateway.py UNCLAIMED FLOW remainder (C11 port).
 * LIVE authority: C:/Users/eman5/hermes-agent/hermes_cli/gateway.py
 *
 * Second-wave port of the gateway CLI's service unit-environment / launcher /
 * linger / identity-print cluster (16 unclaimed defs; the 25 flow1 siblings in
 * gateway_cli_flow.c and the gateway_wrappers/gw_service_lifecycle claims are
 * SEAMED, never re-claimed). Self-contained, libjson-only. Every live effect
 * (unit-file reads, env reads, foreign-module imports, subprocess runs, Path
 * resolve/expanduser, print/log emission, pwd lookups) enters through ONE
 * universal seam; the cores perform NO I/O.
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a BORROWED JSON
 * object (the seam records a copy and must not free it); the result is a NEW
 * json_t* the caller frees. NULL == Python None (never a JSON_NULL node);
 * {"__raise__": cls, "msg": msg} == a raised exception. The harness records
 * {"ev": name, "args": args, "ret": result} per call in order; the oracle's
 * recording stubs emit the identical shape driving LIVE Python with the same
 * scripts. BaseException kinds (SystemExit/KeyboardInterrupt/GeneratorExit/
 * CancelledError) PASS THROUGH every `except Exception`/typed-catch site.
 *
 * FAIL-CLOSED: a MISSING script for a seam returns a RuntimeError raise spec
 * {"__raise__":"RuntimeError","msg":"no script for seam: <name>"} (recorded as
 * the seam ret, then propagated); a scripted JSON null is a valid None (NULL
 * pointer). With no seam installed the dispatcher is also fail-closed.
 *
 * RETURN CONTRACT: every ported function returns a NEW json_t* (caller frees):
 * NULL == None; a {"__raise__": ...} object == a propagated exception. The
 * flows are stateless; observable effects are the ordered seam trace + ret.
 *
 * Seam name table (name -> live call; args key order is part of the trace):
 *   unit_environment_value   _unit_environment_value(unit_path,name)  args{unit_path,name} ret str|null
 *   get_systemd_unit_path    get_systemd_unit_path(system=)           args{system} ret path str
 *   get_service_name         get_service_name()                       args{} ret str
 *   get_hermes_home          hermes_cli.config.get_hermes_home()      args{} ret path str
 *   environ_get              os.environ.get(name,default)             args{name,default?} ret str|null
 *   getuid                   os.getuid()                              args{} ret int
 *   geteuid                  os.geteuid()                             args{} ret int
 *   get_real_home            hermes_constants.get_real_home()         args{} ret str
 *   is_linux                 is_linux()                               args{} ret bool
 *   is_container             hermes_constants.is_container()          args{} ret bool
 *   remap_path_for_user      _remap_path_for_user(path,target)        args{path,target_home_dir} ret str
 *   systemd_env_line         gateway_bundled_env.systemd_env_line(n,v) args{name,value} ret str
 *   subprocess_run           subprocess.run(args,**kw)                args{args,check,timeout,...} ret obj|null
 *   pwd_getpwnam             pwd.getpwnam(username)                   args{username} ret {pw_uid,..}
 *   poll_until               agent.deadline.poll_until(fn,to,interval) args{timeout,interval} ret bool
 *   ensure_linger_enabled    _ensure_linger_enabled(username,system=) args{username,system} ret bool
 *   systemd_unit_is_active   _systemd_unit_is_active(system=)         args{system} ret bool
 *   systemd_cli_bits         _systemd_cli_bits(system)                args{system} ret [sudo,scope,user]
 *   runtime_health_lines     _runtime_health_lines()                  args{} ret [str]
 *   print_info_lines         _print_info_lines(*lines)                args{lines} ret null
 *   foreign_systemd_unit_home gateway_service_owner.foreign_systemd_unit_home(s) args{system} ret str|null
 *   sync_hermes_home_from_systemd_unit _sync_hermes_home_from_systemd_unit(system=) args{system} ret null
 *   hermes_home_from_systemd_unit_file _hermes_home_from_systemd_unit_file(system=) args{system} ret str|null
 *   sealed_steward           hermes_cli.steward.sealed_steward(root)  args{root} ret str|null
 *   path_resolve             Path.resolve()                           args{path} ret str
 *   path_expanduser          Path.expanduser()                        args{path} ret str
 *   print                    builtin print(text?)                     args{text?} ret null
 *   print_error              hermes_cli.setup.print_error(text)       args{text} ret null
 *   print_warning            hermes_cli.setup.print_warning(text)     args{text} ret null
 *
 * Constants embedded (real on both sides, NOT seamed): _SERVICE_BASE="hermes-gateway",
 * _SYSTEM_UNIT_DIR="/etc/systemd/system", _CAPTURE_TEXT (folded into subprocess_run args),
 * PROJECT_ROOT="/proj/root", STEWARD_APT_TERMUX="apt-termux".
 *
 * Internal composition runs REAL on both sides (no seam): _installed_unit_ld_library_path
 * inside _ld_library_path_line; user_systemd_unit_dir + _hermes_home_pinned_by_unit inside
 * _bare_unit_pinned_home; _wait_for_target_user_bus inside _ensure_system_service_linger.
 *
 * Fixture domain (kept identical on both sides): ASCII text inputs; POSIX lexical
 * paths (resolve/expanduser are SEAMS returning scripted strings, no real
 * filesystem; CLEAN paths only -- no '.'/'..' segments, no trailing slashes, no
 * leading '//', matching flow1's lexical join, since Path normalization of those
 * is outside the lexical model); integral JSON numbers read as int; result/pwd
 * doubles are JSON objects carrying the attrs the live body reads
 * (stderr/stdout/returncode; pw_uid). Fixed live message constants carry
 * byte-exact UTF-8 (check/cross/warning/em-dash).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include "json.h"

/* ── JSON / Python-value helpers ── */
static bool gc2_jtruth(const json_t *v) {
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
static bool gc2_jpresent(const json_t *v) { return v && v->type != JSON_NULL; }
static bool gc2_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool gc2_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool gc2_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool gc2_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static const char *gc2_jstr(const json_t *v) {
    return (gc2_is_str(v) && v->str_val) ? v->str_val : NULL;
}
static char *gc2_xsf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2); return buf ? buf : strdup("");
}
static char *gc2_py_float_str(double d) {
    if (d == (double)(long long)d) return gc2_xsf("%.0f", d);
    char buf[64];
    for (int prec = 15; prec <= 17; prec++) {
        snprintf(buf, sizeof(buf), "%.*g", prec, d);
        if (strtod(buf, NULL) == d) break;
    }
    return strdup(buf);
}
static char *gc2_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: return gc2_py_float_str(v->num_val);
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        default:          return strdup("");
    }
}
static const char *gc2_py_type_name(const json_t *v) {
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

/* ── universal seam ── */
typedef struct gc2_trace { json_t *events; } gc2_trace_t;
typedef struct gc2_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} gc2_seams_t;

static void gc2_trace_add(gc2_trace_t *t, json_t *ev) {
    if (!t) { json_free(ev); return; }
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static json_t *gc2_raise(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
static bool gc2_is_raise(const json_t *r) {
    return gc2_is_obj(r) && json_obj_get(r, "__raise__") != NULL;
}
static const char *gc2_raise_class(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
static bool gc2_is_base_exc(const char *cls) {
    return !strcmp(cls, "SystemExit") || !strcmp(cls, "KeyboardInterrupt")
        || !strcmp(cls, "GeneratorExit") || !strcmp(cls, "CancelledError");
}

/* ── FULL builtin exception-parent hierarchy (py3.14.9 quarry venv) ──
 * Audited against builtins: every BaseException subclass -> first base name.
 * OSError aliases (IOError/EnvironmentError/WindowsError) fold to OSError;
 * ExceptionGroup routes through Exception (except-Exception contract); the
 * BaseException trio + BaseExceptionGroup parent BaseException ONLY, so they
 * pass through `except Exception`. BaseException is the root (parent ""). */
typedef struct { const char *cls; const char *parent; } gc2_exc_ent_t;
static const gc2_exc_ent_t GC2_EXC_TABLE[] = {
    {"ArithmeticError", "Exception"}, {"AssertionError", "Exception"},
    {"AttributeError", "Exception"}, {"BaseException", ""},
    {"BaseExceptionGroup", "BaseException"}, {"BlockingIOError", "OSError"},
    {"BrokenPipeError", "ConnectionError"}, {"BufferError", "Exception"},
    {"BytesWarning", "Warning"}, {"ChildProcessError", "OSError"},
    {"ConnectionAbortedError", "ConnectionError"}, {"ConnectionError", "OSError"},
    {"ConnectionRefusedError", "ConnectionError"}, {"ConnectionResetError", "ConnectionError"},
    {"DeprecationWarning", "Warning"}, {"EOFError", "Exception"},
    {"EncodingWarning", "Warning"}, {"EnvironmentError", "OSError"},
    {"Exception", "BaseException"}, {"ExceptionGroup", "Exception"},
    {"FileExistsError", "OSError"}, {"FileNotFoundError", "OSError"},
    {"FloatingPointError", "ArithmeticError"}, {"FutureWarning", "Warning"},
    {"GeneratorExit", "BaseException"}, {"IOError", "OSError"},
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
    {"UserWarning", "Warning"}, {"ValueError", "Exception"},
    {"Warning", "Exception"}, {"WindowsError", "OSError"},
    {"ZeroDivisionError", "ArithmeticError"}, {"_IncompleteInputError", "SyntaxError"},
};
static const char *gc2_exc_parent(const char *cls) {
    for (size_t i = 0; i < sizeof(GC2_EXC_TABLE)/sizeof(GC2_EXC_TABLE[0]); i++)
        if (!strcmp(GC2_EXC_TABLE[i].cls, cls)) return GC2_EXC_TABLE[i].parent;
    return NULL;
}
/* True iff cls walks up the parent chain to `target` (root "" stops the walk). */
static bool gc2_exc_is_a(const char *cls, const char *target) {
    char cur[128];
    snprintf(cur, sizeof(cur), "%s", cls);
    for (int depth = 0; depth < 32; depth++) {
        if (!strcmp(cur, target)) return true;
        const char *p = gc2_exc_parent(cur);
        if (!p || !*p) return false;
        snprintf(cur, sizeof(cur), "%s", p);
    }
    return false;
}
static bool gc2_exc_is_exception(const char *cls) { return gc2_exc_is_a(cls, "Exception"); }
static bool gc2_cls_runtimeerror(const char *c) { return gc2_exc_is_a(c, "RuntimeError"); }
static bool gc2_cls_valueerror(const char *c) { return gc2_exc_is_a(c, "ValueError"); }

/* Fail-closed dispatcher: no seam installed -> RuntimeError raise spec.
 * Ownership: gc2_call OWNS args (frees it after the call); the seam BORROWS args
 * (records a deep copy, must not free it). Results are NEW (caller frees). */
static json_t *gc2_call(const gc2_seams_t *s, const char *name, const json_t *args) {
    if (!s || !s->call) {
        json_free((json_t *)args);
        return gc2_raise("RuntimeError", "no seam installed");
    }
    json_t *r = s->call(s->ctx, name, args);
    json_free((json_t *)args);
    return r;
}
static json_t *gc2_args0(void) { return json_object(); }
static void gc2_set_str(json_t *o, const char *k, const char *v) {
    if (v) json_set(o, k, json_string(v));
}
static void gc2_set_bool(json_t *o, const char *k, bool v) { json_set(o, k, json_bool(v)); }
static void gc2_set_num(json_t *o, const char *k, double v) { json_set(o, k, json_number(v)); }
static void gc2_set_node(json_t *o, const char *k, const json_t *v) {
    if (v && v->type != JSON_NULL) json_set(o, k, json_copy(v));
}

/* ── ASCII string / POSIX lexical path helpers ── */
static bool gc2_is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || c == '\x1c' || c == '\x1d' || c == '\x1e' || c == '\x1f';
}
static char *gc2_strip(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && gc2_is_ws(s[a])) a++;
    while (b > a && gc2_is_ws(s[b - 1])) b--;
    char *out = malloc(b - a + 1);
    memcpy(out, s + a, b - a); out[b - a] = '\0';
    return out;
}
/* str.replace(old, new) — one full pass (sequential passes for chained replaces). */
static char *gc2_replace_all(const char *s, const char *old, const char *nw) {
    size_t ol = strlen(old), nl = strlen(nw);
    if (ol == 0) return strdup(s);
    size_t cap = strlen(s) + 1, len = 0;
    char *out = malloc(cap);
    const char *p = s;
    while (*p) {
        if (!strncmp(p, old, ol)) {
            while (len + nl + 1 > cap) { cap *= 2; out = realloc(out, cap); }
            memcpy(out + len, nw, nl); len += nl; p += ol;
        } else {
            if (len + 2 > cap) { cap *= 2; out = realloc(out, cap); }
            out[len++] = *p++;
        }
    }
    out[len] = '\0';
    return out;
}
static char *gc2_path_join(const char *a, const char *b) {
    if (!b || !*b) return strdup(a ? a : "");
    if (b[0] == '/') return strdup(b);
    if (!a || !*a) return strdup(b);
    size_t na = strlen(a);
    if (a[na - 1] == '/') return gc2_xsf("%s%s", a, b);
    return gc2_xsf("%s/%s", a, b);
}
/* Path.resolve() / Path.expanduser() modeled as seams (scripted strings). */
static json_t *gc2_path_resolve(const gc2_seams_t *s, const char *path) {
    json_t *a = gc2_args0(); gc2_set_str(a, "path", path);
    return gc2_call(s, "path_resolve", a);
}
static json_t *gc2_path_expanduser(const gc2_seams_t *s, const char *path) {
    json_t *a = gc2_args0(); gc2_set_str(a, "path", path);
    return gc2_call(s, "path_expanduser", a);
}/* ── print seam helpers (return seam result: NULL on success, raise on error) ── */
static json_t *gc2_print(const gc2_seams_t *s, const char *text) {
    json_t *a = gc2_args0();
    if (text) gc2_set_str(a, "text", text);
    return gc2_call(s, "print", a);
}
static json_t *gc2_print_error(const gc2_seams_t *s, const char *text) {
    json_t *a = gc2_args0(); gc2_set_str(a, "text", text);
    return gc2_call(s, "print_error", a);
}
static json_t *gc2_print_warning(const gc2_seams_t *s, const char *text) {
    json_t *a = gc2_args0(); gc2_set_str(a, "text", text);
    return gc2_call(s, "print_warning", a);
}
static json_t *gc2_seam_unit_is_active(const gc2_seams_t *s, bool system) {
    json_t *a = gc2_args0(); gc2_set_bool(a, "system", system);
    return gc2_call(s, "systemd_unit_is_active", a);
}

/* ── ported flows (composition order: callees before callers) ── */

/* PoP: gc2_systemd_command @ hermes_cli/gateway.py:_systemd_command */
static json_t *gc2_systemd_command(const json_t *argv) {
    if (!gc2_is_arr(argv))
        return gc2_raise("TypeError", gc2_xsf("'%s' object is not iterable", gc2_py_type_name(argv)));
    char *joined = strdup("");
    for (size_t i = 0; i < argv->c.count; i++) {
        const json_t *part = argv->c.items[i];
        if (!gc2_is_str(part)) {
            free(joined);
            return gc2_raise("AttributeError",
                gc2_xsf("'%s' object has no attribute 'replace'", gc2_py_type_name(part)));
        }
        const char *p = part->str_val ? part->str_val : "";
        char *e1 = gc2_replace_all(p, "\\", "\\\\");
        char *e2 = gc2_replace_all(e1, "\"", "\\\"");
        char *e3 = gc2_replace_all(e2, "%", "%%");
        char *e4 = gc2_replace_all(e3, "$", "$$");
        free(e1); free(e2); free(e3);
        char *quoted = gc2_xsf("\"%s\"", e4);
        free(e4);
        char *next = gc2_xsf("%s%s%s", joined, i ? " " : "", quoted);
        free(joined); free(quoted);
        joined = next;
    }
    json_t *ret = json_string(joined);
    free(joined);
    return ret;
}

/* PoP: gc2_completed_process_detail @ hermes_cli/gateway.py:_completed_process_detail */
static json_t *gc2_completed_process_detail(const json_t *result) {
    if (!gc2_is_obj(result))
        return gc2_raise("AttributeError",
            gc2_xsf("'%s' object has no attribute 'stderr'", gc2_py_type_name(result)));
    const json_t *err = json_obj_get(result, "stderr");
    const json_t *out = json_obj_get(result, "stdout");
    const json_t *rc = json_obj_get(result, "returncode");
    char *val;
    if (gc2_jtruth(err)) val = gc2_py_str(err);
    else if (gc2_jtruth(out)) val = gc2_py_str(out);
    else { char *rcs = gc2_py_str(rc); val = gc2_xsf("exit %s", rcs); free(rcs); }
    char *stripped = gc2_strip(val);
    free(val);
    json_t *ret = json_string(stripped);
    free(stripped);
    return ret;
}

/* PoP: gc2_hermes_home_pinned_by_unit @ hermes_cli/gateway.py:_hermes_home_pinned_by_unit */
static json_t *gc2_hermes_home_pinned_by_unit(const char *unit_path, const gc2_seams_t *s) {
    json_t *a = gc2_args0();
    gc2_set_str(a, "unit_path", unit_path);
    gc2_set_str(a, "name", "HERMES_HOME");
    return gc2_call(s, "unit_environment_value", a);
}

/* PoP: gc2_installed_unit_ld_library_path @ hermes_cli/gateway.py:_installed_unit_ld_library_path */
static json_t *gc2_installed_unit_ld_library_path(bool system, const gc2_seams_t *s) {
    json_t *a = gc2_args0(); gc2_set_bool(a, "system", system);
    json_t *up = gc2_call(s, "get_systemd_unit_path", a);
    if (gc2_is_raise(up)) return up;
    json_t *ua = gc2_args0();
    gc2_set_str(ua, "unit_path", gc2_jstr(up));
    gc2_set_str(ua, "name", "LD_LIBRARY_PATH");
    json_free(up);
    json_t *r = gc2_call(s, "unit_environment_value", ua);
    if (gc2_is_raise(r)) return r;
    if (gc2_jtruth(r)) return r;
    json_free(r);
    return json_string("");
}

/* PoP: gc2_ld_library_path_line @ hermes_cli/gateway.py:_ld_library_path_line */
static json_t *gc2_ld_library_path_line(bool system, const char *target_home_dir, const gc2_seams_t *s) {
    json_t *ea = gc2_args0();
    gc2_set_str(ea, "name", "LD_LIBRARY_PATH");
    gc2_set_str(ea, "default", "");
    json_t *env = gc2_call(s, "environ_get", ea);
    if (gc2_is_raise(env)) return env;
    char *raw;
    if (gc2_jtruth(env)) { raw = gc2_py_str(env); json_free(env); }
    else {
        json_free(env);
        json_t *inst = gc2_installed_unit_ld_library_path(system, s);
        if (gc2_is_raise(inst)) return inst;
        raw = gc2_py_str(inst); json_free(inst);
    }
    json_t *comps = json_array();
    {
        char *tmp = strdup(raw);
        char *tok = strtok(tmp, ":");
        while (tok) { if (*tok) json_append(comps, json_string(tok)); tok = strtok(NULL, ":"); }
        free(tmp);
    }
    free(raw);
    if (target_home_dir) {
        json_t *remapped = json_array();
        for (size_t i = 0; i < comps->c.count; i++) {
            const char *p = gc2_jstr(comps->c.items[i]);
            json_t *ra = gc2_args0();
            gc2_set_str(ra, "path", p);
            gc2_set_str(ra, "target_home_dir", target_home_dir);
            json_t *rr = gc2_call(s, "remap_path_for_user", ra);
            if (gc2_is_raise(rr)) { json_free(comps); json_free(remapped); return rr; }
            json_append(remapped, rr ? rr : json_null());
        }
        json_free(comps); comps = remapped;
    }
    json_t *ret;
    if (comps->c.count > 0) {
        char *joined = strdup("");
        for (size_t i = 0; i < comps->c.count; i++) {
            const char *p = gc2_jstr(comps->c.items[i]);
            char *next = gc2_xsf("%s%s%s", joined, i ? ":" : "", p ? p : "");
            free(joined); joined = next;
        }
        json_t *sa = gc2_args0();
        gc2_set_str(sa, "name", "LD_LIBRARY_PATH");
        gc2_set_str(sa, "value", joined);
        free(joined);
        ret = gc2_call(s, "systemd_env_line", sa);
    } else {
        ret = json_string("");
    }
    json_free(comps);
    return ret;
}

/* PoP: gc2_loginctl_enable_linger @ hermes_cli/gateway.py:_loginctl_enable_linger */
static json_t *gc2_loginctl_enable_linger(const char *username, const gc2_seams_t *s) {
    json_t *argv = json_array();
    json_append(argv, json_string("loginctl"));
    json_append(argv, json_string("enable-linger"));
    json_append(argv, json_string(username ? username : ""));
    json_t *a = gc2_args0();
    json_set(a, "args", argv);
    gc2_set_bool(a, "check", false);
    gc2_set_num(a, "timeout", 30);
    gc2_set_bool(a, "capture_output", true);
    gc2_set_bool(a, "text", true);
    gc2_set_str(a, "encoding", "utf-8");
    gc2_set_str(a, "errors", "replace");
    return gc2_call(s, "subprocess_run", a);
}

/* PoP: gc2_wait_for_target_user_bus @ hermes_cli/gateway.py:_wait_for_target_user_bus */
static json_t *gc2_wait_for_target_user_bus(double uid, double timeout, const gc2_seams_t *s) {
    (void)uid;  /* bus path is constructed live but carries no seam; poll_until abstracts the poll */
    json_t *a = gc2_args0();
    gc2_set_num(a, "timeout", timeout);
    gc2_set_num(a, "interval", 0.2);
    return gc2_call(s, "poll_until", a);
}

/* PoP: gc2_user_systemd_unit_dir @ hermes_cli/gateway.py:user_systemd_unit_dir */
static json_t *gc2_user_systemd_unit_dir(const gc2_seams_t *s) {
    json_t *ea = gc2_args0();
    gc2_set_str(ea, "name", "XDG_CONFIG_HOME");
    gc2_set_str(ea, "default", "");
    json_t *env = gc2_call(s, "environ_get", ea);
    if (gc2_is_raise(env)) return env;
    if (!gc2_jpresent(env)) {
        json_free(env);
        return gc2_raise("AttributeError", "'NoneType' object has no attribute 'strip'");
    }
    char *envs = gc2_py_str(env); json_free(env);
    char *config_home = gc2_strip(envs); free(envs);
    /* LIVE wraps XDG_CONFIG_HOME in Path(); on Windows that normalizes
       backslashes to forward slashes (C:\ -> C:/) before the / joins. */
    {
        char *norm_home = gc2_replace_all(config_home, "\\", "/");
        free(config_home);
        config_home = norm_home;
    }
    char *base;
    if (config_home[0]) { base = strdup(config_home); }
    else {
        json_t *gh = gc2_call(s, "get_real_home", gc2_args0());
        if (gc2_is_raise(gh)) { free(config_home); return gh; }
        char *ghs = gc2_py_str(gh); json_free(gh);
        base = gc2_path_join(ghs, ".config");
        free(ghs);
    }
    free(config_home);
    char *d1 = gc2_path_join(base, "systemd");
    char *d2 = gc2_path_join(d1, "user");
    free(base); free(d1);
    json_t *ret = json_string(d2);
    free(d2);
    return ret;
}

/* PoP: gc2_user_runtime_dir @ hermes_cli/gateway.py:_user_runtime_dir */
static json_t *gc2_user_runtime_dir(const gc2_seams_t *s) {
    json_t *ea = gc2_args0();
    gc2_set_str(ea, "name", "XDG_RUNTIME_DIR");
    json_t *env = gc2_call(s, "environ_get", ea);
    if (gc2_is_raise(env)) return env;
    char *path;
    if (gc2_jtruth(env)) { path = gc2_py_str(env); json_free(env); }
    else {
        json_free(env);
        json_t *uid = gc2_call(s, "getuid", gc2_args0());
        if (gc2_is_raise(uid)) return uid;
        char *uids = gc2_py_str(uid); json_free(uid);
        path = gc2_xsf("/run/user/%s", uids);
        free(uids);
    }
    json_t *ret = json_string(path);
    free(path);
    return ret;
}

/* PoP: gc2_ensure_system_service_linger @ hermes_cli/gateway.py:_ensure_system_service_linger */
static json_t *gc2_ensure_system_service_linger(const char *username, const gc2_seams_t *s) {
    json_t *ea = gc2_args0();
    gc2_set_str(ea, "username", username);
    gc2_set_bool(ea, "system", true);
    json_t *el = gc2_call(s, "ensure_linger_enabled", ea);
    if (gc2_is_raise(el)) return el;
    bool enabled = gc2_jtruth(el);
    json_free(el);
    if (!enabled) return NULL;
    json_t *pa = gc2_args0(); gc2_set_str(pa, "username", username);
    json_t *pw = gc2_call(s, "pwd_getpwnam", pa);
    if (gc2_is_raise(pw)) return pw;
    double uid = json_get_num(pw, "pw_uid", 0);
    json_free(pw);
    char *uids = gc2_py_float_str(uid);
    json_t *bus = gc2_wait_for_target_user_bus(uid, 5.0, s);
    if (gc2_is_raise(bus)) { free(uids); return bus; }
    bool bus_up = gc2_jtruth(bus);
    json_free(bus);
    json_t *r;
    if (bus_up) {
        char *msg = gc2_xsf("\xe2\x9c\x93 /run/user/%s/bus is up \xe2\x80\x94 cron and Kanban workers can use systemd-run --user", uids);
        r = gc2_print(s, msg); free(msg);
        if (gc2_is_raise(r)) { free(uids); return r; }
        json_free(r);
    } else {
        char *m1 = gc2_xsf("\xe2\x9a\xa0 /run/user/%s/bus did not appear within 5s.", uids);
        r = gc2_print(s, m1); free(m1);
        if (gc2_is_raise(r)) { free(uids); return r; }
        json_free(r);
        char *m2 = gc2_xsf("  Start the user manager: sudo systemctl start user@%s.service", uids);
        r = gc2_print(s, m2); free(m2);
        if (gc2_is_raise(r)) { free(uids); return r; }
        json_free(r);
    }
    free(uids);
    json_t *act = gc2_seam_unit_is_active(s, true);
    if (gc2_is_raise(act)) return act;
    bool active = gc2_jtruth(act);
    json_free(act);
    if (active) {
        r = gc2_print(s, "  The running gateway was started without a user D-Bus; restart it to pick one up:");
        if (gc2_is_raise(r)) return r;
        json_free(r);
        json_t *sn = gc2_call(s, "get_service_name", gc2_args0());
        if (gc2_is_raise(sn)) return sn;
        char *msg = gc2_xsf("    sudo systemctl restart %s.service", gc2_jstr(sn) ? gc2_jstr(sn) : "");
        json_free(sn);
        r = gc2_print(s, msg); free(msg);
        if (gc2_is_raise(r)) return r;
        json_free(r);
    }
    return NULL;
}

/* PoP: gc2_refuses_container_user_scope_install @ hermes_cli/gateway.py:refuses_container_user_scope_install */
static json_t *gc2_refuses_container_user_scope_install(bool system, const gc2_seams_t *s) {
    if (system) return json_bool(false);
    json_t *ic = gc2_call(s, "is_container", gc2_args0());
    if (gc2_is_raise(ic)) return ic;
    bool container = gc2_jtruth(ic);
    json_free(ic);
    if (!container) return json_bool(false);
    json_t *r = gc2_print_error(s, "Refusing to install a user-scope systemd gateway service inside a container.");
    if (gc2_is_raise(r)) return r;
    json_free(r);
    static const char *LINES[] = {
        "The unit file and its enable symlink would be written to the home directory, which is",
        "commonly the host's own home bind-mounted in \xe2\x80\x94 the host's user manager then enables and",
        "starts the same unit, so a second gateway polls the same bot token outside the container",
        "(Telegram: 'Conflict: terminated by other getUpdates request').",
        "",
        "  hermes gateway run                                # run as the container's main process",
        "  docker run --restart unless-stopped ...           # container restart policy",
        "",
        "If systemd manages this container (systemd as PID 1), install an isolated system service instead:",
        "  sudo hermes gateway install --system --run-as-user <user>",
    };
    json_t *lines = json_array();
    for (size_t i = 0; i < sizeof(LINES)/sizeof(LINES[0]); i++)
        json_append(lines, json_string(LINES[i]));
    json_t *a = gc2_args0();
    json_set(a, "lines", lines);
    r = gc2_call(s, "print_info_lines", a);
    if (gc2_is_raise(r)) return r;
    json_free(r);
    return json_bool(true);
}

/* PoP: gc2_is_apt_termux_install @ hermes_cli/gateway.py:_is_apt_termux_install */
static json_t *gc2_is_apt_termux_install(const gc2_seams_t *s) {
    json_t *a = gc2_args0();
    gc2_set_str(a, "root", "/proj/root");
    json_t *st = gc2_call(s, "sealed_steward", a);
    if (gc2_is_raise(st)) return st;
    const char *sv = gc2_jstr(st);
    bool match = sv && !strcmp(sv, "apt-termux");
    json_free(st);
    return json_bool(match);
}

/* PoP: gc2_print_service_not_installed @ hermes_cli/gateway.py:_print_service_not_installed */
static json_t *gc2_print_service_not_installed(bool system, const gc2_seams_t *s) {
    json_t *a = gc2_args0(); gc2_set_bool(a, "system", system);
    json_t *bits = gc2_call(s, "systemd_cli_bits", a);
    if (gc2_is_raise(bits)) return bits;
    const char *sudo = gc2_jstr(json_get(bits, 0));
    const char *scope = gc2_jstr(json_get(bits, 1));
    json_t *r = gc2_print(s, "\xe2\x9c\x97 Gateway service is not installed");
    if (gc2_is_raise(r)) { json_free(bits); return r; }
    json_free(r);
    char *msg = gc2_xsf("  Run: %shermes gateway install%s", sudo ? sudo : "", scope ? scope : "");
    json_free(bits);
    r = gc2_print(s, msg); free(msg);
    if (gc2_is_raise(r)) return r;
    json_free(r);
    return NULL;
}

/* PoP: gc2_print_runtime_health @ hermes_cli/gateway.py:_print_runtime_health */
static json_t *gc2_print_runtime_health(const gc2_seams_t *s) {
    json_t *rl = gc2_call(s, "runtime_health_lines", gc2_args0());
    if (gc2_is_raise(rl)) return rl;
    if (gc2_jtruth(rl)) {
        json_t *r = gc2_print(s, NULL);
        if (gc2_is_raise(r)) { json_free(rl); return r; }
        json_free(r);
        r = gc2_print(s, "Recent gateway health:");
        if (gc2_is_raise(r)) { json_free(rl); return r; }
        json_free(r);
        for (size_t i = 0; i < rl->c.count; i++) {
            const char *line = gc2_jstr(rl->c.items[i]);
            char *msg = gc2_xsf("  %s", line ? line : "");
            r = gc2_print(s, msg); free(msg);
            if (gc2_is_raise(r)) { json_free(rl); return r; }
            json_free(r);
        }
    }
    json_free(rl);
    return NULL;
}

/* PoP: gc2_systemd_unit_belongs_to_current_home @ hermes_cli/gateway.py:_systemd_unit_belongs_to_current_home */
static json_t *gc2_systemd_unit_belongs_to_current_home(bool system, const gc2_seams_t *s) {
    json_t *fa = gc2_args0(); gc2_set_bool(fa, "system", system);
    json_t *foreign = gc2_call(s, "foreign_systemd_unit_home", fa);
    if (gc2_is_raise(foreign)) return foreign;
    if (gc2_jpresent(foreign)) {
        json_t *ua = gc2_args0(); gc2_set_bool(ua, "system", system);
        json_t *up = gc2_call(s, "get_systemd_unit_path", ua);
        if (gc2_is_raise(up)) { json_free(foreign); return up; }
        json_t *gh = gc2_call(s, "get_hermes_home", gc2_args0());
        if (gc2_is_raise(gh)) { json_free(foreign); json_free(up); return gh; }
        char *msg = gc2_xsf("Refusing to remove %s: it runs HERMES_HOME=%s, but this process has HERMES_HOME=%s",
                            gc2_jstr(up) ? gc2_jstr(up) : "",
                            gc2_jstr(foreign) ? gc2_jstr(foreign) : "",
                            gc2_jstr(gh) ? gc2_jstr(gh) : "");
        json_free(up); json_free(gh); json_free(foreign);
        json_t *r = gc2_print_warning(s, msg); free(msg);
        if (gc2_is_raise(r)) return r;
        json_free(r);
        return json_bool(false);
    }
    json_free(foreign);
    json_t *sa = gc2_args0(); gc2_set_bool(sa, "system", system);
    json_t *sync = gc2_call(s, "sync_hermes_home_from_systemd_unit", sa);
    if (gc2_is_raise(sync)) return sync;
    json_free(sync);
    json_t *ha = gc2_args0(); gc2_set_bool(ha, "system", system);
    json_t *unit_home = gc2_call(s, "hermes_home_from_systemd_unit_file", ha);
    if (gc2_is_raise(unit_home)) return unit_home;
    if (!gc2_jpresent(unit_home)) { json_free(unit_home); return json_bool(true); }
    json_t *eu = gc2_path_expanduser(s, gc2_jstr(unit_home));
    if (gc2_is_raise(eu)) { json_free(unit_home); return eu; }
    json_t *resolved = gc2_path_resolve(s, gc2_jstr(eu));
    json_free(eu);
    if (gc2_is_raise(resolved)) { json_free(unit_home); return resolved; }
    json_t *gh = gc2_call(s, "get_hermes_home", gc2_args0());
    if (gc2_is_raise(gh)) { json_free(unit_home); json_free(resolved); return gh; }
    json_t *gh_resolved = gc2_path_resolve(s, gc2_jstr(gh));
    json_free(gh);
    if (gc2_is_raise(gh_resolved)) { json_free(unit_home); json_free(resolved); return gh_resolved; }
    bool equal = !strcmp(gc2_jstr(resolved) ? gc2_jstr(resolved) : "",
                         gc2_jstr(gh_resolved) ? gc2_jstr(gh_resolved) : "");
    json_free(resolved); json_free(gh_resolved);
    if (equal) { json_free(unit_home); return json_bool(true); }
    json_t *ua = gc2_args0(); gc2_set_bool(ua, "system", system);
    json_t *up = gc2_call(s, "get_systemd_unit_path", ua);
    if (gc2_is_raise(up)) { json_free(unit_home); return up; }
    json_t *gh2 = gc2_call(s, "get_hermes_home", gc2_args0());
    if (gc2_is_raise(gh2)) { json_free(unit_home); json_free(up); return gh2; }
    char *msg = gc2_xsf("Refusing to remove %s: it runs HERMES_HOME=%s, but this process has HERMES_HOME=%s",
                        gc2_jstr(up) ? gc2_jstr(up) : "",
                        gc2_jstr(unit_home) ? gc2_jstr(unit_home) : "",
                        gc2_jstr(gh2) ? gc2_jstr(gh2) : "");
    json_free(up); json_free(gh2); json_free(unit_home);
    json_t *r = gc2_print_warning(s, msg); free(msg);
    if (gc2_is_raise(r)) return r;
    json_free(r);
    return json_bool(false);
}

/* PoP: gc2_bare_unit_pinned_home @ hermes_cli/gateway.py:_bare_unit_pinned_home */
static json_t *gc2_bare_unit_pinned_home(const gc2_seams_t *s) {
    json_t *il = gc2_call(s, "is_linux", gc2_args0());
    if (gc2_is_raise(il)) return il;
    bool linux = gc2_jtruth(il);
    json_free(il);
    if (!linux) return NULL;
    json_t *eu = gc2_call(s, "geteuid", gc2_args0());
    if (gc2_is_raise(eu)) return eu;
    double euid = gc2_is_num(eu) ? eu->num_val : 0;
    json_free(eu);
    char *unit_dir;
    if (euid == 0) { unit_dir = strdup("/etc/systemd/system"); }
    else {
        json_t *ud = gc2_user_systemd_unit_dir(s);
        if (gc2_is_raise(ud)) return ud;
        unit_dir = strdup(gc2_jstr(ud) ? gc2_jstr(ud) : "");
        json_free(ud);
    }
    char *unit_path = gc2_path_join(unit_dir, "hermes-gateway.service");
    free(unit_dir);
    json_t *pinned = gc2_hermes_home_pinned_by_unit(unit_path, s);
    free(unit_path);
    if (gc2_is_raise(pinned)) return pinned;
    if (!gc2_jtruth(pinned)) { json_free(pinned); return NULL; }
    json_t *ex = gc2_path_expanduser(s, gc2_jstr(pinned));
    if (gc2_is_raise(ex)) {
        const char *cls = gc2_raise_class(ex);
        json_free(pinned);
        if (gc2_cls_runtimeerror(cls) || gc2_cls_valueerror(cls)) { json_free(ex); return NULL; }
        return ex;
    }
    json_t *resolved = gc2_path_resolve(s, gc2_jstr(ex));
    json_free(ex);
    if (gc2_is_raise(resolved)) {
        const char *cls = gc2_raise_class(resolved);
        json_free(pinned);
        if (gc2_cls_runtimeerror(cls) || gc2_cls_valueerror(cls)) { json_free(resolved); return NULL; }
        return resolved;
    }
    json_free(pinned);
    return resolved;
}