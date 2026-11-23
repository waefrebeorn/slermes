/*
 * gateway_migrate_flow2.c — hermes_cli/gateway_migrate.py UNCLAIMED remainder
 * (LIVE authority: C:/Users/eman5/hermes-agent/hermes_cli/gateway_migrate.py).
 *
 * Second-wave FLOW port. gateway_migrate_flow (flow1) owns the manifest/resume
 * leaves, the ownership probe, the preflight, the bring-up ladder, the removal
 * order and the two compensating orchestrators (apply_migration /
 * rollback_migration) plus the dataclass properties. flow2 ports the 26 still
 * UNCLAIMED defs: the home/env + service-manager plumbing, the duplicate-
 * credential preflight cluster, the port-binder check, the plan formatter and
 * the CLI / update-hook entry points.
 *
 * SEAM DOCTRINE (same universal seam as flow1): every live effect — service
 * verbs, config reads/writes, foreign-module calls, print, the claimed flow1
 * siblings and the claimed dataclass properties — enters through ONE seam:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` borrowed JSON object
 * (null-valued keys dropped on BOTH sides); result NEW json_t* (NULL == None,
 * never a JSON_NULL node); {"__raise__": cls, "msg": msg} == a raised
 * exception. The harness records {"ev","args","ret"} per call in order; the
 * oracle's recording stubs emit the identical shape driving LIVE Python.
 *
 * CLAIMED SIBLINGS ARE SEAMED. The flow1-owned functions (_service_label,
 * _remove_verb, _manifest_path, _print, _default_home, _host_gateway_owner,
 * _own_gateway_pid, _installed_services, _read_manifest,
 * _manifest_not_yet_served, _resume_target, _signalled_gateways,
 * apply_migration) and the claimed dataclass properties (plan.secondaries,
 * plan.default, plan.already_multiplexed, plan.standalone_secondaries,
 * plan.blocked, plan.target_service_kind, plan.expected_served_names,
 * pg.service_label, pg.has_gateway) are modelled as seams returning scripted
 * results; they are NOT re-ported here (no PoP re-claim). Unclaimed non-ported
 * collaborators (_installed_services, _profile_gateway_config,
 * platform_serves_profile_prefix, _home_env / _multiplex_read_mode context
 * managers) are likewise seamed.
 *
 * RETURN CONTRACT: every gm2_flow_* returns a NEW json_t* (caller frees):
 * NULL == Python None; a {"__raise__","msg"} object == a raised exception,
 * propagated or caught exactly as the live def does. Tuples are arrays with
 * explicit json_null() elements. BaseException (SystemExit / KeyboardInterrupt
 * / GeneratorExit) passes through every except Exception / suppress(Exception).
 *
 * Plan representation (MigrationPlan FIELDS, fixture shape, key order semantic):
 *   {default_home:str, profiles:[pg], multiplex_flag_on:bool,
 *    live_served:[str]|null, manifest:obj|null, interrupted:bool,
 *    blockers:[str], notices:[str], standalone_by_config:[str]}
 * ProfileGateway (pg):
 *   {name:str, home:str, pid:int|null, services:[[kind,system],...],
 *    run_as_user:str|null, uid:int|null, runtime_home:str|null}
 * A service tuple is a 2-element array [kind(str), system(bool)].
 * Config double (cfg): {platforms:{<platform_value>:pc}}; pc carries at least
 *   {enabled:bool, extra:obj|null} (token/api_key optional). Paths are POSIX
 *   strings; pids/uids/ports are int literals (integral floats read as ints).
 *
 * Fixture domain (kept identical on both sides, documented exclusions):
 * - platform values are canonical lowercase Platform member values; the oracle
 *   stub Enum has no _missing_ hook, so non-member / uppercase values are out
 *   of domain for cfg.platforms KEYS (they raise at materialization).
 * - _credential_claims tuple keys project to "('pv', 'fp')" — pv/fp are simple
 *   ASCII (no quotes/backslashes/control chars) so Python repr == the C key.
 * - pc.extra is null/absent/dict; cfg.platforms present; pc.enabled present.
 * - _set_nested is modelled for the fixed key "gateway.multiplex_profiles" with
 *   gateway absent or a dict (no list-typed gateway, no dotted literal keys,
 *   no phantom siblings).
 * - profiles_to_serve returns a list of [name, home] pairs (or null -> raise).
 * - ports are int or str; hosts are str.
 *
 * Seam name table (name -> live call; args key order is part of the trace):
 *   print                       print(line[, file=sys.stderr])   {line?, file?}
 *   print_lines                 _print(lines)                    {lines}
 *   default_home                _default_home()                  {} -> path
 *   host_gateway_owner          _host_gateway_owner()            {} -> {pid,home}|null
 *   own_gateway_pid             _own_gateway_pid(home, owner)    {home, owner?}
 *   installed_services          _installed_services(home)        {home}
 *   read_manifest               _read_manifest(default_home)     {default_home}
 *   manifest_not_yet_served     _manifest_not_yet_served(m, ls)  {manifest?, live_served?}
 *   resume_target               _resume_target(plan)             {plan}
 *   signalled_gateways          _signalled_gateways(plan)        {plan}
 *   service_label               _service_label(service)          {service}
 *   remove_verb                 _remove_verb(service)            {service}
 *   manifest_path               _manifest_path(default_home)     {default_home}
 *   pg_service_label            pg.service_label()               {name}
 *   pg_has_gateway              pg.has_gateway                   {name}
 *   plan_secondaries            plan.secondaries                 {}
 *   plan_standalone_secondaries plan.standalone_secondaries      {}
 *   plan_default                plan.default                     {}
 *   plan_already_multiplexed    plan.already_multiplexed         {}
 *   plan_blocked                plan.blocked                     {}
 *   plan_target_service_kind    plan.target_service_kind()       {}
 *   plan_expected_served        plan.expected_served_names       {}
 *   apply_migration             apply_migration(plan, served_wait=) {plan, served_wait}
 *   profiles_to_serve           profiles.profiles_to_serve(...)  {multiplex, include_parked?|include_standalone?}
 *   profile_name_for_home       hermes_constants.profile_name_for_home(home) {home}
 *   bring_root_slot_up          gateway_multiplex_s6.bring_root_slot_up() {}
 *   park_named_slot             gateway_multiplex_s6.park_named_slot(name) {name}
 *   running_under_s6            gw._running_under_s6()           {}
 *   named_slot_name             gateway_multiplex_s6.named_slot_name(name) {name}
 *   s6_scandir                  S6ServiceManager().scandir       {}
 *   path_is_dir                 Path.is_dir()                    {path}
 *   home_env_enter / home_env_exit   _home_env(home) CM          {home}
 *   multiplex_read_enter / multiplex_read_exit  _multiplex_read_mode() CM {}
 *   launchd_install             gw.launchd_install()             {}
 *   gww_install                 gateway_windows.install(start_now=, start_on_login=) {start_now, start_on_login}
 *   systemd_install             gw.systemd_install(system=, run_as_user=, non_interactive=) {system, run_as_user?, non_interactive}
 *   get_service_name            gw.get_service_name()            {}
 *   run_systemctl               gw._run_systemctl(args, system=, check=, timeout=) {args, system, check, timeout}
 *   service_call                gw._service_call(kind, verb, system) {kind, verb, system}
 *   profiles_stop_gateway_process  profiles._stop_gateway_process(home) {home}
 *   gw_spawn_detached           gw._spawn_detached_gateway()     {}
 *   explicit_multiplex_flag     gateway_multiplex_mode.explicit_multiplex_flag(home) {default_home}
 *   require_readable_config     config.require_readable_config_before_write(path) {path}
 *   set_nested                  config._set_nested(cfg, key, value) {key, value}
 *   write_user_config           config._write_user_config(path, cfg) {path, config}
 *   is_task_registered          gateway_windows.is_task_registered() {}
 *   is_startup_entry_installed  gateway_windows.is_startup_entry_installed() {}
 *   get_systemd_unit_path       gw.get_systemd_unit_path(system=) {system}
 *   read_systemd_user_from_unit gw._read_systemd_user_from_unit(path) {path}
 *   gateway_identity            gateway_migrate_guards.gateway_identity(home, pid, services) {home, pid?, services}
 *   recorded_served_profiles    gateway_multiplex_served.recorded_served_profiles(home) {default_home}
 *   auto_migration_blockers     gateway_migrate_guards.auto_migration_blockers(plan) {plan}
 *   auto_migration_opted_out    gateway_migrate_guards.auto_migration_opted_out(home) {default_home}
 *   masked_default_unit         gateway_migrate_guards.masked_default_unit(home) {default_home}
 *   profile_gateway_config      _profile_gateway_config(home)    {home}
 *   credential_env_keys         profile_channels.credential_env_keys() {}
 *   adapter_credential_fingerprint  GatewayRunner._adapter_credential_fingerprint(probe) {probe}
 *   platform_binds_port         gateway.config.platform_binds_port(pv, extra) {platform, extra?}
 *   serves_profile_prefix       platform_serves_profile_prefix(pv) {platform_value}
 *   format_url_host             url_utils.format_url_host(host)  {host}
 *   cmd_backends_report         gateway_migrate_backends.cmd_backends_report(args) {args}
 *   stdin_isatty                sys.stdin.isatty()               {}
 *   prompt_yes_no               setup.prompt_yes_no(question, default) {question, default}
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

#define GM2_MANIFEST_NAME "gateway_migration.json"
#define GM2_MIGRATE_COMMAND "hermes gateway migrate --multiplex"
#define GM2_SERVED_WAIT 90.0

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

static char *gm2_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: return gm2_py_float_str(v->num_val);
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        case JSON_ARRAY: {
            char *out = strdup("[");
            for (size_t i = 0; i < v->c.count; i++) {
                char *item = gm2_py_str(v->c.items[i]);
                char *next = gm2_xsf("%s%s%s", out, i ? ", " : "", item);
                free(out); free(item); out = next;
            }
            char *next = gm2_xsf("%s]", out); free(out); return next;
        }
        case JSON_OBJECT: {
            char *out = strdup("{");
            for (size_t i = 0; i < v->c.count; i++) {
                char *item = gm2_py_str(v->c.items[i]);
                char *next = gm2_xsf("%s%s'%s': %s", out, i ? ", " : "", v->c.keys[i], item);
                free(out); free(item); out = next;
            }
            char *next = gm2_xsf("%s}", out); free(out); return next;
        }
    }
    return strdup("");
}

/* ── path helpers (Path.as_posix semantics) ── */

static char *gm2_path_norm(const char *s) {
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
static char *gm2_path_join(const char *base, const char *name) {
    char *b = gm2_path_norm(base);
    size_t bl = strlen(b);
    char *out = gm2_xsf("%s%s%s", b, (bl > 0 && b[bl - 1] == '/') ? "" : "/", name);
    free(b);
    return out;
}
static bool gm2_path_eq(const char *a, const char *b) {
    char *na = gm2_path_norm(a), *nb = gm2_path_norm(b);
    bool eq = strcasecmp(na, nb) == 0;
    free(na); free(nb);
    return eq;
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
static json_t *gm2_raise_attr(const json_t *v, const char *attr) {
    char *m = gm2_xsf("'%s' object has no attribute '%s'", gm2_py_type_name(v), attr);
    json_t *r = gm2_raise_obj("AttributeError", m);
    free(m);
    return r;
}
#define GM2_PROP(r) do { if (gm2_is_raise(r)) return (r); } while (0)

/* ── FULL builtin exception hierarchy (child -> parent) ── */

typedef struct { const char *child, *parent; } gm2_exc_rel_t;
static const gm2_exc_rel_t GM2_EXC_PARENTS[] = {
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
    {"JSONDecodeError", "ValueError"},
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
/* except Exception / suppress(Exception): true for every Exception descendant.
 * The BaseException trio (and BaseException itself) pass through. An unknown
 * kind is a custom Exception subclass (the oracle's _exc_class fallback). */
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
        if (!p) return true;  /* unknown -> custom Exception subclass */
        k = p;
    }
    return true;
}
/* contextlib.suppress(Exception): swallow Exception raises, keep BaseException.
 * Returns NULL when swallowed (the caller then proceeds with the fallback). */
static json_t *gm2_suppress_exception(json_t *r) {
    if (gm2_is_raise(r) && gm2_is_exception(gm2_raise_class(r))) {
        json_free(r);
        return NULL;
    }
    return r;
}

/* ── args builders (null-valued keys dropped on BOTH sides) ── */

static json_t *gm2_args0(void) { return json_object(); }
static void gm2_aset_j(json_t *a, const char *k, const json_t *v) {
    if (gm2_jpresent(v)) json_set(a, k, json_copy(v));
}
static void gm2_aset_s(json_t *a, const char *k, const char *v) {
    if (v) json_set(a, k, json_string(v));
}
static void gm2_aset_b(json_t *a, const char *k, bool v) { json_set(a, k, json_bool(v)); }
static void gm2_aset_n(json_t *a, const char *k, double v) { json_set(a, k, json_number(v)); }

/* ── string-array helpers ── */

static char *gm2_join_strs(const json_t *arr, const char *sep) {
    char *out = strdup("");
    if (gm2_is_arr(arr)) {
        for (size_t i = 0; i < arr->c.count; i++) {
            const char *e = gm2_jstr(arr->c.items[i]);
            if (!e) e = "";
            char *next = gm2_xsf("%s%s%s", out, i ? sep : "", e);
            free(out); out = next;
        }
    }
    return out;
}
static int gm2_cmp_cstr(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
static void gm2_arr_append_str(json_t *arr, const char *s) {
    json_append(arr, json_string(s));
}
static json_t *gm2_plan_profiles(const json_t *plan) {
    json_t *p = json_obj_get(plan, "profiles");
    return gm2_is_arr(p) ? p : NULL;
}
static json_t *gm2_plan_blockers(json_t *plan) {
    json_t *b = json_obj_get(plan, "blockers");
    if (!gm2_is_arr(b)) { b = json_array(); json_set(plan, "blockers", b); }
    return b;
}
static json_t *gm2_plan_notices(json_t *plan) {
    json_t *n = json_obj_get(plan, "notices");
    if (!gm2_is_arr(n)) { n = json_array(); json_set(plan, "notices", n); }
    return n;
}

/* ── claimed dataclass property / method seams ── */

static json_t *gm2_pg_service_label(const json_t *pg, const gm2_seams_t *s) {
    json_t *a = gm2_args0();
    gm2_aset_s(a, "name", gm2_jstr(json_obj_get(pg, "name")));
    return gm2_scall(s, "pg_service_label", a);
}
static json_t *gm2_pg_has_gateway(const json_t *pg, const gm2_seams_t *s) {
    json_t *a = gm2_args0();
    gm2_aset_s(a, "name", gm2_jstr(json_obj_get(pg, "name")));
    return gm2_scall(s, "pg_has_gateway", a);
}
static json_t *gm2_plan_prop(const gm2_seams_t *s, const char *name) {
    return gm2_scall(s, name, gm2_args0());
}

/* ── Platform member table (gateway.config.Platform builtin values) ── */

static bool gm2_platform_valid(const char *pv) {
    static const char *members[] = {
        "local", "telegram", "discord", "whatsapp", "whatsapp_cloud", "slack",
        "signal", "mattermost", "matrix", "email", "sms", "dingtalk",
        "api_server", "webhook", "msgraph_webhook", "feishu", "wecom",
        "wecom_callback", "weixin", "bluebubbles", "qqbot", "relay",
    };
    if (!pv) return false;
    for (size_t i = 0; i < sizeof(members) / sizeof(members[0]); i++)
        if (!strcmp(pv, members[i])) return true;
    return false;
}

/* ── home_env / multiplex_read_mode context-manager seams ── */

static json_t *gm2_home_env_enter(const gm2_seams_t *s, const char *home) {
    json_t *a = gm2_args0(); gm2_aset_s(a, "home", home);
    return gm2_scall(s, "home_env_enter", a);
}
static json_t *gm2_home_env_exit(const gm2_seams_t *s, const char *home) {
    json_t *a = gm2_args0(); gm2_aset_s(a, "home", home);
    return gm2_scall(s, "home_env_exit", a);
}
/* Run the CM exit after a body result; an exit raise overrides the body. */
static json_t *gm2_cm_finish(const gm2_seams_t *s, const char *exit_name,
                             const char *home, json_t *body) {
    json_t *a = gm2_args0();
    if (home) gm2_aset_s(a, "home", home);
    json_t *ex = gm2_scall(s, exit_name, a);
    if (gm2_is_raise(ex)) { json_free(body); return ex; }
    json_free(ex);
    return body;
}

/* ════════════════════════ ported functions ════════════════════════ */

/* PoP: gm2_flow_profile_homes @ hermes_cli/gateway_migrate.py:_profile_homes */
json_t *gm2_flow_profile_homes(const gm2_seams_t *s) {
    json_t *a = gm2_args0();
    gm2_aset_b(a, "multiplex", true);
    gm2_aset_b(a, "include_parked", true);
    gm2_aset_b(a, "include_standalone", false);
    json_t *r = gm2_scall(s, "profiles_to_serve", a);
    GM2_PROP(r);
    if (!gm2_is_arr(r)) {
        char *m = gm2_xsf("'%s' object is not iterable", gm2_py_type_name(r));
        json_t *e = gm2_raise_obj("TypeError", m);
        free(m); json_free(r);
        return e;
    }
    json_t *out = json_array();
    for (size_t i = 0; i < r->c.count; i++) {
        const json_t *pair = r->c.items[i];
        const json_t *nm = gm2_is_arr(pair) ? json_get(pair, 0) : NULL;
        const json_t *hm = gm2_is_arr(pair) ? json_get(pair, 1) : NULL;
        if (!gm2_is_str(hm)) {
            json_free(out); json_free(r);
            return gm2_raise_obj("TypeError",
                "expected str, bytes or os.PathLike object, not NoneType");
        }
        char *home = gm2_path_norm(gm2_jstr(hm));
        json_t *entry = json_array();
        json_append(entry, json_copy(nm ? nm : json_null()));
        json_append(entry, json_string(home));
        free(home);
        json_append(out, entry);
    }
    json_free(r);
    return out;
}

/* PoP: gm2_flow_gateway_identity @ hermes_cli/gateway_migrate.py:_gateway_identity */
json_t *gm2_flow_gateway_identity(const char *home, const json_t *pid,
                                  const json_t *services, const gm2_seams_t *s) {
    json_t *a = gm2_args0();
    gm2_aset_s(a, "home", home);
    gm2_aset_j(a, "pid", pid);
    gm2_aset_j(a, "services", services);
    return gm2_scall(s, "gateway_identity", a);
}

/* PoP: gm2_flow_windows_task_installed @ hermes_cli/gateway_migrate.py:_windows_task_installed */
json_t *gm2_flow_windows_task_installed(const gm2_seams_t *s) {
    json_t *r1 = gm2_scall(s, "is_task_registered", gm2_args0());
    if (gm2_is_raise(r1)) {
        if (gm2_is_exception(gm2_raise_class(r1))) { json_free(r1); return json_bool(false); }
        return r1;
    }
    if (gm2_jtruth(r1)) { json_free(r1); return json_bool(true); }
    json_free(r1);
    json_t *r2 = gm2_scall(s, "is_startup_entry_installed", gm2_args0());
    if (gm2_is_raise(r2)) {
        if (gm2_is_exception(gm2_raise_class(r2))) { json_free(r2); return json_bool(false); }
        return r2;
    }
    json_t *ret = json_bool(gm2_jtruth(r2));
    json_free(r2);
    return ret;
}

/* PoP: gm2_flow_systemd_service_user @ hermes_cli/gateway_migrate.py:_systemd_service_user */
json_t *gm2_flow_systemd_service_user(const char *home, const json_t *services,
                                      const gm2_seams_t *s) {
    json_t *target = json_array();
    json_append(target, json_string("systemd"));
    json_append(target, json_bool(true));
    bool found = false;
    if (gm2_is_arr(services)) {
        for (size_t i = 0; i < services->c.count; i++)
            if (gm2_py_eq(services->c.items[i], target)) { found = true; break; }
    }
    json_free(target);
    if (!found) return NULL;
    json_t *en = gm2_home_env_enter(s, home);
    GM2_PROP(en);
    json_t *a = gm2_args0(); gm2_aset_b(a, "system", true);
    json_t *path = gm2_scall(s, "get_systemd_unit_path", a);
    json_t *r;
    if (gm2_is_raise(path)) { r = path; }
    else {
        json_t *a2 = gm2_args0(); gm2_aset_s(a2, "path", gm2_jstr(path));
        r = gm2_scall(s, "read_systemd_user_from_unit", a2);
        json_free(path);
    }
    return gm2_cm_finish(s, "home_env_exit", home, r);
}

/* PoP: gm2_flow_s6_slot_op @ hermes_cli/gateway_migrate.py:_s6_slot_op */
json_t *gm2_flow_s6_slot_op(const char *verb, const char *home, const gm2_seams_t *s) {
    json_t *a = gm2_args0(); gm2_aset_s(a, "home", home);
    json_t *pn = gm2_scall(s, "profile_name_for_home", a);
    GM2_PROP(pn);
    const char *name = gm2_jtruth(pn) ? gm2_jstr(pn) : "default";
    if (!name) name = "default";
    if (!strcmp(name, "default")) {
        if (!strcmp(verb, "start") || !strcmp(verb, "restart")) {
            json_t *r = gm2_scall(s, "bring_root_slot_up", gm2_args0());
            GM2_PROP(r);
            json_free(r);
        }
        json_free(pn);
        return NULL;
    }
    if (!strcmp(verb, "stop")) {
        json_t *a2 = gm2_args0(); gm2_aset_s(a2, "name", name);
        json_t *r = gm2_scall(s, "park_named_slot", a2);
        json_free(pn);
        GM2_PROP(r);
        json_free(r);
        return NULL;
    }
    json_free(pn);
    return NULL;
}

/* PoP: gm2_flow_service_op @ hermes_cli/gateway_migrate.py:_service_op */
json_t *gm2_flow_service_op(const char *kind, bool system, const char *verb,
                            const char *home, const char *run_as_user,
                            const gm2_seams_t *s) {
    if (!strcmp(kind, "s6"))
        return gm2_flow_s6_slot_op(verb, home, s);
    json_t *en = gm2_home_env_enter(s, home);
    GM2_PROP(en);
    json_t *r = NULL;
    if (!strcmp(verb, "install")) {
        if (!strcmp(kind, "launchd")) {
            r = gm2_scall(s, "launchd_install", gm2_args0());
        } else if (!strcmp(kind, "windows")) {
            json_t *a = gm2_args0();
            gm2_aset_b(a, "start_now", true);
            gm2_aset_b(a, "start_on_login", true);
            r = gm2_scall(s, "gww_install", a);
        } else {
            json_t *a = gm2_args0();
            gm2_aset_b(a, "system", system);
            gm2_aset_s(a, "run_as_user", run_as_user);
            gm2_aset_b(a, "non_interactive", true);
            r = gm2_scall(s, "systemd_install", a);
        }
        if (!gm2_is_raise(r)) { json_free(r); r = NULL; }
    } else if (!strcmp(verb, "enable")) {
        if (strcmp(kind, "systemd") != 0) {
            r = NULL;
        } else {
            json_t *sn = gm2_scall(s, "get_service_name", gm2_args0());
            if (gm2_is_raise(sn)) { r = sn; }
            else {
                json_t *args_arr = json_array();
                gm2_arr_append_str(args_arr, "enable");
                gm2_arr_append_str(args_arr, gm2_jstr(sn) ? gm2_jstr(sn) : "");
                json_t *a = gm2_args0();
                gm2_aset_j(a, "args", args_arr);
                gm2_aset_b(a, "system", system);
                gm2_aset_b(a, "check", false);
                gm2_aset_n(a, "timeout", 30);
                json_free(args_arr); json_free(sn);
                r = gm2_scall(s, "run_systemctl", a);
                if (!gm2_is_raise(r)) {
                    double rc = json_get_num(r, "returncode", 0);
                    if (rc != 0) {
                        json_t *sn2 = gm2_scall(s, "get_service_name", gm2_args0());
                        if (gm2_is_raise(sn2)) { json_free(r); r = sn2; }
                        else {
                            char *rcs = gm2_py_float_str(rc);
                            char *m = gm2_xsf("could not enable %s at boot (systemctl enable "
                                              "exited %s); a reboot would come up with no gateway",
                                              gm2_jstr(sn2) ? gm2_jstr(sn2) : "", rcs);
                            free(rcs); json_free(sn2); json_free(r);
                            r = gm2_raise_obj("RuntimeError", m);
                            free(m);
                        }
                    } else { json_free(r); r = NULL; }
                }
            }
        }
    } else {
        json_t *a = gm2_args0();
        gm2_aset_s(a, "kind", kind);
        gm2_aset_s(a, "verb", verb);
        gm2_aset_b(a, "system", system);
        r = gm2_scall(s, "service_call", a);
        if (!gm2_is_raise(r)) { json_free(r); r = NULL; }
    }
    return gm2_cm_finish(s, "home_env_exit", home, r);
}

/* PoP: gm2_flow_stop_gateway_process @ hermes_cli/gateway_migrate.py:_stop_gateway_process */
json_t *gm2_flow_stop_gateway_process(const char *home, const gm2_seams_t *s) {
    json_t *a = gm2_args0(); gm2_aset_s(a, "home", home);
    json_t *r = gm2_scall(s, "profiles_stop_gateway_process", a);
    GM2_PROP(r);
    json_free(r);
    return NULL;
}

/* PoP: gm2_flow_spawn_detached_gateway @ hermes_cli/gateway_migrate.py:_spawn_detached_gateway */
json_t *gm2_flow_spawn_detached_gateway(const char *home, const gm2_seams_t *s) {
    json_t *en = gm2_home_env_enter(s, home);
    GM2_PROP(en);
    json_t *r = gm2_scall(s, "gw_spawn_detached", gm2_args0());
    return gm2_cm_finish(s, "home_env_exit", home, r);
}

/* PoP: gm2_flow_read_multiplex_flag @ hermes_cli/gateway_migrate.py:_read_multiplex_flag */
json_t *gm2_flow_read_multiplex_flag(const char *default_home, const gm2_seams_t *s) {
    json_t *a = gm2_args0();
    /* LIVE wraps default_home in Path(...) and forwards its .as_posix(); the
     * oracle normalizes with PathDouble (backslash->'/', collapse '//', rstrip
     * trailing '/' except root) before recording the seam argument. */
    char *nh = gm2_path_norm(default_home);
    gm2_aset_s(a, "default_home", nh);
    free(nh);
    json_t *r = gm2_scall(s, "explicit_multiplex_flag", a);
    GM2_PROP(r);
    json_t *ret = json_bool(gm2_is_bool(r) && r->bool_val);
    json_free(r);
    return ret;
}

/* PoP: gm2_flow_write_multiplex_flag @ hermes_cli/gateway_migrate.py:_write_multiplex_flag */
json_t *gm2_flow_write_multiplex_flag(const char *default_home, bool value,
                                      const gm2_seams_t *s) {
    char *path = gm2_path_join(default_home, "config.yaml");
    json_t *a = gm2_args0(); gm2_aset_s(a, "path", path);
    json_t *uc = gm2_scall(s, "require_readable_config", a);
    if (gm2_is_raise(uc)) { free(path); return uc; }
    if (!gm2_is_obj(uc)) {
        json_t *e = gm2_raise_attr(uc, "pop");
        json_free(uc); free(path);
        return e;
    }
    json_obj_del(uc, "multiplex_profiles");
    json_t *a2 = gm2_args0();
    gm2_aset_s(a2, "key", "gateway.multiplex_profiles");
    gm2_aset_b(a2, "value", value);
    json_t *sn = gm2_scall(s, "set_nested", a2);
    if (gm2_is_raise(sn)) { json_free(uc); free(path); return sn; }
    json_free(sn);
    /* _set_nested contract for the fixed 2-segment key (domain: gateway absent
     * or a dict): create/overwrite the gateway mapping, set the leaf. */
    json_t *gw = json_obj_get(uc, "gateway");
    if (!gm2_is_obj(gw)) { gw = json_object(); json_set(uc, "gateway", gw); }
    json_set(gw, "multiplex_profiles", json_bool(value));
    json_t *a3 = gm2_args0();
    gm2_aset_s(a3, "path", path);
    gm2_aset_j(a3, "config", uc);
    json_t *wu = gm2_scall(s, "write_user_config", a3);
    json_free(uc); free(path);
    GM2_PROP(wu);
    json_free(wu);
    return NULL;
}

/* PoP: gm2_flow_credential_probe @ hermes_cli/gateway_migrate.py:_credential_probe */
json_t *gm2_flow_credential_probe(const json_t *pc) {
    const json_t *extra = json_obj_get(pc, "extra");
    if (gm2_jtruth(extra) && !gm2_is_obj(extra))
        return gm2_raise_attr(extra, "get");
    const json_t *tok = json_obj_get(pc, "token");
    const json_t *token = gm2_jtruth(tok) ? tok : json_obj_get(pc, "api_key");
    json_t *out = json_object();
    json_set(out, "token", token ? json_copy(token) : json_null());
    const char *ids[] = {"app_id", "client_id", "bot_id", "project_secret"};
    const char *keys[] = {"_app_id", "_client_id", "_bot_id", "_project_secret"};
    for (int i = 0; i < 4; i++) {
        const json_t *v = gm2_is_obj(extra) ? json_obj_get(extra, ids[i]) : NULL;
        json_set(out, keys[i], v ? json_copy(v) : json_null());
    }
    json_set(out, "config", pc ? json_copy(pc) : json_null());
    return out;
}

/* PoP: gm2_flow_credential_claims @ hermes_cli/gateway_migrate.py:_credential_claims */
json_t *gm2_flow_credential_claims(const json_t *config, const gm2_seams_t *s) {
    const json_t *platforms = json_obj_get(config, "platforms");
    if (!gm2_is_obj(platforms))
        return gm2_raise_attr(config, "platforms");
    json_t *claims = json_object();
    for (size_t i = 0; i < platforms->c.count; i++) {
        const char *pv = platforms->c.keys[i];
        const json_t *pc = platforms->c.items[i];
        const json_t *enabled = json_obj_get(pc, "enabled");
        if (!gm2_jtruth(enabled)) continue;
        json_t *probe = gm2_flow_credential_probe(pc);
        GM2_PROP(probe);
        json_t *a = gm2_args0();
        gm2_aset_j(a, "probe", probe);
        json_free(probe);
        json_t *fp = gm2_scall(s, "adapter_credential_fingerprint", a);
        GM2_PROP(fp);
        if (fp) {  /* fp is not None */
            const char *fps = gm2_jstr(fp);
            char *key = gm2_xsf("('%s', '%s')", pv, fps ? fps : "");
            json_set(claims, key, json_string(pv));
            free(key);
        }
        json_free(fp);
    }
    return claims;
}

/* PoP: gm2_flow_credential_key_names @ hermes_cli/gateway_migrate.py:_credential_key_names */
json_t *gm2_flow_credential_key_names(const char *platform_value, const gm2_seams_t *s) {
    json_t *r = gm2_scall(s, "credential_env_keys", gm2_args0());
    GM2_PROP(r);
    char **names = NULL;
    size_t n = 0;
    if (gm2_is_obj(r)) {
        for (size_t i = 0; i < r->c.count; i++) {
            const char *pid = gm2_jstr(r->c.items[i]);
            if (pid && !strcmp(pid, platform_value)) {
                names = realloc(names, (n + 1) * sizeof(char *));
                names[n++] = strdup(r->c.keys[i]);
            }
        }
    }
    json_free(r);
    if (n > 1) qsort(names, n, sizeof(char *), gm2_cmp_cstr);
    char *joined = strdup("");
    for (size_t i = 0; i < n; i++) {
        char *next = gm2_xsf("%s%s%s", joined, i ? "/" : "", names[i]);
        free(joined); joined = next;
        free(names[i]);
    }
    free(names);
    json_t *ret;
    if (joined[0] == '\0') {
        char *m = gm2_xsf("the %s token", platform_value);
        ret = json_string(m);
        free(m);
    } else {
        ret = json_string(joined);
    }
    free(joined);
    return ret;
}

/* PoP: gm2_flow_duplicate_credential_lines @ hermes_cli/gateway_migrate.py:duplicate_credential_lines */
json_t *gm2_flow_duplicate_credential_lines(const json_t *configs, const gm2_seams_t *s) {
    json_t *owners = json_object();
    json_t *lines = json_array();
    if (gm2_is_arr(configs)) {
        for (size_t i = 0; i < configs->c.count; i++) {
            const json_t *pair = configs->c.items[i];
            if (!gm2_is_arr(pair) || pair->c.count < 2) continue;
            const char *name = gm2_jstr(json_get(pair, 0));
            const json_t *cfg = json_get(pair, 1);
            json_t *claims = gm2_flow_credential_claims(cfg, s);
            if (gm2_is_raise(claims)) { json_free(owners); json_free(lines); return claims; }
            for (size_t j = 0; j < claims->c.count; j++) {
                const char *claim = claims->c.keys[j];
                const char *pv = gm2_jstr(claims->c.items[j]);
                const json_t *existing = json_obj_get(owners, claim);
                const char *owner;
                if (existing) { owner = gm2_jstr(existing); }
                else { json_set(owners, claim, json_string(name ? name : "")); owner = name; }
                if (owner && name && !strcmp(owner, name)) continue;
                json_t *key = gm2_flow_credential_key_names(pv ? pv : "", s);
                if (gm2_is_raise(key)) { json_free(claims); json_free(owners); json_free(lines); return key; }
                const char *ks = gm2_jstr(key);
                char *line = gm2_xsf(
                    "Profiles '%s' and '%s' both hold the same %s credential (%s): "
                    "one platform token can serve only one gateway, so the bot answers from whichever profile "
                    "claims it first and the other's adapter is parked. Give '%s' its own bot token, or remove "
                    "%s from the profile that should not own it (or keep it in %s and route %s's "
                    "chats with profile_routes — gateway.profile_routes in %s's config.yaml), then run "
                    "%s.",
                    owner ? owner : "", name ? name : "", pv ? pv : "", ks ? ks : "",
                    name ? name : "", ks ? ks : "", owner ? owner : "", name ? name : "",
                    owner ? owner : "", GM2_MIGRATE_COMMAND);
                gm2_arr_append_str(lines, line);
                free(line); json_free(key);
            }
            json_free(claims);
        }
    }
    json_free(owners);
    return lines;
}

/* PoP: gm2_flow_duplicate_credential_findings @ hermes_cli/gateway_migrate.py:duplicate_credential_findings */
json_t *gm2_flow_duplicate_credential_findings(const gm2_seams_t *s) {
    json_t *en = gm2_scall(s, "multiplex_read_enter", gm2_args0());
    GM2_PROP(en);
    json_free(en);
    json_t *homes = gm2_flow_profile_homes(s);
    json_t *configs = json_array();
    json_t *err = NULL;
    if (gm2_is_raise(homes)) { err = homes; }
    else {
        for (size_t i = 0; i < homes->c.count; i++) {
            const json_t *pair = homes->c.items[i];
            const char *name = gm2_jstr(json_get(pair, 0));
            const char *home = gm2_jstr(json_get(pair, 1));
            json_t *a = gm2_args0(); gm2_aset_s(a, "home", home);
            json_t *cfg = gm2_scall(s, "profile_gateway_config", a);
            if (gm2_is_raise(cfg)) {
                if (gm2_is_exception(gm2_raise_class(cfg))) { json_free(cfg); continue; }
                err = cfg; break;
            }
            json_t *entry = json_array();
            json_append(entry, json_string(name ? name : ""));
            json_append(entry, cfg);
            json_append(configs, entry);
        }
        json_free(homes);
    }
    /* the with block ends here: __exit__ fires BEFORE duplicate_credential_lines
     * (the live `return duplicate_credential_lines(configs)` is OUTSIDE the with). */
    json_t *ex = gm2_scall(s, "multiplex_read_exit", gm2_args0());
    if (gm2_is_raise(ex)) { json_free(configs); json_free(err); return ex; }
    json_free(ex);
    if (err) { json_free(configs); return err; }
    json_t *result = gm2_flow_duplicate_credential_lines(configs, s);
    json_free(configs);
    return result;
}

/* PoP: gm2_flow_check_duplicate_credentials @ hermes_cli/gateway_migrate.py:_check_duplicate_credentials */
json_t *gm2_flow_check_duplicate_credentials(json_t *plan, const json_t *configs,
                                             const gm2_seams_t *s) {
    json_t *pairs = json_array();
    json_t *profiles = gm2_plan_profiles(plan);
    if (profiles) {
        for (size_t i = 0; i < profiles->c.count; i++) {
            const json_t *pg = profiles->c.items[i];
            const char *name = gm2_jstr(json_obj_get(pg, "name"));
            if (name && json_has(configs, name)) {
                json_t *entry = json_array();
                json_append(entry, json_string(name));
                json_append(entry, json_copy(json_obj_get(configs, name)));
                json_append(pairs, entry);
            }
        }
    }
    json_t *lines = gm2_flow_duplicate_credential_lines(pairs, s);
    json_free(pairs);
    GM2_PROP(lines);
    json_t *blockers = gm2_plan_blockers(plan);
    for (size_t i = 0; i < lines->c.count; i++)
        json_append(blockers, json_copy(lines->c.items[i]));
    json_free(lines);
    return NULL;
}

/* _listener_url's extra computation, inside suppress(Exception). Returns the
 * extra dict (NEW) or a raise spec; the caller swallows Exception raises. */
static json_t *gm2_listener_extra(const json_t *default_cfg, const char *pv) {
    if (!gm2_jpresent(default_cfg))
        return gm2_raise_attr(default_cfg, "platforms");
    const json_t *platforms = json_obj_get(default_cfg, "platforms");
    if (!gm2_is_obj(platforms))
        return gm2_raise_attr(default_cfg, "platforms");
    if (!gm2_platform_valid(pv)) {
        char *m = gm2_xsf("'%s' is not a valid Platform", pv);
        json_t *e = gm2_raise_obj("ValueError", m);
        free(m);
        return e;
    }
    const json_t *pc = json_obj_get(platforms, pv);
    const json_t *ex;
    if (gm2_jtruth(pc)) {
        if (!gm2_is_obj(pc)) return gm2_raise_attr(pc, "extra");
        ex = json_obj_get(pc, "extra");
    } else {
        ex = NULL;  /* SimpleNamespace(extra={}).extra -> {} */
    }
    if (gm2_jtruth(ex)) {
        if (!gm2_is_obj(ex)) return gm2_raise_attr(ex, "get");
        return json_copy(ex);
    }
    return json_object();
}

/* PoP: gm2_flow_listener_url @ hermes_cli/gateway_migrate.py:_listener_url */
json_t *gm2_flow_listener_url(const json_t *default_cfg, const char *platform_value,
                              const char *profile, const gm2_seams_t *s) {
    json_t *extra = gm2_listener_extra(default_cfg, platform_value);
    if (gm2_is_raise(extra)) {
        if (gm2_is_exception(gm2_raise_class(extra))) { json_free(extra); extra = json_object(); }
        else return extra;
    }
    const char *host0 = "<host>"; const char *port0 = "<port>"; const char *tail = "/...";
    if (!strcmp(platform_value, "api_server")) { host0 = "127.0.0.1"; port0 = "8642"; tail = "/v1/..."; }
    else if (!strcmp(platform_value, "webhook")) { host0 = "0.0.0.0"; port0 = "8644"; tail = "/webhooks/<route>"; }
    const json_t *hv = json_obj_get(extra, "host");
    const json_t *host = gm2_jtruth(hv) ? hv : NULL;
    const json_t *pv = json_obj_get(extra, "port");
    const json_t *port = gm2_jtruth(pv) ? pv : NULL;
    json_t *a = gm2_args0();
    if (host) gm2_aset_j(a, "host", host); else gm2_aset_s(a, "host", host0);
    json_t *fh = gm2_scall(s, "format_url_host", a);
    if (gm2_is_raise(fh)) { json_free(extra); return fh; }
    char *ports = port ? gm2_py_str(port) : strdup(port0);
    char *url = gm2_xsf("http://%s:%s/p/%s%s",
                        gm2_jstr(fh) ? gm2_jstr(fh) : "", ports, profile, tail);
    free(ports); json_free(fh); json_free(extra);
    json_t *ret = json_string(url);
    free(url);
    return ret;
}

/* PoP: gm2_flow_check_secondary_port_binders @ hermes_cli/gateway_migrate.py:_check_secondary_port_binders */
json_t *gm2_flow_check_secondary_port_binders(json_t *plan, const json_t *configs,
                                              const gm2_seams_t *s) {
    const json_t *default_cfg = json_obj_get(configs, "default");
    json_t *secs = gm2_plan_prop(s, "plan_secondaries");
    GM2_PROP(secs);
    json_t *err = NULL;
    if (gm2_is_arr(secs)) {
        for (size_t i = 0; i < secs->c.count && !err; i++) {
            const json_t *pg = secs->c.items[i];
            const char *name = gm2_jstr(json_obj_get(pg, "name"));
            const json_t *cfg = json_obj_get(configs, name ? name : "");
            if (!gm2_jpresent(cfg)) continue;
            const json_t *platforms = json_obj_get(cfg, "platforms");
            if (!gm2_is_obj(platforms)) { err = gm2_raise_attr(cfg, "platforms"); break; }
            for (size_t j = 0; j < platforms->c.count && !err; j++) {
                const char *pv = platforms->c.keys[j];
                const json_t *pc = platforms->c.items[j];
                const json_t *enabled = json_obj_get(pc, "enabled");
                if (!gm2_jtruth(enabled)) continue;
                const json_t *extra = json_obj_get(pc, "extra");
                if (!extra) { err = gm2_raise_attr(pc, "extra"); break; }
                json_t *a = gm2_args0();
                gm2_aset_s(a, "platform", pv);
                gm2_aset_j(a, "extra", extra);
                json_t *binds = gm2_scall(s, "platform_binds_port", a);
                if (gm2_is_raise(binds)) { err = binds; break; }
                bool binds_b = gm2_jtruth(binds);
                json_free(binds);
                if (!binds_b) continue;
                json_t *a2 = gm2_args0(); gm2_aset_s(a2, "platform_value", pv);
                json_t *serves = gm2_scall(s, "serves_profile_prefix", a2);
                if (gm2_is_raise(serves)) { err = serves; break; }
                bool serves_b = gm2_jtruth(serves);
                json_free(serves);
                if (serves_b) {
                    json_t *url = gm2_flow_listener_url(default_cfg, pv, name ? name : "", s);
                    if (gm2_is_raise(url)) { err = url; break; }
                    char *line = gm2_xsf(
                        "Profile '%s': %s moves onto the default listener at %s (its key/secret is "
                        "unchanged; update clients that call the old per-profile port).",
                        name ? name : "", pv, gm2_jstr(url) ? gm2_jstr(url) : "");
                    gm2_arr_append_str(gm2_plan_notices(plan), line);
                    free(line); json_free(url);
                } else {
                    char *line = gm2_xsf(
                        "Profile '%s' enables %s, which binds its own port and has no "
                        "/p/%s/ ingress on the default listener yet; the multiplexer would skip "
                        "the whole profile. Disable it there (platforms.%s.enabled: false) or "
                        "add a /p/<profile>/ ingress for it first.",
                        name ? name : "", pv, name ? name : "", pv);
                    gm2_arr_append_str(gm2_plan_blockers(plan), line);
                    free(line);
                }
            }
        }
    }
    json_free(secs);
    if (err) return err;
    return NULL;
}

/* PoP: gm2_flow_load_profile_configs @ hermes_cli/gateway_migrate.py:_load_profile_configs */
json_t *gm2_flow_load_profile_configs(json_t *plan, const gm2_seams_t *s) {
    json_t *en = gm2_scall(s, "multiplex_read_enter", gm2_args0());
    GM2_PROP(en);
    json_free(en);
    json_t *configs = json_object();
    json_t *err = NULL;
    json_t *profiles = gm2_plan_profiles(plan);
    if (profiles) {
        for (size_t i = 0; i < profiles->c.count && !err; i++) {
            const json_t *pg = profiles->c.items[i];
            const char *name = gm2_jstr(json_obj_get(pg, "name"));
            const char *home = gm2_jstr(json_obj_get(pg, "home"));
            json_t *a = gm2_args0(); gm2_aset_s(a, "home", home);
            json_t *cfg = gm2_scall(s, "profile_gateway_config", a);
            if (gm2_is_raise(cfg)) {
                if (gm2_is_exception(gm2_raise_class(cfg))) {
                    const char *msg = gm2_jstr(json_obj_get(cfg, "msg"));
                    char *line = gm2_xsf("Profile '%s': could not load its gateway config (%s).",
                                         name ? name : "", msg ? msg : "");
                    gm2_arr_append_str(gm2_plan_blockers(plan), line);
                    free(line); json_free(cfg);
                    continue;
                }
                err = cfg; break;
            }
            json_set(configs, name ? name : "", cfg);
        }
    }
    json_t *result = err ? err : configs;
    if (err) json_free(configs);
    return gm2_cm_finish(s, "multiplex_read_exit", NULL, result);
}

/* PoP: gm2_flow_plan_tail @ hermes_cli/gateway_migrate.py:_plan_tail */
json_t *gm2_flow_plan_tail(const json_t *plan, const gm2_seams_t *s) {
    json_t *lines = json_array();
    const json_t *blockers = json_obj_get(plan, "blockers");
    if (gm2_is_arr(blockers) && blockers->c.count > 0) {
        const json_t *manifest = json_obj_get(plan, "manifest");
        if (!gm2_jpresent(manifest)) {
            gm2_arr_append_str(lines, "");
            gm2_arr_append_str(lines, "  ✗ Blockers (fix these first, nothing will be changed):");
        } else {
            json_t *a = gm2_args0();
            gm2_aset_s(a, "default_home", gm2_jstr(json_obj_get(plan, "default_home")));
            json_t *mp = gm2_scall(s, "manifest_path", a);
            if (gm2_is_raise(mp)) { json_free(lines); return mp; }
            char *line = gm2_xsf("  ⚠ Blockers found, but a migration is already in progress on this host "
                                 "(manifest %s).", gm2_jstr(mp) ? gm2_jstr(mp) : "");
            gm2_arr_append_str(lines, "");
            gm2_arr_append_str(lines, line);
            free(line); json_free(mp);
            gm2_arr_append_str(lines, "    This run RESUMES it and converges anyway — refusing would strand the profiles whose");
            gm2_arr_append_str(lines, "    gateways an earlier attempt already removed. Fix these afterwards:");
        }
        for (size_t i = 0; i < blockers->c.count; i++) {
            char *line = gm2_xsf("    • %s", gm2_jstr(blockers->c.items[i]) ? gm2_jstr(blockers->c.items[i]) : "");
            gm2_arr_append_str(lines, line);
            free(line);
        }
    }
    const json_t *notices = json_obj_get(plan, "notices");
    if (gm2_is_arr(notices) && notices->c.count > 0) {
        gm2_arr_append_str(lines, "");
        gm2_arr_append_str(lines, "  Notices:");
        for (size_t i = 0; i < notices->c.count; i++) {
            char *line = gm2_xsf("    • %s", gm2_jstr(notices->c.items[i]) ? gm2_jstr(notices->c.items[i]) : "");
            gm2_arr_append_str(lines, line);
            free(line);
        }
    }
    return lines;
}

/* PoP: gm2_flow_format_update_warning @ hermes_cli/gateway_migrate.py:format_update_warning */
json_t *gm2_flow_format_update_warning(const json_t *plan, const json_t *auto_blockers) {
    json_t *lines = json_array();
    gm2_arr_append_str(lines, "⚠ Your profiles each run their own gateway. A single multiplexed gateway is the recommended");
    gm2_arr_append_str(lines, "  setup, but this install cannot be migrated automatically yet:");
    const json_t *blockers = json_obj_get(plan, "blockers");
    if (gm2_is_arr(blockers)) {
        for (size_t i = 0; i < blockers->c.count; i++) {
            char *line = gm2_xsf("    • %s", gm2_jstr(blockers->c.items[i]) ? gm2_jstr(blockers->c.items[i]) : "");
            gm2_arr_append_str(lines, line); free(line);
        }
    }
    if (gm2_is_arr(auto_blockers)) {
        for (size_t i = 0; i < auto_blockers->c.count; i++) {
            char *line = gm2_xsf("    • %s", gm2_jstr(auto_blockers->c.items[i]) ? gm2_jstr(auto_blockers->c.items[i]) : "");
            gm2_arr_append_str(lines, line); free(line);
        }
    }
    char *line = gm2_xsf("  After fixing the above, run:  %s", GM2_MIGRATE_COMMAND);
    gm2_arr_append_str(lines, line); free(line);
    gm2_arr_append_str(lines, "  (`hermes update` will migrate automatically once nothing blocks it.)");
    return lines;
}

/* PoP: gm2_flow_host_supports_migration @ hermes_cli/gateway_migrate.py:_host_supports_migration */
json_t *gm2_flow_host_supports_migration(const gm2_seams_t *s) {
    json_t *r = gm2_scall(s, "running_under_s6", gm2_args0());
    GM2_PROP(r);
    if (!gm2_jtruth(r)) { json_free(r); return NULL; }
    json_free(r);
    json_t *scandir = gm2_scall(s, "s6_scandir", gm2_args0());
    GM2_PROP(scandir);
    json_t *a = gm2_args0(); gm2_aset_s(a, "name", "default");
    json_t *name = gm2_scall(s, "named_slot_name", a);
    if (gm2_is_raise(name)) { json_free(scandir); return name; }
    char *path = gm2_path_join(gm2_jstr(scandir) ? gm2_jstr(scandir) : "",
                               gm2_jstr(name) ? gm2_jstr(name) : "");
    json_free(name);
    json_t *a2 = gm2_args0(); gm2_aset_s(a2, "path", path);
    json_t *isdir = gm2_scall(s, "path_is_dir", a2);
    if (gm2_is_raise(isdir)) { free(path); json_free(scandir); return isdir; }
    json_t *ret = NULL;
    if (!gm2_jtruth(isdir)) {
        /* the message re-joins scandir / named_slot_name('default') — a SECOND
         * named_slot_name call (live evaluates it again in the f-string). */
        json_t *a3 = gm2_args0(); gm2_aset_s(a3, "name", "default");
        json_t *name2 = gm2_scall(s, "named_slot_name", a3);
        if (gm2_is_raise(name2)) { free(path); json_free(scandir); json_free(isdir); return name2; }
        char *path2 = gm2_path_join(gm2_jstr(scandir) ? gm2_jstr(scandir) : "",
                                    gm2_jstr(name2) ? gm2_jstr(name2) : "");
        json_free(name2);
        char *m = gm2_xsf("s6-supervised container without a root gateway slot (%s); "
                          "the container's boot registers it — restart the container.", path2);
        free(path2);
        ret = json_string(m);
        free(m);
    }
    json_free(isdir); free(path); json_free(scandir);
    return ret;
}

/* PoP: gm2_flow_build_migration_plan @ hermes_cli/gateway_migrate.py:build_migration_plan */
json_t *gm2_flow_build_migration_plan(const gm2_seams_t *s) {
    json_t *dh = gm2_scall(s, "default_home", gm2_args0());
    GM2_PROP(dh);
    const char *home = gm2_jstr(dh);
    char *home_s = strdup(home ? home : "");
    json_free(dh);
    json_t *owner = gm2_scall(s, "host_gateway_owner", gm2_args0());
    if (gm2_is_raise(owner)) { free(home_s); return owner; }
    json_t *homes = gm2_flow_profile_homes(s);
    if (gm2_is_raise(homes)) { json_free(owner); free(home_s); return homes; }
    json_t *profiles = json_array();
    json_t *err = NULL;
    for (size_t i = 0; i < homes->c.count && !err; i++) {
        const json_t *pair = homes->c.items[i];
        const char *name = gm2_jstr(json_get(pair, 0));
        const char *phome = gm2_jstr(json_get(pair, 1));
        json_t *a = gm2_args0();
        gm2_aset_s(a, "home", phome);
        gm2_aset_j(a, "owner", owner);
        json_t *pid = gm2_scall(s, "own_gateway_pid", a);
        if (gm2_is_raise(pid)) { err = pid; break; }
        json_t *a2 = gm2_args0(); gm2_aset_s(a2, "home", phome);
        json_t *services = gm2_scall(s, "installed_services", a2);
        if (gm2_is_raise(services)) { json_free(pid); err = services; break; }
        json_t *ident = gm2_flow_gateway_identity(phome, pid, services, s);
        if (gm2_is_raise(ident)) { json_free(pid); json_free(services); err = ident; break; }
        const json_t *uid = json_get(ident, 0);
        const json_t *rh = json_get(ident, 1);
        json_t *rau = gm2_flow_systemd_service_user(phome, services, s);
        if (gm2_is_raise(rau)) { json_free(pid); json_free(services); json_free(ident); err = rau; break; }
        json_t *pg = json_object();
        json_set(pg, "name", json_string(name ? name : ""));
        json_set(pg, "home", json_string(phome ? phome : ""));
        json_set(pg, "pid", gm2_jpresent(pid) ? json_copy(pid) : json_null());
        json_set(pg, "services", json_copy(services));
        json_set(pg, "run_as_user", gm2_jpresent(rau) ? json_copy(rau) : json_null());
        json_set(pg, "uid", gm2_jpresent(uid) ? json_copy(uid) : json_null());
        bool same = gm2_jpresent(rh) && gm2_path_eq(gm2_jstr(rh) ? gm2_jstr(rh) : "", phome ? phome : "");
        json_set(pg, "runtime_home", (gm2_jpresent(rh) && !same) ? json_copy(rh) : json_null());
        json_append(profiles, pg);
        json_free(pid); json_free(services); json_free(ident); json_free(rau);
    }
    json_free(homes);
    if (err) { json_free(profiles); json_free(owner); free(home_s); return err; }
    json_free(owner);
    json_t *flag = gm2_flow_read_multiplex_flag(home_s, s);
    if (gm2_is_raise(flag)) { json_free(profiles); free(home_s); return flag; }
    json_t *a3 = gm2_args0(); gm2_aset_s(a3, "default_home", home_s);
    json_t *ls = gm2_scall(s, "recorded_served_profiles", a3);
    if (gm2_is_raise(ls)) { json_free(flag); json_free(profiles); free(home_s); return ls; }
    json_t *a4 = gm2_args0(); gm2_aset_s(a4, "default_home", home_s);
    json_t *manifest = gm2_scall(s, "read_manifest", a4);
    if (gm2_is_raise(manifest)) { json_free(ls); json_free(flag); json_free(profiles); free(home_s); return manifest; }
    json_t *plan = json_object();
    json_set(plan, "default_home", json_string(home_s));
    json_set(plan, "profiles", profiles);
    json_set(plan, "multiplex_flag_on", flag);
    json_set(plan, "live_served", gm2_jpresent(ls) ? ls : json_null());
    json_set(plan, "manifest", gm2_jpresent(manifest) ? manifest : json_null());
    json_set(plan, "interrupted", json_bool(false));
    json_set(plan, "blockers", json_array());
    json_set(plan, "notices", json_array());
    json_set(plan, "standalone_by_config", json_array());
    free(home_s);
    /* foldable = {name for name, _home in _profile_homes()} (second call) */
    json_t *homes2 = gm2_flow_profile_homes(s);
    if (gm2_is_raise(homes2)) { json_free(plan); return homes2; }
    json_t *a5 = gm2_args0();
    gm2_aset_b(a5, "multiplex", true);
    gm2_aset_b(a5, "include_parked", false);
    gm2_aset_b(a5, "include_standalone", true);
    json_t *pts = gm2_scall(s, "profiles_to_serve", a5);
    if (gm2_is_raise(pts)) { json_free(homes2); json_free(plan); return pts; }
    json_t *sbc = json_array();
    if (gm2_is_arr(pts)) {
        for (size_t i = 0; i < pts->c.count; i++) {
            const json_t *pair = pts->c.items[i];
            const char *name = gm2_jstr(json_get(pair, 0));
            if (!name || !strcmp(name, "default")) continue;
            bool foldable = false;
            for (size_t j = 0; j < homes2->c.count; j++) {
                const char *fn = gm2_jstr(json_get(homes2->c.items[j], 0));
                if (fn && !strcmp(fn, name)) { foldable = true; break; }
            }
            if (!foldable) gm2_arr_append_str(sbc, name);
        }
    }
    json_free(pts); json_free(homes2);
    json_set(plan, "standalone_by_config", sbc);
    /* plan.interrupted = plan.multiplex_flag_on and _manifest_not_yet_served(...) */
    const json_t *flagn = json_obj_get(plan, "multiplex_flag_on");
    if (gm2_jtruth(flagn)) {
        json_t *a6 = gm2_args0();
        gm2_aset_j(a6, "manifest", json_obj_get(plan, "manifest"));
        gm2_aset_j(a6, "live_served", json_obj_get(plan, "live_served"));
        json_t *mnys = gm2_scall(s, "manifest_not_yet_served", a6);
        if (gm2_is_raise(mnys)) { json_free(plan); return mnys; }
        /* live: plan.interrupted = flag and _manifest_not_yet_served(...) —
         * a None result assigns None (json_set ignores a NULL val). */
        json_set(plan, "interrupted", mnys ? mnys : json_null());
    }
    if (json_len(profiles) < 2) {
        gm2_arr_append_str(gm2_plan_notices(plan), "Only one profile exists: nothing to multiplex.");
        return plan;
    }
    json_t *configs = gm2_flow_load_profile_configs(plan, s);
    if (gm2_is_raise(configs)) { json_free(plan); return configs; }
    json_t *r1 = gm2_flow_check_duplicate_credentials(plan, configs, s);
    if (gm2_is_raise(r1)) { json_free(configs); json_free(plan); return r1; }
    json_free(r1);
    json_t *r2 = gm2_flow_check_secondary_port_binders(plan, configs, s);
    if (gm2_is_raise(r2)) { json_free(configs); json_free(plan); return r2; }
    json_free(r2);
    json_free(configs);
    json_t *a7 = gm2_args0(); gm2_aset_j(a7, "plan", plan);
    json_t *ab = gm2_scall(s, "auto_migration_blockers", a7);
    if (gm2_is_raise(ab)) { json_free(plan); return ab; }
    if (gm2_is_arr(ab)) {
        for (size_t i = 0; i < ab->c.count; i++) {
            char *line = gm2_xsf("Not migrated automatically by `hermes update`: %s",
                                 gm2_jstr(ab->c.items[i]) ? gm2_jstr(ab->c.items[i]) : "");
            gm2_arr_append_str(gm2_plan_notices(plan), line);
            free(line);
        }
    }
    json_free(ab);
    gm2_arr_append_str(gm2_plan_notices(plan),
        "Profiles created after the migration are served by the running multiplexer as soon as "
        "they exist (it rescans profiles/ on create/delete and every 30s).");
    return plan;
}

/* PoP: gm2_flow_format_plan @ hermes_cli/gateway_migrate.py:format_plan */
json_t *gm2_flow_format_plan(const json_t *plan, bool dry_run, const gm2_seams_t *s) {
    const char *home = gm2_jstr(json_obj_get(plan, "default_home"));
    if (!home) home = "";
    json_t *lines = json_array();
    gm2_arr_append_str(lines, dry_run ? "Migration plan (dry run — nothing changed)" : "Migration plan");
    char *l = gm2_xsf("  default home: %s", home);
    gm2_arr_append_str(lines, l); free(l);
    gm2_arr_append_str(lines, "");
    gm2_arr_append_str(lines, "  profile      gateway pid   service");
    json_t *profiles = gm2_plan_profiles(plan);
    json_t *err = NULL;
    if (profiles) {
        for (size_t i = 0; i < profiles->c.count && !err; i++) {
            const json_t *pg = profiles->c.items[i];
            const char *name = gm2_jstr(json_obj_get(pg, "name"));
            const json_t *pid = json_obj_get(pg, "pid");
            json_t *lab = gm2_pg_service_label(pg, s);
            if (gm2_is_raise(lab)) { err = lab; break; }
            char *pids = gm2_jtruth(pid) ? gm2_py_str(pid) : strdup("-");
            l = gm2_xsf("  %-12s %-13s %s", name ? name : "", pids, gm2_jstr(lab) ? gm2_jstr(lab) : "");
            gm2_arr_append_str(lines, l);
            free(l); free(pids); json_free(lab);
        }
    }
    if (err) { json_free(lines); return err; }
    const json_t *sbc = json_obj_get(plan, "standalone_by_config");
    if (gm2_is_arr(sbc) && sbc->c.count > 0) {
        char *joined = gm2_join_strs(sbc, ", ");
        l = gm2_xsf("  Standalone by config (gateway.standalone: true), left alone: %s", joined);
        gm2_arr_append_str(lines, l); free(l); free(joined);
        gm2_arr_append_str(lines, "    (temporary compatibility shim; remove the key and re-run once the gaps it covers for you are fixed)");
    }
    gm2_arr_append_str(lines, "");
    json_t *am = gm2_plan_prop(s, "plan_already_multiplexed");
    if (gm2_is_raise(am)) { json_free(lines); return am; }
    if (gm2_jtruth(am)) {
        const json_t *ls = json_obj_get(plan, "live_served");
        char *suffix;
        if (gm2_jtruth(ls)) {
            char *joined = gm2_join_strs(ls, ", ");
            suffix = gm2_xsf(" (serving %s)", joined);
            free(joined);
        } else suffix = strdup(" (flag on)");
        l = gm2_xsf("  ✓ The default gateway is already multiplexing%s.", suffix);
        gm2_arr_append_str(lines, l);
        free(l); free(suffix); json_free(am);
        return lines;
    }
    json_free(am);
    const json_t *interrupted = json_obj_get(plan, "interrupted");
    const json_t *flagn = json_obj_get(plan, "multiplex_flag_on");
    if (gm2_jtruth(interrupted)) {
        char *mp = gm2_path_join(home, GM2_MANIFEST_NAME);
        l = gm2_xsf("  ↻ An earlier migration was interrupted before the default gateway came up "
                    "(flag on, no live multiplexer; manifest %s); this run resumes it.", mp);
        gm2_arr_append_str(lines, l); free(l); free(mp);
    } else if (gm2_jtruth(flagn)) {
        json_t *ss0 = gm2_plan_prop(s, "plan_standalone_secondaries");
        if (gm2_is_raise(ss0)) { json_free(lines); return ss0; }
        if (gm2_jtruth(ss0))
            gm2_arr_append_str(lines, "  ↻ Half-migrated host: the flag is on, but the profile(s) below still own a gateway. This run converges them.");
        json_free(ss0);
    }
    json_t *steps = json_array();
    json_t *a = gm2_args0(); gm2_aset_j(a, "plan", plan);
    json_t *signalled = gm2_scall(s, "signalled_gateways", a);
    if (gm2_is_raise(signalled)) { json_free(steps); json_free(lines); return signalled; }
    json_t *ss = gm2_plan_prop(s, "plan_standalone_secondaries");
    if (gm2_is_raise(ss)) { json_free(signalled); json_free(steps); json_free(lines); return ss; }
    if (gm2_is_arr(ss)) {
        for (size_t i = 0; i < ss->c.count && !err; i++) {
            const json_t *pg = ss->c.items[i];
            const char *name = gm2_jstr(json_obj_get(pg, "name"));
            const json_t *pid = json_obj_get(pg, "pid");
            char *stop_part = strdup("");
            if (gm2_jtruth(pid)) {
                char *pids = gm2_py_str(pid);
                free(stop_part);
                stop_part = gm2_xsf("stop pid %s", pids);
                free(pids);
            }
            char *inner = strdup("");
            const json_t *services = json_obj_get(pg, "services");
            if (gm2_is_arr(services)) {
                for (size_t j = 0; j < services->c.count && !err; j++) {
                    const json_t *svc = services->c.items[j];
                    json_t *a1 = gm2_args0(); gm2_aset_j(a1, "service", svc);
                    json_t *rv = gm2_scall(s, "remove_verb", a1);
                    if (gm2_is_raise(rv)) { err = rv; break; }
                    json_t *a2 = gm2_args0(); gm2_aset_j(a2, "service", svc);
                    json_t *sl = gm2_scall(s, "service_label", a2);
                    if (gm2_is_raise(sl)) { json_free(rv); err = sl; break; }
                    char *piece = gm2_xsf("%s %s", gm2_jstr(rv) ? gm2_jstr(rv) : "", gm2_jstr(sl) ? gm2_jstr(sl) : "");
                    char *next = gm2_xsf("%s%s%s", inner, inner[0] ? " + " : "", piece);
                    free(inner); inner = next; free(piece);
                    json_free(rv); json_free(sl);
                }
            }
            if (err) { free(stop_part); free(inner); break; }
            char *what;
            if (stop_part[0] && inner[0]) what = gm2_xsf("%s + %s", stop_part, inner);
            else if (stop_part[0]) what = strdup(stop_part);
            else what = strdup(inner);
            l = gm2_xsf("  - %s: %s", name ? name : "", what);
            gm2_arr_append_str(steps, l);
            free(l); free(what); free(stop_part); free(inner);
        }
    }
    json_free(ss);
    if (err) { json_free(signalled); json_free(steps); json_free(lines); return err; }
    if (json_len(profiles) < 2) {
        json_t *tail = gm2_flow_plan_tail(plan, s);
        if (gm2_is_raise(tail)) { json_free(signalled); json_free(steps); json_free(lines); return tail; }
        for (size_t i = 0; i < tail->c.count; i++) json_append(lines, json_copy(tail->c.items[i]));
        json_free(tail); json_free(signalled); json_free(steps);
        return lines;
    }
    if (steps->c.count == 0) {
        gm2_arr_append_str(lines, "  No secondary profile runs its own gateway; the only step is turning the flag on:");
    } else {
        gm2_arr_append_str(lines, "  Steps:");
        for (size_t i = 0; i < steps->c.count; i++) json_append(lines, json_copy(steps->c.items[i]));
    }
    char *cfgp = gm2_path_join(home, "config.yaml");
    l = gm2_xsf("  - default: set gateway.multiplex_profiles: true in %s", cfgp);
    gm2_arr_append_str(lines, l); free(l); free(cfgp);
    /* target = _resume_target(plan)[0] if manifest is not None else target_service_kind() */
    json_t *target = NULL;
    const json_t *manifest = json_obj_get(plan, "manifest");
    if (gm2_jpresent(manifest)) {
        json_t *a2 = gm2_args0(); gm2_aset_j(a2, "plan", plan);
        json_t *rt = gm2_scall(s, "resume_target", a2);
        if (gm2_is_raise(rt)) { json_free(signalled); json_free(steps); json_free(lines); return rt; }
        const json_t *t0 = gm2_is_arr(rt) ? json_get(rt, 0) : NULL;
        target = gm2_jpresent(t0) ? json_copy(t0) : NULL;
        json_free(rt);
    } else {
        target = gm2_plan_prop(s, "plan_target_service_kind");
        if (gm2_is_raise(target)) { json_free(signalled); json_free(steps); json_free(lines); return target; }
    }
    json_t *dg = gm2_plan_prop(s, "plan_default");
    if (gm2_is_raise(dg)) { json_free(target); json_free(signalled); json_free(steps); json_free(lines); return dg; }
    json_t *hg = gm2_pg_has_gateway(dg, s);
    if (gm2_is_raise(hg)) { json_free(dg); json_free(target); json_free(signalled); json_free(steps); json_free(lines); return hg; }
    const char *verb = gm2_jtruth(hg) ? "restart" : "start";
    json_free(dg); json_free(hg);
    char *tsuffix;
    if (gm2_jtruth(target)) {
        const json_t *t0 = gm2_is_arr(target) ? json_get(target, 0) : NULL;
        tsuffix = gm2_xsf(" via %s", gm2_jstr(t0) ? gm2_jstr(t0) : "");
    } else tsuffix = strdup(" (detached)");
    json_t *es = gm2_plan_prop(s, "plan_expected_served");
    if (gm2_is_raise(es)) { free(tsuffix); json_free(target); json_free(signalled); json_free(steps); json_free(lines); return es; }
    size_t nserve = gm2_is_arr(es) ? es->c.count : 0;
    json_free(es);
    l = gm2_xsf("  - default: %s the gateway%s, verify it serves %zu profiles", verb, tsuffix, nserve);
    gm2_arr_append_str(lines, l); free(l); free(tsuffix); json_free(target);
    char *mp = gm2_path_join(home, GM2_MANIFEST_NAME);
    l = gm2_xsf("  - record the previous state in %s (used to undo a FAILED apply, and to resume this command after a crash)", mp);
    gm2_arr_append_str(lines, l); free(l); free(mp);
    if (gm2_is_arr(signalled) && signalled->c.count > 0) {
        char *joined = gm2_join_strs(signalled, ", ");
        gm2_arr_append_str(lines, "");
        l = gm2_xsf("  ⚠ This SIGTERMs running gateway process(es): %s.", joined);
        gm2_arr_append_str(lines, l); free(l); free(joined);
        gm2_arr_append_str(lines, "    They drain in-flight turns and exit; their profiles are served by the host gateway afterwards.");
    }
    json_t *tail = gm2_flow_plan_tail(plan, s);
    if (gm2_is_raise(tail)) { json_free(signalled); json_free(steps); json_free(lines); return tail; }
    for (size_t i = 0; i < tail->c.count; i++) json_append(lines, json_copy(tail->c.items[i]));
    json_free(tail); json_free(signalled); json_free(steps);
    return lines;
}

/* PoP: gm2_flow_cmd_migrate @ hermes_cli/gateway_migrate.py:cmd_migrate */
json_t *gm2_flow_cmd_migrate(const json_t *args, const gm2_seams_t *s) {
    const json_t *backends = json_obj_get(args, "backends");
    if (gm2_jtruth(backends)) {
        json_t *a = gm2_args0(); gm2_aset_j(a, "args", args);
        json_t *r = gm2_scall(s, "cmd_backends_report", a);
        GM2_PROP(r);
        json_free(r);
        return NULL;
    }
    const json_t *jf = json_obj_get(args, "json");
    if (gm2_jtruth(jf)) {
        json_t *a = gm2_args0();
        gm2_aset_s(a, "line", "✗ --json is only supported with --backends.");
        gm2_aset_s(a, "file", "stderr");
        json_t *r = gm2_scall(s, "print", a);
        GM2_PROP(r);
        json_free(r);
        return gm2_raise_obj("SystemExit", "2");
    }
    json_t *reason = gm2_flow_host_supports_migration(s);
    GM2_PROP(reason);
    if (gm2_jtruth(reason)) {
        char *m = gm2_xsf("✗ %s", gm2_jstr(reason));
        json_t *a = gm2_args0(); gm2_aset_s(a, "line", m);
        json_t *r = gm2_scall(s, "print", a);
        free(m); json_free(reason);
        GM2_PROP(r);
        json_free(r);
        return gm2_raise_obj("SystemExit", "1");
    }
    json_free(reason);
    json_t *plan = gm2_flow_build_migration_plan(s);
    GM2_PROP(plan);
    bool dry_run = gm2_jtruth(json_obj_get(args, "dry_run"));
    json_t *fp = gm2_flow_format_plan(plan, dry_run, s);
    if (gm2_is_raise(fp)) { json_free(plan); return fp; }
    json_t *a = gm2_args0(); gm2_aset_j(a, "lines", fp);
    json_t *pl = gm2_scall(s, "print_lines", a);
    json_free(fp);
    if (gm2_is_raise(pl)) { json_free(plan); return pl; }
    json_free(pl);
    if (dry_run) { json_free(plan); return NULL; }
    json_t *am = gm2_plan_prop(s, "plan_already_multiplexed");
    if (gm2_is_raise(am)) { json_free(plan); return am; }
    if (gm2_jtruth(am)) { json_free(am); json_free(plan); return NULL; }
    json_free(am);
    const json_t *manifest = json_obj_get(plan, "manifest");
    if (!gm2_jpresent(manifest)) {
        json_t *blocked = gm2_plan_prop(s, "plan_blocked");
        if (gm2_is_raise(blocked)) { json_free(plan); return blocked; }
        json_t *profiles = gm2_plan_profiles(plan);
        bool cond = gm2_jtruth(blocked) || (profiles ? profiles->c.count : 0) < 2;
        json_free(blocked);
        if (cond) {
            json_t *blocked2 = gm2_plan_prop(s, "plan_blocked");
            if (gm2_is_raise(blocked2)) { json_free(plan); return blocked2; }
            const char *code = gm2_jtruth(blocked2) ? "1" : "0";
            json_free(blocked2); json_free(plan);
            return gm2_raise_obj("SystemExit", code);
        }
    }
    bool yes = gm2_jtruth(json_obj_get(args, "yes"));
    if (!yes) {
        json_t *it = gm2_scall(s, "stdin_isatty", gm2_args0());
        if (gm2_is_raise(it)) { json_free(plan); return it; }
        if (gm2_jtruth(it)) {
            json_t *a2 = gm2_args0();
            gm2_aset_s(a2, "question", "Apply this migration now?");
            gm2_aset_b(a2, "default", true);
            json_t *py = gm2_scall(s, "prompt_yes_no", a2);
            if (gm2_is_raise(py)) { json_free(it); json_free(plan); return py; }
            if (!gm2_jtruth(py)) {
                json_t *a3 = gm2_args0(); gm2_aset_s(a3, "line", "Aborted; nothing changed.");
                json_t *r = gm2_scall(s, "print", a3);
                json_free(py); json_free(it);
                GM2_PROP(r);
                json_free(r); json_free(plan);
                return NULL;
            }
            json_free(py);
        }
        json_free(it);
    }
    json_t *pr = gm2_scall(s, "print", gm2_args0());
    if (gm2_is_raise(pr)) { json_free(plan); return pr; }
    json_free(pr);
    json_t *a4 = gm2_args0();
    gm2_aset_j(a4, "plan", plan);
    gm2_aset_n(a4, "served_wait", GM2_SERVED_WAIT);
    json_t *amig = gm2_scall(s, "apply_migration", a4);
    if (gm2_is_raise(amig)) { json_free(plan); return amig; }
    const char *code = gm2_jtruth(amig) ? "0" : "1";
    json_free(amig); json_free(plan);
    return gm2_raise_obj("SystemExit", code);
}

/* PoP: gm2_flow_maybe_auto_migrate_after_update @ hermes_cli/gateway_migrate.py:maybe_auto_migrate_after_update */
json_t *gm2_flow_maybe_auto_migrate_after_update(const gm2_seams_t *s) {
    json_t *hs = gm2_flow_host_supports_migration(s);
    GM2_PROP(hs);
    if (gm2_jpresent(hs)) { json_free(hs); return NULL; }
    json_free(hs);
    json_t *dh = gm2_scall(s, "default_home", gm2_args0());
    GM2_PROP(dh);
    json_t *a = gm2_args0(); gm2_aset_s(a, "default_home", gm2_jstr(dh));
    json_free(dh);
    json_t *oo = gm2_scall(s, "auto_migration_opted_out", a);
    GM2_PROP(oo);
    if (gm2_jtruth(oo)) { json_free(oo); return NULL; }
    json_free(oo);
    json_t *plan = gm2_flow_build_migration_plan(s);
    GM2_PROP(plan);
    json_t *am = gm2_plan_prop(s, "plan_already_multiplexed");
    if (gm2_is_raise(am)) { json_free(plan); return am; }
    json_t *profiles = gm2_plan_profiles(plan);
    if (gm2_jtruth(am) || (profiles ? profiles->c.count : 0) < 2) {
        json_free(am); json_free(plan); return NULL;
    }
    json_free(am);
    bool resuming = gm2_jpresent(json_obj_get(plan, "manifest"));
    json_t *ss = gm2_plan_prop(s, "plan_standalone_secondaries");
    if (gm2_is_raise(ss)) { json_free(plan); return ss; }
    if (!gm2_jtruth(ss) && !resuming) { json_free(ss); json_free(plan); return NULL; }
    json_free(ss);
    json_t *masked = NULL;
    if (!resuming) {
        json_t *a2 = gm2_args0();
        gm2_aset_s(a2, "default_home", gm2_jstr(json_obj_get(plan, "default_home")));
        masked = gm2_scall(s, "masked_default_unit", a2);
        if (gm2_is_raise(masked)) { json_free(plan); return masked; }
    }
    if (gm2_jpresent(masked)) {
        char *m = gm2_xsf("\n  ℹ Per-profile gateways kept: the default gateway unit is masked (%s).\n"
                          "    Unmask it and run %s to fold them into one gateway.",
                          gm2_jstr(masked), GM2_MIGRATE_COMMAND);
        json_t *a3 = gm2_args0(); gm2_aset_s(a3, "line", m);
        json_t *r = gm2_scall(s, "print", a3);
        free(m); json_free(masked);
        GM2_PROP(r);
        json_free(r); json_free(plan);
        return NULL;
    }
    json_free(masked);
    json_t *pr = gm2_scall(s, "print", gm2_args0());
    if (gm2_is_raise(pr)) { json_free(plan); return pr; }
    json_free(pr);
    json_t *a4 = gm2_args0(); gm2_aset_j(a4, "plan", plan);
    json_t *ab = gm2_scall(s, "auto_migration_blockers", a4);
    if (gm2_is_raise(ab)) { json_free(plan); return ab; }
    if (!resuming) {
        json_t *blocked = gm2_plan_prop(s, "plan_blocked");
        if (gm2_is_raise(blocked)) { json_free(ab); json_free(plan); return blocked; }
        bool cond = gm2_jtruth(blocked) || gm2_jtruth(ab);
        json_free(blocked);
        if (cond) {
            json_t *fuw = gm2_flow_format_update_warning(plan, ab);
            json_t *a5 = gm2_args0(); gm2_aset_j(a5, "lines", fuw);
            json_t *pl = gm2_scall(s, "print_lines", a5);
            json_free(fuw); json_free(ab);
            GM2_PROP(pl);
            json_free(pl); json_free(plan);
            return NULL;
        }
    }
    json_free(ab);
    json_t *a6 = gm2_args0();
    gm2_aset_s(a6, "line", "→ Migrating per-profile gateways onto one multiplexed default gateway...");
    json_t *pr2 = gm2_scall(s, "print", a6);
    if (gm2_is_raise(pr2)) { json_free(plan); return pr2; }
    json_free(pr2);
    json_t *fp = gm2_flow_format_plan(plan, false, s);
    if (gm2_is_raise(fp)) { json_free(plan); return fp; }
    json_t *a7 = gm2_args0(); gm2_aset_j(a7, "lines", fp);
    json_t *pl2 = gm2_scall(s, "print_lines", a7);
    json_free(fp);
    if (gm2_is_raise(pl2)) { json_free(plan); return pl2; }
    json_free(pl2);
    json_t *a8 = gm2_args0();
    gm2_aset_j(a8, "plan", plan);
    gm2_aset_n(a8, "served_wait", GM2_SERVED_WAIT);
    json_t *amig = gm2_scall(s, "apply_migration", a8);
    if (gm2_is_raise(amig)) { json_free(plan); return amig; }
    json_free(amig);
    json_free(plan);
    return NULL;
}
