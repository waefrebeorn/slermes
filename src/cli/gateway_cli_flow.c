/*
 * gateway_cli_flow.c — hermes_cli/gateway.py FLOW cores + leaves (C11 port).
 * LIVE authority: C:/Users/eman5/hermes-agent/hermes_cli/gateway.py
 *
 * Self-contained, libjson-only. Ports the DECISION sequences of the gateway
 * CLI's multiplexer/host-attach guards, systemd scope/reset helpers, runtime
 * pid/timeout leaves and the status/print flows. Every live effect (systemctl
 * runs, unit-file reads, host-record probes, foreign-module imports, config
 * reads, print/log emission, process signals) enters through ONE universal
 * seam; the cores perform NO I/O.
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (table below); `args` is a BORROWED JSON
 * object (the seam records a copy and must not free it); the result is a NEW
 * json_t* the caller frees. NULL == Python None (never a JSON_NULL node);
 * {"__raise__": cls, "msg": msg, ...} == a raised exception. The harness
 * records {"ev": name, "args": args, "ret": result} per call in order; the
 * oracle's recording stubs emit the identical shape driving LIVE Python with
 * the same scripts. BaseException kinds (SystemExit/KeyboardInterrupt/
 * GeneratorExit/CancelledError) PASS THROUGH every `except Exception` site.
 * With no seam installed the dispatcher is fail-closed (returns a RuntimeError
 * raise spec).
 *
 * RETURN CONTRACT: every ported function returns a NEW json_t* (caller frees):
 * NULL == None; a {"__raise__": ...} object == a propagated exception. The
 * flows are stateless (no injected module state); the observable effects are
 * the ordered seam trace + the return value.
 *
 * Seam name table (name -> live call; args key order is part of the trace):
 *   get_hermes_home            hermes_cli.config.get_hermes_home()      args{} ret home str
 *   get_service_name           get_service_name()                       args{} ret str
 *   get_systemd_unit_path      get_systemd_unit_path(system=)           args{system} ret path str
 *   path_exists                Path.exists()                            args{path} ret bool
 *   read_text                  Path.read_text(encoding=)                args{path,encoding} ret str
 *   unlink                     Path.unlink()                            args{path} ret null
 *   run_systemctl              _run_systemctl(args,system=,**kw)        args{args,system,**kw} ret {stdout,returncode,..}|raise
 *   positive_pid               _positive_pid(value)                     args{value?} ret int|null
 *   profile_suffix             _profile_suffix()                        args{} ret str
 *   select_systemd_scope       _select_systemd_scope(system)            args{system} ret bool
 *   require_root_for_system_service _require_root_for_system_service(action) args{action} ret null|raise
 *   preflight_user_systemd     _preflight_user_systemd()                args{} ret null|raise
 *   require_service_installed  _require_service_installed(action,system=) args{action,system} ret null|raise
 *   systemd_error_indicates_start_limit _systemd_error_indicates_start_limit(exc) args{exc} ret bool
 *   systemd_service_is_start_limited _systemd_service_is_start_limited(system=) args{system} ret bool
 *   print_systemd_start_limit_wait _print_systemd_start_limit_wait(system=) args{system} ret null
 *   service_scope_label        _service_scope_label(system)             args{system} ret str
 *   wait_for_systemd_service_restart _wait_for_systemd_service_restart(system=,previous_pid=) args{system,previous_pid?} ret null
 *   supports_systemd_services  supports_systemd_services()              args{} ret bool
 *   is_service_installed       _is_service_installed()                  args{} ret bool
 *   named_profile_served       named_profile_served_by_running_multiplexer() args{} ret bool
 *   guard_named_profile_under_multiplexer _guard_named_profile_under_multiplexer(force=) args{force} ret null|raise
 *   running_under_gateway_supervisor _running_under_gateway_supervisor() args{} ret bool
 *   find_gateway_pids          find_gateway_pids(all_profiles=)         args{all_profiles} ret [int]
 *   kill_gateway_processes     kill_gateway_processes()                 args{} ret int
 *   terminate_pid              gateway.status.terminate_pid(pid,force=) args{pid,force} ret null|raise
 *   foreign_systemd_unit_home  gateway_service_owner.foreign_systemd_unit_home(s) args{system} ret str|null
 *   read_raw_config            hermes_cli.config.read_raw_config()      args{} ret dict|null
 *   os_getenv                  os.getenv(name)                          args{name} ret str|null
 *   parse                      parse(value)  (_agent_timeout_setting cb) args{value?} ret num
 *   print_info / print_success / print_error / print_warning  hermes_cli.setup.* args{text} ret null
 *   print                      builtin print(text?)                     args{text?} ret null
 *   emit                       _print_indented emit callback            args{text} ret null
 *   logger.debug               logger.debug(msg,*a,exc_info=)           args{msg,args?,exc_info?} ret null
 *   logger.warning             logger.warning(msg,*a)                   args{msg,args?} ret null
 *   host_gateway_serving       gateway.host_attach.host_gateway_serving(name) args{name} ret gateway|null
 *   host_gateway               gateway.host_attach.host_gateway()       args{} ret gateway|null
 *   launched_by_other_tenant   gateway.host_attach.launched_by_other_tenant(owner_home,our_home) args{owner_home,our_home} ret bool
 *   decide                     gateway.host_attach.decide(home,replace=) args{home,replace} ret decision|null
 *   standalone_rescan_message  gateway.host_attach.standalone_rescan_message(profile) args{profile} ret str
 *   gateway.describe           HostGateway.describe()                   args{} ret str
 *   get_process_hermes_home    gateway.status._get_process_hermes_home() args{} ret str
 *   same_hermes_home           gateway.status._same_hermes_home(l,r)    args{left,right} ret bool
 *   profile_name_for_home      hermes_constants.profile_name_for_home(home) args{home} ret str|null
 *   display_hermes_home        hermes_constants.display_hermes_home(home) args{home} ret str
 *   profile_is_standalone      hermes_cli.profiles.profile_is_standalone(home) args{home} ret bool
 *   recorded_standalone_warning_lines hermes_cli.gateway_multiplex_mode.recorded_standalone_warning_lines() args{} ret [str]
 *   served_profile_ingress_urls hermes_cli.gateway_multiplex_served.served_profile_ingress_urls(profile) args{profile?} ret dict|null
 *   format_ingress_url_lines   hermes_cli.gateway_multiplex_served.format_ingress_url_lines(per_platform,indent=) args{per_platform,indent} ret [str]
 *
 * Constants embedded (real on both sides, NOT seamed): GATEWAY_SERVICE_RESTART_EXIT_CODE=75,
 * GATEWAY_FATAL_CONFIG_EXIT_CODE=78, ATTACH="attach", REFUSE="refuse", REPLACE_HOST="replace-host",
 * STANDALONE_DEPRECATION_NOTICE (byte-exact UTF-8), _CAPTURE_TEXT (folded into run_systemctl args).
 *
 * Internal composition runs REAL on both sides (no seam): _parse_kv_pairs inside
 * _systemctl_show; _current_profile_name inside host_multiplexer_serving /
 * _served_profile_needs_no_service / _named_profile_refused_under_multiplexer;
 * host_multiplexer_serving inside _served_by_another_host_gateway;
 * _served_by_another_host_gateway inside _named_profile_refused_under_multiplexer;
 * _named_profile_refused_under_multiplexer inside _served_profile_needs_no_service;
 * _host_decision_exit_code inside _attach_to_host_gateway_or_guard.
 *
 * Fixture domain (kept identical on both sides): ASCII text; POSIX lexical paths
 * (no symlinks/resolve); str.strip()/splitlines() model the ASCII whitespace set
 * (space \t \n \r \v \f); integral JSON numbers read as int; gateway/decision
 * doubles are JSON objects {home,pid,profile_label,...} / {outcome,message,transient}.
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
static bool jtruth(const json_t *v) {
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
static bool jpresent(const json_t *v) { return v && v->type != JSON_NULL; }
static bool is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }
static bool is_bool(const json_t *v) { return v && v->type == JSON_BOOL; }
static const char *jstr(const json_t *v) {
    return (is_str(v) && v->str_val) ? v->str_val : NULL;
}
static char *xsf(const char *fmt, ...) {
    va_list ap, ap2; va_start(ap, fmt); va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) { va_end(ap2); return strdup(""); }
    char *buf = malloc((size_t)n + 1);
    if (buf) vsnprintf(buf, (size_t)n + 1, fmt, ap2);
    va_end(ap2); return buf ? buf : strdup("");
}
static char *py_float_str(double d) {
    if (d == (double)(long long)d) return xsf("%.0f", d);
    char buf[64];
    for (int prec = 15; prec <= 17; prec++) {
        snprintf(buf, sizeof(buf), "%.*g", prec, d);
        if (strtod(buf, NULL) == d) break;
    }
    return strdup(buf);
}
static char *py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    switch (v->type) {
        case JSON_BOOL:   return strdup(v->bool_val ? "True" : "False");
        case JSON_NUMBER: return py_float_str(v->num_val);
        case JSON_STRING: return strdup(v->str_val ? v->str_val : "");
        default:          return strdup("");  /* containers not str()-projected in this port */
    }
}

/* ── universal seam ── */
typedef struct gc_trace { json_t *events; } gc_trace_t;
typedef struct gc_seams {
    void *ctx;
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} gc_seams_t;

static void gc_trace_add(gc_trace_t *t, json_t *ev) {
    if (!t) { json_free(ev); return; }
    if (!t->events) t->events = json_array();
    json_append(t->events, ev);
}
static json_t *gc_raise(const char *cls, const char *msg) {
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(cls));
    json_set(o, "msg", json_string(msg ? msg : ""));
    return o;
}
static bool gc_is_raise(const json_t *r) {
    return is_obj(r) && json_obj_get(r, "__raise__") != NULL;
}
static const char *gc_raise_class(const json_t *r) {
    return json_get_str(r, "__raise__", "");
}
static bool gc_is_base_exc(const char *cls) {
    return !strcmp(cls, "SystemExit") || !strcmp(cls, "KeyboardInterrupt")
        || !strcmp(cls, "GeneratorExit") || !strcmp(cls, "CancelledError");
}
/* Fail-closed dispatcher: no seam installed -> RuntimeError raise spec.
 * Ownership: gc_call OWNS args (frees it after the call); the seam BORROWS args
 * (records a deep copy, must not free it). Results are NEW (caller frees). */
static json_t *gc_call(const gc_seams_t *s, const char *name, const json_t *args) {
    if (!s || !s->call) {
        json_free((json_t *)args);
        return gc_raise("RuntimeError", "no seam installed");
    }
    json_t *r = s->call(s->ctx, name, args);
    json_free((json_t *)args);
    return r;
}
/* args builders (owned object passed to gc_call; the seam borrows it). */
static json_t *gc_args0(void) { return json_object(); }
static void gc_set_str(json_t *o, const char *k, const char *v) {
    if (v) json_set(o, k, json_string(v));
}
static void gc_set_bool(json_t *o, const char *k, bool v) { json_set(o, k, json_bool(v)); }
static void gc_set_num(json_t *o, const char *k, double v) { json_set(o, k, json_number(v)); }
static void gc_set_node(json_t *o, const char *k, const json_t *v) {
    if (v && v->type != JSON_NULL) json_set(o, k, json_copy(v));
}

/* ── ASCII string / POSIX path helpers ── */
static bool gc_is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || c == '\x1c' || c == '\x1d' || c == '\x1e' || c == '\x1f';
}
static char *gc_strip(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && gc_is_ws(s[a])) a++;
    while (b > a && gc_is_ws(s[b - 1])) b--;
    char *out = malloc(b - a + 1);
    memcpy(out, s + a, b - a); out[b - a] = '\0';
    return out;
}
/* str.splitlines() over the ASCII boundary set: \n \r \r\n \v \f. */
static json_t *gc_splitlines(const char *s) {
    json_t *arr = json_array();
    if (!s) return arr;
    size_t n = strlen(s), i = 0, start = 0;
    while (i < n) {
        char c = s[i];
        if (c == '\n' || c == '\v' || c == '\f'
            || c == '\x1c' || c == '\x1d' || c == '\x1e') {
            char *line = strndup(s + start, i - start);
            json_append(arr, json_string(line)); free(line);
            i++; start = i;
        } else if (c == '\r') {
            char *line = strndup(s + start, i - start);
            json_append(arr, json_string(line)); free(line);
            i++; if (i < n && s[i] == '\n') i++;
            start = i;
        } else i++;
    }
    if (start < n) {
        char *line = strndup(s + start, n - start);
        json_append(arr, json_string(line)); free(line);
    }
    return arr;
}
static char *gc_path_parent(const char *p) {
    if (!p || !*p) return strdup(".");
    char *tmp = strdup(p); size_t n = strlen(tmp);
    while (n > 1 && tmp[n - 1] == '/') tmp[--n] = '\0';
    char *slash = strrchr(tmp, '/');
    char *out;
    if (!slash) out = strdup(".");
    else if (slash == tmp) out = strdup("/");
    else out = strndup(tmp, (size_t)(slash - tmp));
    free(tmp); return out;
}
static char *gc_path_name(const char *p) {
    if (!p || !*p) return strdup("");
    char *tmp = strdup(p); size_t n = strlen(tmp);
    while (n > 1 && tmp[n - 1] == '/') tmp[--n] = '\0';
    char *slash = strrchr(tmp, '/');
    char *out = strdup(slash ? slash + 1 : tmp);
    free(tmp); return out;
}
static char *gc_path_join(const char *a, const char *b) {
    if (!b || !*b) return strdup(a ? a : "");
    if (b[0] == '/') return strdup(b);
    if (!a || !*a) return strdup(b);
    size_t na = strlen(a);
    if (a[na - 1] == '/') return xsf("%s%s", a, b);
    return xsf("%s/%s", a, b);
}

static const char *py_type_name(const json_t *v) {
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
/* Exception-class catch sets (scripted class names; subclasses folded in). */
static bool gc_cls_runtimeerror(const char *c) {
    return !strcmp(c, "RuntimeError") || !strcmp(c, "SystemctlUnavailableError");
}
static bool gc_cls_oserror(const char *c) {
    return !strcmp(c, "OSError") || !strcmp(c, "FileNotFoundError")
        || !strcmp(c, "PermissionError") || !strcmp(c, "ProcessLookupError")
        || !strcmp(c, "TimeoutError") || !strcmp(c, "ConnectionError")
        || !strcmp(c, "ConnectionRefusedError") || !strcmp(c, "InterruptedError");
}
static bool gc_cls_timeoutexpired(const char *c) { return !strcmp(c, "TimeoutExpired"); }

/* ── leaves ── */

/* PoP: gc_parse_kv_pairs @ hermes_cli/gateway.py:_parse_kv_pairs */
json_t *gc_parse_kv_pairs(const json_t *items) {
    json_t *out = json_object();
    if (!is_arr(items)) return out;
    for (size_t i = 0; i < items->c.count; i++) {
        const json_t *item = items->c.items[i];
        if (!is_str(item)) {
            json_free(out);
            return gc_raise("TypeError", xsf("argument of type '%s' is not iterable", py_type_name(item)));
        }
        const char *s = item->str_val ? item->str_val : "";
        const char *eq = strchr(s, '=');
        if (!eq) continue;
        char *k = strndup(s, (size_t)(eq - s));
        char *v = gc_strip(eq + 1);
        json_set(out, k, json_string(v));
        free(k); free(v);
    }
    return out;
}

/* PoP: gc_systemd_cli_bits @ hermes_cli/gateway.py:_systemd_cli_bits */
json_t *gc_systemd_cli_bits(bool system) {
    json_t *a = json_array();
    if (system) {
        json_append(a, json_string("sudo "));
        json_append(a, json_string(" --system"));
        json_append(a, json_string(""));
    } else {
        json_append(a, json_string(""));
        json_append(a, json_string(""));
        json_append(a, json_string("--user "));
    }
    return a;
}

/* PoP: gc_runtime_state_pid @ hermes_cli/gateway.py:_runtime_state_pid */
json_t *gc_runtime_state_pid(const json_t *state, const gc_seams_t *s) {
    const json_t *st = state;
    json_t *empty = NULL;
    if (!jtruth(state)) { empty = json_object(); st = empty; }
    if (!is_obj(st)) {
        json_free(empty);
        return gc_raise("AttributeError", xsf("'%s' object has no attribute 'get'", py_type_name(state)));
    }
    const json_t *pid = json_obj_get(st, "pid");
    json_t *args = gc_args0();
    if (pid && pid->type != JSON_NULL) json_set(args, "value", json_copy(pid));
    else if (!pid) json_set(args, "value", json_number(0));
    /* present-null -> value None -> dropped by the seam args projection */
    json_t *r = gc_call(s, "positive_pid", args);
    json_free(empty);
    if (gc_is_raise(r)) return r;
    json_t *ret = jtruth(r) ? r : json_number(0);
    if (ret != r) json_free(r);
    return ret;
}

/* PoP: gc_host_decision_exit_code @ hermes_cli/gateway.py:_host_decision_exit_code */
json_t *gc_host_decision_exit_code(const json_t *decision) {
    bool transient = is_obj(decision) ? json_get_bool(decision, "transient", false) : false;
    return json_number(transient ? 75 : 78);
}

/* PoP: gc_print_info_lines @ hermes_cli/gateway.py:_print_info_lines */
json_t *gc_print_info_lines(const json_t *lines, const gc_seams_t *s) {
    if (is_arr(lines)) {
        for (size_t i = 0; i < lines->c.count; i++) {
            json_t *args = gc_args0();
            json_set(args, "text", json_copy(lines->c.items[i]));
            json_t *r = gc_call(s, "print_info", args);
            if (gc_is_raise(r)) return r;
            json_free(r);
        }
    }
    return NULL;
}

/* PoP: gc_print_indented @ hermes_cli/gateway.py:_print_indented */
json_t *gc_print_indented(const char *text, const gc_seams_t *s) {
    json_t *ls = gc_splitlines(text ? text : "");
    for (size_t i = 0; i < ls->c.count; i++) {
        char *line = xsf("  %s", jstr(ls->c.items[i]) ? jstr(ls->c.items[i]) : "");
        json_t *args = gc_args0();
        json_set(args, "text", json_string(line));
        free(line);
        json_t *r = gc_call(s, "emit", args);
        if (gc_is_raise(r)) { json_free(ls); return r; }
        json_free(r);
    }
    json_free(ls);
    return NULL;
}

/* PoP: gc_current_profile_name @ hermes_cli/gateway.py:_current_profile_name */
json_t *gc_current_profile_name(const gc_seams_t *s) {
    json_t *home = gc_call(s, "get_hermes_home", gc_args0());
    if (gc_is_raise(home)) return home;
    json_t *args = gc_args0();
    gc_set_str(args, "home", jstr(home));
    json_t *pnfh = gc_call(s, "profile_name_for_home", args);
    json_free(home);
    if (gc_is_raise(pnfh)) return pnfh;
    if (jtruth(pnfh)) return pnfh;
    json_free(pnfh);
    return gc_call(s, "profile_suffix", gc_args0());
}

/* ── systemd flows ── */

/* PoP: gc_systemd_unit_is_active @ hermes_cli/gateway.py:_systemd_unit_is_active */
json_t *gc_systemd_unit_is_active(bool system, const gc_seams_t *s) {
    json_t *a = gc_args0(); gc_set_bool(a, "system", system);
    json_t *up = gc_call(s, "get_systemd_unit_path", a);
    if (gc_is_raise(up)) return up;
    json_t *ea = gc_args0(); gc_set_str(ea, "path", jstr(up));
    json_t *ex = gc_call(s, "path_exists", ea);
    json_free(up);
    if (gc_is_raise(ex)) return ex;
    if (!jtruth(ex)) { json_free(ex); return json_bool(false); }
    json_free(ex);
    json_t *svc = gc_call(s, "get_service_name", gc_args0());
    if (gc_is_raise(svc)) return svc;
    json_t *argv = json_array();
    json_append(argv, json_string("is-active"));
    json_append(argv, json_string(jstr(svc) ? jstr(svc) : ""));
    json_free(svc);
    json_t *ra = gc_args0();
    json_set(ra, "args", argv);
    gc_set_bool(ra, "system", system);
    gc_set_num(ra, "timeout", 10);
    gc_set_bool(ra, "capture_output", true);
    gc_set_bool(ra, "text", true);
    gc_set_str(ra, "encoding", "utf-8");
    gc_set_str(ra, "errors", "replace");
    json_t *result = gc_call(s, "run_systemctl", ra);
    if (gc_is_raise(result)) {
        const char *cls = gc_raise_class(result);
        if (gc_cls_runtimeerror(cls) || gc_cls_timeoutexpired(cls)) {
            json_free(result); return json_bool(false);
        }
        return result;
    }
    char *stripped = gc_strip(json_get_str(result, "stdout", ""));
    bool active = !strcmp(stripped, "active");
    free(stripped); json_free(result);
    return json_bool(active);
}

/* PoP: gc_systemctl_show @ hermes_cli/gateway.py:_systemctl_show */
json_t *gc_systemctl_show(const json_t *properties, bool system, const gc_seams_t *s) {
    json_t *svc = gc_call(s, "get_service_name", gc_args0());
    if (gc_is_raise(svc)) {
        const char *cls = gc_raise_class(svc);
        if (gc_cls_runtimeerror(cls) || gc_cls_timeoutexpired(cls) || gc_cls_oserror(cls)) {
            json_free(svc); return json_object();
        }
        return svc;
    }
    json_t *sa = gc_args0(); gc_set_bool(sa, "system", system);
    json_t *sel = gc_call(s, "select_systemd_scope", sa);
    if (gc_is_raise(sel)) {
        const char *cls = gc_raise_class(sel);
        json_free(svc);
        if (gc_cls_runtimeerror(cls) || gc_cls_timeoutexpired(cls) || gc_cls_oserror(cls)) {
            json_free(sel); return json_object();
        }
        return sel;
    }
    bool sel_system = jtruth(sel);
    json_free(sel);
    /* ",".join(properties) */
    char *props = strdup("");
    if (is_arr(properties)) {
        for (size_t i = 0; i < properties->c.count; i++) {
            const char *p = jstr(properties->c.items[i]);
            char *next = xsf("%s%s%s", props, i ? "," : "", p ? p : "");
            free(props); props = next;
        }
    }
    json_t *argv = json_array();
    json_append(argv, json_string("show"));
    json_append(argv, json_string(jstr(svc) ? jstr(svc) : ""));
    json_append(argv, json_string("--no-pager"));
    json_append(argv, json_string("--property"));
    json_append(argv, json_string(props));
    free(props); json_free(svc);
    json_t *ra = gc_args0();
    json_set(ra, "args", argv);
    gc_set_bool(ra, "system", sel_system);
    gc_set_num(ra, "timeout", 10);
    gc_set_bool(ra, "capture_output", true);
    gc_set_bool(ra, "text", true);
    gc_set_str(ra, "encoding", "utf-8");
    gc_set_str(ra, "errors", "replace");
    json_t *result = gc_call(s, "run_systemctl", ra);
    if (gc_is_raise(result)) {
        const char *cls = gc_raise_class(result);
        if (gc_cls_runtimeerror(cls) || gc_cls_timeoutexpired(cls) || gc_cls_oserror(cls)) {
            json_free(result); return json_object();
        }
        return result;
    }
    double rc = json_get_num(result, "returncode", -1);
    if (rc != 0) { json_free(result); return json_object(); }
    json_t *lines = gc_splitlines(json_get_str(result, "stdout", ""));
    json_free(result);
    json_t *ret = gc_parse_kv_pairs(lines);
    json_free(lines);
    return ret;
}

/* PoP: gc_systemd_unit_installed @ hermes_cli/gateway.py:_systemd_unit_installed */
json_t *gc_systemd_unit_installed(const gc_seams_t *s) {
    json_t *sup = gc_call(s, "supports_systemd_services", gc_args0());
    if (gc_is_raise(sup)) return sup;
    if (!jtruth(sup)) { json_free(sup); return json_bool(false); }
    json_free(sup);
    for (int si = 0; si < 2; si++) {
        bool system = si == 1;
        json_t *a = gc_args0(); gc_set_bool(a, "system", system);
        json_t *up = gc_call(s, "get_systemd_unit_path", a);
        if (gc_is_raise(up)) return up;
        json_t *ea = gc_args0(); gc_set_str(ea, "path", jstr(up));
        json_t *ex = gc_call(s, "path_exists", ea);
        json_free(up);
        if (gc_is_raise(ex)) return ex;
        bool exists = jtruth(ex);
        json_free(ex);
        if (!exists) continue;
        json_t *fa = gc_args0(); gc_set_bool(fa, "system", system);
        json_t *fh = gc_call(s, "foreign_systemd_unit_home", fa);
        if (gc_is_raise(fh)) return fh;
        bool is_none = !jpresent(fh);
        json_free(fh);
        if (is_none) return json_bool(true);
    }
    return json_bool(false);
}

/* PoP: gc_agent_timeout_setting @ hermes_cli/gateway.py:_agent_timeout_setting */
json_t *gc_agent_timeout_setting(const char *env_var, const char *key, const gc_seams_t *s) {
    json_t *ea = gc_args0(); gc_set_str(ea, "name", env_var);
    json_t *env_raw = gc_call(s, "os_getenv", ea);
    if (gc_is_raise(env_raw)) return env_raw;
    if (jpresent(env_raw)) {
        char *raw_s = py_str(env_raw);
        char *stripped = gc_strip(raw_s);
        bool nonempty = stripped[0] != '\0';
        free(raw_s); free(stripped);
        if (nonempty) {
            json_t *pa = gc_args0();
            json_set(pa, "value", json_copy(env_raw));
            json_free(env_raw);
            return gc_call(s, "parse", pa);
        }
    }
    json_free(env_raw);
    json_t *cfg = gc_call(s, "read_raw_config", gc_args0());
    if (gc_is_raise(cfg)) return cfg;
    const json_t *ac = NULL;
    json_t *empty = NULL;
    if (is_obj(cfg)) {
        ac = json_obj_get(cfg, "agent");
        if (!ac) { empty = json_object(); ac = empty; }
    } else { empty = json_object(); ac = empty; }
    json_t *ret;
    if (is_obj(ac) && json_has(ac, key)) {
        const json_t *val = json_obj_get(ac, key);
        json_t *pa = gc_args0();
        if (val && val->type != JSON_NULL) json_set(pa, "value", json_copy(val));
        ret = gc_call(s, "parse", pa);
    } else {
        ret = gc_call(s, "parse", gc_args0());
    }
    json_free(empty); json_free(cfg);
    return ret;
}

/* PoP: gc_systemd_scope_preamble @ hermes_cli/gateway.py:_systemd_scope_preamble */
json_t *gc_systemd_scope_preamble(const char *action, bool system,
                                  bool require_installed, bool preflight_user,
                                  const gc_seams_t *s) {
    json_t *sa = gc_args0(); gc_set_bool(sa, "system", system);
    json_t *sel = gc_call(s, "select_systemd_scope", sa);
    if (gc_is_raise(sel)) return sel;
    bool eff = jtruth(sel);
    json_free(sel);
    if (eff) {
        json_t *a = gc_args0(); gc_set_str(a, "action", action);
        json_t *r = gc_call(s, "require_root_for_system_service", a);
        if (gc_is_raise(r)) return r;
        json_free(r);
    } else if (preflight_user) {
        json_t *r = gc_call(s, "preflight_user_systemd", gc_args0());
        if (gc_is_raise(r)) return r;
        json_free(r);
    }
    if (require_installed) {
        json_t *a = gc_args0();
        gc_set_str(a, "action", action);
        gc_set_bool(a, "system", eff);
        json_t *r = gc_call(s, "require_service_installed", a);
        if (gc_is_raise(r)) return r;
        json_free(r);
    }
    return json_bool(eff);
}

/* PoP: gc_retire_hermes_replace_dropin @ hermes_cli/gateway.py:_retire_hermes_replace_dropin */
json_t *gc_retire_hermes_replace_dropin(bool system, const gc_seams_t *s) {
    json_t *a = gc_args0(); gc_set_bool(a, "system", system);
    json_t *up = gc_call(s, "get_systemd_unit_path", a);
    if (gc_is_raise(up)) return up;
    const char *path = jstr(up) ? jstr(up) : "";
    char *parent = gc_path_parent(path);
    char *name = gc_path_name(path);
    char *dname = xsf("%s.d", name);
    char *d1 = gc_path_join(parent, dname);
    char *dropin = gc_path_join(d1, "20-replace.conf");
    free(parent); free(name); free(dname); free(d1);
    json_t *ra = gc_args0();
    gc_set_str(ra, "path", dropin);
    gc_set_str(ra, "encoding", "utf-8-sig");
    json_t *text = gc_call(s, "read_text", ra);
    if (gc_is_raise(text)) {
        const char *cls = gc_raise_class(text);
        if (gc_cls_oserror(cls)) { json_free(text); free(dropin); json_free(up); return json_bool(false); }
        free(dropin); json_free(up); return text;
    }
    const char *t = jstr(text) ? jstr(text) : "";
    bool all = strstr(t, "Added to end the gateway respawn storm")
            && strstr(t, "--replace") && strstr(t, "ExecStart=");
    json_free(text);
    if (!all) { free(dropin); json_free(up); return json_bool(false); }
    json_t *ua = gc_args0(); gc_set_str(ua, "path", dropin);
    json_t *r = gc_call(s, "unlink", ua);
    free(dropin); json_free(up);
    if (gc_is_raise(r)) return r;
    json_free(r);
    return json_bool(true);
}

/* ── multiplexer / host-attach flows ── */

/* Propagate a BaseException raise out of a try region; route a plain Exception
 * raise to the function's `catch:` handler. `var` is nulled when it escapes as
 * ret so the cleanup label does not double-free it. */
#define GC_TRY_RAISE(var) do { \
    if (gc_is_raise(var)) { \
        if (gc_is_base_exc(gc_raise_class(var))) { ret = (var); (var) = NULL; goto done; } \
        goto catch; \
    } \
} while (0)

/* PoP: gc_host_multiplexer_serving @ hermes_cli/gateway.py:host_multiplexer_serving */
json_t *gc_host_multiplexer_serving(const char *profile_name, const gc_seams_t *s) {
    json_t *name = NULL, *gateway = NULL, *home = NULL, *desc = NULL, *r = NULL, *ret = NULL;
    if (profile_name) name = json_string(profile_name);
    else {
        name = gc_current_profile_name(s);
        GC_TRY_RAISE(name);
    }
    const char *nm = jtruth(name) ? jstr(name) : "default";
    json_t *a = gc_args0(); gc_set_str(a, "name", nm);
    gateway = gc_call(s, "host_gateway_serving", a);
    GC_TRY_RAISE(gateway);
    if (!jpresent(gateway)) { ret = NULL; goto done; }
    home = gc_call(s, "get_hermes_home", gc_args0());
    GC_TRY_RAISE(home);
    json_t *la = gc_args0();
    gc_set_str(la, "owner_home", json_get_str(gateway, "home", NULL));
    gc_set_str(la, "our_home", jstr(home));
    r = gc_call(s, "launched_by_other_tenant", la);
    GC_TRY_RAISE(r);
    bool other = jtruth(r); json_free(r); r = NULL;
    if (other) {
        desc = gc_call(s, "gateway.describe", gc_args0());
        GC_TRY_RAISE(desc);
        json_t *da = gc_args0();
        gc_set_str(da, "msg", "Host gateway %s belongs to another Hermes home; not ours");
        json_t *arr = json_array();
        json_append(arr, json_string(jstr(desc) ? jstr(desc) : ""));
        json_set(da, "args", arr);
        r = gc_call(s, "logger.debug", da);
        GC_TRY_RAISE(r);
        json_free(r); r = NULL;
        ret = NULL; goto done;
    }
    ret = json_copy(gateway); goto done;
catch:
    {
        json_t *da = gc_args0();
        gc_set_str(da, "msg", "Host multiplexer probe failed");
        gc_set_bool(da, "exc_info", true);
        json_t *lr = gc_call(s, "logger.debug", da);
        if (gc_is_raise(lr)) { ret = lr; goto done; }
        json_free(lr);
        ret = NULL;
    }
done:
    json_free(name); json_free(gateway); json_free(home); json_free(desc); json_free(r);
    return ret;
}

/* PoP: gc_served_by_another_host_gateway @ hermes_cli/gateway.py:_served_by_another_host_gateway */
json_t *gc_served_by_another_host_gateway(const char *profile_name, const gc_seams_t *s) {
    json_t *gateway = gc_host_multiplexer_serving(profile_name, s);
    if (gc_is_raise(gateway)) return gateway;
    if (!jpresent(gateway)) return NULL;
    json_t *ph = NULL, *sm = NULL, *ret = NULL;
    ph = gc_call(s, "get_process_hermes_home", gc_args0());
    if (gc_is_raise(ph)) {
        if (gc_is_base_exc(gc_raise_class(ph))) { ret = ph; ph = NULL; goto done; }
        goto catch;
    }
    json_t *sa = gc_args0();
    gc_set_str(sa, "left", json_get_str(gateway, "home", NULL));
    gc_set_str(sa, "right", jstr(ph));
    sm = gc_call(s, "same_hermes_home", sa);
    if (gc_is_raise(sm)) {
        if (gc_is_base_exc(gc_raise_class(sm))) { ret = sm; sm = NULL; goto done; }
        goto catch;
    }
    bool same = jtruth(sm);
    json_free(sm); sm = NULL; json_free(ph); ph = NULL;
    if (same) { ret = NULL; goto done; }
    ret = gateway; gateway = NULL; goto done;
catch:
    {
        json_t *da = gc_args0();
        gc_set_str(da, "msg", "Host multiplexer home comparison failed");
        gc_set_bool(da, "exc_info", true);
        json_t *lr = gc_call(s, "logger.debug", da);
        if (gc_is_raise(lr)) { ret = lr; goto done; }
        json_free(lr);
        ret = gateway; gateway = NULL;
    }
done:
    json_free(gateway); json_free(ph); json_free(sm);
    return ret;
}

/* PoP: gc_host_multiplexer_for_all_verb @ hermes_cli/gateway.py:_host_multiplexer_for_all_verb */
json_t *gc_host_multiplexer_for_all_verb(const gc_seams_t *s) {
    json_t *g = gc_call(s, "host_gateway", gc_args0());
    if (gc_is_raise(g)) {
        if (gc_is_base_exc(gc_raise_class(g))) return g;
        json_free(g);
        json_t *da = gc_args0();
        gc_set_str(da, "msg", "Host gateway probe failed");
        gc_set_bool(da, "exc_info", true);
        json_t *lr = gc_call(s, "logger.debug", da);
        if (gc_is_raise(lr)) return lr;
        json_free(lr);
        return NULL;
    }
    return g;
}

/* PoP: gc_host_multiplexer_is_ours @ hermes_cli/gateway.py:_host_multiplexer_is_ours */
json_t *gc_host_multiplexer_is_ours(const json_t *owner, const gc_seams_t *s) {
    if (!jpresent(owner)) return json_bool(false);  /* owner.home AttributeError -> except */
    json_t *ph = gc_call(s, "get_process_hermes_home", gc_args0());
    if (gc_is_raise(ph)) {
        if (gc_is_base_exc(gc_raise_class(ph))) return ph;
        json_free(ph); return json_bool(false);
    }
    json_t *sa = gc_args0();
    gc_set_str(sa, "left", json_get_str(owner, "home", NULL));
    gc_set_str(sa, "right", jstr(ph));
    json_t *sm = gc_call(s, "same_hermes_home", sa);
    json_free(ph);
    if (gc_is_raise(sm)) {
        if (gc_is_base_exc(gc_raise_class(sm))) return sm;
        json_free(sm); return json_bool(false);
    }
    json_t *ret = json_bool(jtruth(sm));
    json_free(sm);
    return ret;
}

/* PoP: gc_print_unfolded_gateway_note @ hermes_cli/gateway.py:_print_unfolded_gateway_note */
json_t *gc_print_unfolded_gateway_note(const json_t *owner, const gc_seams_t *s) {
    /* The comprehension evaluates the iterable find_gateway_pids(all_profiles=True)
     * FIRST; owner.pid is read lazily in the filter afterwards. So the find seam
     * fires even when owner is None (the AttributeError/TypeError is caught). */
    json_t *fa = gc_args0(); gc_set_bool(fa, "all_profiles", true);
    json_t *pids = gc_call(s, "find_gateway_pids", fa);
    if (gc_is_raise(pids)) {
        if (gc_is_base_exc(gc_raise_class(pids))) return pids;
        json_free(pids); return NULL;
    }
    if (!jpresent(owner)) { json_free(pids); return NULL; }
    double owner_pid = json_get_num(owner, "pid", 0);
    json_t *others = json_array();
    if (is_arr(pids)) {
        for (size_t i = 0; i < pids->c.count; i++) {
            const json_t *p = pids->c.items[i];
            if (is_num(p) && p->num_val != owner_pid) json_append(others, json_copy(p));
        }
    }
    json_free(pids);
    if (others->c.count == 0) { json_free(others); return NULL; }
    char *joined = strdup("");
    for (size_t i = 0; i < others->c.count; i++) {
        char *ps = py_str(others->c.items[i]);
        char *next = xsf("%s%s%s", joined, i ? ", " : "", ps);
        free(ps); free(joined); joined = next;
    }
    char *line1 = xsf("  %zu per-profile gateway process(es) still run beside it (PIDs: %s).",
                      others->c.count, joined);
    free(joined); json_free(others);
    json_t *a1 = gc_args0(); gc_set_str(a1, "text", line1); free(line1);
    json_t *r1 = gc_call(s, "print", a1);
    if (gc_is_raise(r1)) return r1;
    json_free(r1);
    json_t *a2 = gc_args0();
    gc_set_str(a2, "text", "  They were left running; fold them in with: hermes gateway migrate --multiplex");
    json_t *r2 = gc_call(s, "print", a2);
    if (gc_is_raise(r2)) return r2;
    json_free(r2);
    return NULL;
}

/* PoP: gc_stop_host_multiplexer @ hermes_cli/gateway.py:_stop_host_multiplexer */
json_t *gc_stop_host_multiplexer(const json_t *owner, const gc_seams_t *s) {
    json_t *stopped = gc_call(s, "kill_gateway_processes", gc_args0());
    if (gc_is_raise(stopped)) return stopped;
    if (jtruth(stopped)) return stopped;
    json_free(stopped);
    if (!jpresent(owner))
        return gc_raise("AttributeError", "'NoneType' object has no attribute 'pid'");
    json_t *ta = gc_args0();
    gc_set_num(ta, "pid", json_get_num(owner, "pid", 0));
    gc_set_bool(ta, "force", false);
    json_t *r = gc_call(s, "terminate_pid", ta);
    if (gc_is_raise(r)) {
        const char *cls = gc_raise_class(r);
        if (!strcmp(cls, "ProcessLookupError") || !strcmp(cls, "PermissionError") || gc_cls_oserror(cls)) {
            json_free(r); return json_number(0);
        }
        return r;
    }
    json_free(r);
    return json_number(1);
}

/* PoP: gc_print_multiplex_standalone_reason @ hermes_cli/gateway.py:_print_multiplex_standalone_reason */
json_t *gc_print_multiplex_standalone_reason(const gc_seams_t *s) {
    json_t *lines = gc_call(s, "recorded_standalone_warning_lines", gc_args0());
    if (gc_is_raise(lines)) return lines;
    if (is_arr(lines)) {
        for (size_t i = 0; i < lines->c.count; i++) {
            json_t *a = gc_args0();
            json_set(a, "text", json_copy(lines->c.items[i]));
            json_t *r = gc_call(s, "print", a);
            if (gc_is_raise(r)) { json_free(lines); return r; }
            json_free(r);
        }
    }
    json_free(lines);
    return NULL;
}

/* PoP: gc_print_served_ingress_urls @ hermes_cli/gateway.py:_print_served_ingress_urls */
json_t *gc_print_served_ingress_urls(const char *profile, const gc_seams_t *s) {
    json_t *ua = gc_args0(); gc_set_str(ua, "profile", profile);
    json_t *urls = gc_call(s, "served_profile_ingress_urls", ua);
    if (gc_is_raise(urls)) {
        if (gc_is_base_exc(gc_raise_class(urls))) return urls;
        json_free(urls); return NULL;
    }
    if (!jtruth(urls) || !is_obj(urls)) { json_free(urls); return NULL; }
    json_t *r = gc_call(s, "print", gc_args0());
    if (gc_is_raise(r)) { json_free(urls); return r; }
    json_free(r);
    json_t *ha = gc_args0();
    gc_set_str(ha, "text", "Inbound callback URLs on the shared listener:");
    r = gc_call(s, "print", ha);
    if (gc_is_raise(r)) { json_free(urls); return r; }
    json_free(r);
    /* sorted(urls.items()) by key */
    size_t n = urls->c.count;
    const char **keys = malloc(n * sizeof(char *));
    for (size_t i = 0; i < n; i++) keys[i] = urls->c.keys[i];
    for (size_t i = 0; i < n; i++)
        for (size_t j = i + 1; j < n; j++)
            if (strcmp(keys[j], keys[i]) < 0) { const char *t = keys[i]; keys[i] = keys[j]; keys[j] = t; }
    bool has_profile = profile && profile[0];
    for (size_t i = 0; i < n; i++) {
        const char *name = keys[i];
        const json_t *per = json_obj_get(urls, name);
        char *indent = has_profile ? strdup("  ") : xsf("  %s/", name);
        json_t *fa = gc_args0();
        gc_set_node(fa, "per_platform", per);
        gc_set_str(fa, "indent", indent);
        free(indent);
        json_t *flines = gc_call(s, "format_ingress_url_lines", fa);
        if (gc_is_raise(flines)) { free(keys); json_free(urls); return flines; }
        if (is_arr(flines)) {
            for (size_t k = 0; k < flines->c.count; k++) {
                json_t *pa = gc_args0();
                json_set(pa, "text", json_copy(flines->c.items[k]));
                json_t *pr = gc_call(s, "print", pa);
                if (gc_is_raise(pr)) { json_free(flines); free(keys); json_free(urls); return pr; }
                json_free(pr);
            }
        }
        json_free(flines);
    }
    free(keys); json_free(urls);
    return NULL;
}
json_t *gc_named_profile_refused_under_multiplexer(bool force, const gc_seams_t *s);


/* ── print emission helpers (return the seam result: raise spec or NULL) ── */
static json_t *gc_print_text(const gc_seams_t *s, const char *text) {
    json_t *a = gc_args0();
    if (text) gc_set_str(a, "text", text);
    return gc_call(s, "print", a);
}
static json_t *gc_emit_named(const gc_seams_t *s, const char *name, const char *text) {
    json_t *a = gc_args0(); gc_set_str(a, "text", text);
    return gc_call(s, name, a);
}
#define GC_PROP(var) do { if (gc_is_raise(var)) { ret = (var); (var) = NULL; goto done; } } while (0)

/* PoP: gc_served_profile_needs_no_service @ hermes_cli/gateway.py:_served_profile_needs_no_service */
json_t *gc_served_profile_needs_no_service(const gc_seams_t *s) {
    json_t *ret = NULL, *r = NULL, *home = NULL, *cpn = NULL, *msg = NULL;
    r = gc_call(s, "named_profile_served", gc_args0());
    GC_PROP(r);
    bool served = jtruth(r); json_free(r); r = NULL;
    if (!served) {
        ret = gc_named_profile_refused_under_multiplexer(false, s);
        goto done;
    }
    home = gc_call(s, "get_hermes_home", gc_args0());
    GC_PROP(home);
    json_t *pa = gc_args0(); gc_set_str(pa, "home", jstr(home));
    r = gc_call(s, "profile_is_standalone", pa);
    GC_PROP(r);
    bool standalone = jtruth(r); json_free(r); r = NULL;
    if (standalone) {
        cpn = gc_current_profile_name(s);
        GC_PROP(cpn);
        json_t *ma = gc_args0(); gc_set_str(ma, "profile", jstr(cpn));
        msg = gc_call(s, "standalone_rescan_message", ma);
        GC_PROP(msg);
        r = gc_emit_named(s, "print_info", jstr(msg) ? jstr(msg) : "");
        GC_PROP(r);
        json_free(r); r = NULL;
        ret = json_bool(true); goto done;
    }
    cpn = gc_current_profile_name(s);
    GC_PROP(cpn);
    char *succ = xsf("Profile '%s' is already served by the default multiplexer.",
                     jstr(cpn) ? jstr(cpn) : "");
    r = gc_emit_named(s, "print_success", succ);
    free(succ);
    GC_PROP(r);
    json_free(r); r = NULL;
    r = gc_emit_named(s, "print_info", "  (served now by the running multiplexed gateway \xe2\x80\x94 add its bot token and it connects)");
    GC_PROP(r);
    json_free(r); r = NULL;
    r = gc_emit_named(s, "print_info", "  No standalone gateway service was installed or started.");
    GC_PROP(r);
    json_free(r); r = NULL;
    ret = json_bool(true);
done:
    json_free(r); json_free(home); json_free(cpn); json_free(msg);
    return ret;
}

/* PoP: gc_named_profile_refused_under_multiplexer @ hermes_cli/gateway.py:_named_profile_refused_under_multiplexer */
json_t *gc_named_profile_refused_under_multiplexer(bool force, const gc_seams_t *s) {
    if (force) return json_bool(false);
    json_t *ret = NULL, *suffix = NULL, *owner = NULL, *r = NULL;
    json_t *home1 = NULL, *pnfh1 = NULL, *home2 = NULL, *pis = NULL;
    json_t *home3 = NULL, *pnfh2 = NULL, *isi = NULL, *desc = NULL, *home4 = NULL, *dh = NULL;
    bool standalone = false, new_standalone = false, served = false;
    /* try: suffix / standalone / new_standalone */
    suffix = gc_current_profile_name(s);
    if (gc_is_raise(suffix)) {
        if (gc_is_base_exc(gc_raise_class(suffix))) { ret = suffix; suffix = NULL; goto done; }
        ret = json_bool(false); goto done;
    }
    home1 = gc_call(s, "get_hermes_home", gc_args0());
    if (gc_is_raise(home1)) { if (gc_is_base_exc(gc_raise_class(home1))) { ret = home1; home1 = NULL; goto done; } ret = json_bool(false); goto done; }
    json_t *a1 = gc_args0(); gc_set_str(a1, "home", jstr(home1));
    pnfh1 = gc_call(s, "profile_name_for_home", a1);
    if (gc_is_raise(pnfh1)) { if (gc_is_base_exc(gc_raise_class(pnfh1))) { ret = pnfh1; pnfh1 = NULL; goto done; } ret = json_bool(false); goto done; }
    bool cond1 = jpresent(pnfh1) && strcmp(jstr(pnfh1) ? jstr(pnfh1) : "", "default") != 0;
    if (cond1) {
        home2 = gc_call(s, "get_hermes_home", gc_args0());
        if (gc_is_raise(home2)) { if (gc_is_base_exc(gc_raise_class(home2))) { ret = home2; home2 = NULL; goto done; } ret = json_bool(false); goto done; }
        json_t *a2 = gc_args0(); gc_set_str(a2, "home", jstr(home2));
        pis = gc_call(s, "profile_is_standalone", a2);
        if (gc_is_raise(pis)) { if (gc_is_base_exc(gc_raise_class(pis))) { ret = pis; pis = NULL; goto done; } ret = json_bool(false); goto done; }
        standalone = jtruth(pis);
    }
    home3 = gc_call(s, "get_hermes_home", gc_args0());
    if (gc_is_raise(home3)) { if (gc_is_base_exc(gc_raise_class(home3))) { ret = home3; home3 = NULL; goto done; } ret = json_bool(false); goto done; }
    json_t *a3 = gc_args0(); gc_set_str(a3, "home", jstr(home3));
    pnfh2 = gc_call(s, "profile_name_for_home", a3);
    if (gc_is_raise(pnfh2)) { if (gc_is_base_exc(gc_raise_class(pnfh2))) { ret = pnfh2; pnfh2 = NULL; goto done; } ret = json_bool(false); goto done; }
    bool cond2 = jpresent(pnfh2) && strcmp(jstr(pnfh2) ? jstr(pnfh2) : "", "default") != 0;
    if (cond2) {
        isi = gc_call(s, "is_service_installed", gc_args0());
        if (gc_is_raise(isi)) { if (gc_is_base_exc(gc_raise_class(isi))) { ret = isi; isi = NULL; goto done; } ret = json_bool(false); goto done; }
        new_standalone = !jtruth(isi);
    }
    /* except Exception: return False  (handled above per seam) */
    const char *suf = jstr(suffix) ? jstr(suffix) : "";
    owner = gc_served_by_another_host_gateway(NULL, s);
    GC_PROP(owner);
    served = jpresent(owner);
    if (!served) {
        r = gc_call(s, "named_profile_served", gc_args0());
        GC_PROP(r);
        served = jtruth(r); json_free(r); r = NULL;
    }
    if (standalone) {
        if (!served) { ret = json_bool(false); goto done; }
        json_t *ma = gc_args0(); gc_set_str(ma, "profile", suf);
        r = gc_call(s, "standalone_rescan_message", ma);
        GC_PROP(r);
        json_t *er = gc_emit_named(s, "print_error", jstr(r) ? jstr(r) : "");
        json_free(r); r = NULL;
        GC_PROP(er);
        json_free(er);
        ret = json_bool(true); goto done;
    }
    if (!served && !new_standalone) { ret = json_bool(false); goto done; }
    if (served) {
        char *m = xsf("The host gateway already serves profile '%s'.", suf);
        r = gc_emit_named(s, "print_error", m); free(m);
        GC_PROP(r); json_free(r); r = NULL;
        if (jpresent(owner)) {
            desc = gc_call(s, "gateway.describe", gc_args0());
            GC_PROP(desc);
            char *dl = xsf("  %s", jstr(desc) ? jstr(desc) : "");
            r = gc_print_text(s, dl); free(dl);
            GC_PROP(r); json_free(r); r = NULL;
        }
    } else {
        char *m = xsf("Profile '%s' does not get a gateway of its own.", suf);
        r = gc_emit_named(s, "print_error", m); free(m);
        GC_PROP(r); json_free(r); r = NULL;
    }
    r = gc_print_text(s,
        "  Exactly one gateway per host is the inbound process for every\n"
        "  profile. Starting a separate gateway for this profile would\n"
        "  double-bind its platforms (two pollers on one bot token, port\n"
        "  conflicts).\n");
    GC_PROP(r); json_free(r); r = NULL;
    if (served) {
        r = gc_print_text(s, "  Manage the host gateway instead:"); GC_PROP(r); json_free(r); r = NULL;
        r = gc_print_text(s, NULL); GC_PROP(r); json_free(r); r = NULL;
        const char *label = jpresent(owner) ? json_get_str(owner, "profile_label", "default") : "default";
        char *m = xsf("    hermes -p %s gateway restart", label);
        r = gc_print_text(s, m); free(m); GC_PROP(r); json_free(r); r = NULL;
    } else {
        r = gc_print_text(s, "  Install or start the host gateway from the default profile; it serves this one too:"); GC_PROP(r); json_free(r); r = NULL;
        r = gc_print_text(s, NULL); GC_PROP(r); json_free(r); r = NULL;
        r = gc_print_text(s, "    hermes gateway install"); GC_PROP(r); json_free(r); r = NULL;
        r = gc_print_text(s, NULL); GC_PROP(r); json_free(r); r = NULL;
        r = gc_print_text(s, "  Or fold an existing per-profile fleet onto one host gateway:"); GC_PROP(r); json_free(r); r = NULL;
        r = gc_print_text(s, NULL); GC_PROP(r); json_free(r); r = NULL;
        r = gc_print_text(s, "    hermes gateway migrate --multiplex"); GC_PROP(r); json_free(r); r = NULL;
    }
    r = gc_print_text(s, NULL); GC_PROP(r); json_free(r); r = NULL;
    r = gc_print_text(s, "  A separate per-profile gateway (for a fleet split across UNIX users or a"); GC_PROP(r); json_free(r); r = NULL;
    {
        char *m = xsf("  HERMES_HOME outside profiles/) needs --force:  hermes -p %s gateway install --force", suf);
        r = gc_print_text(s, m); free(m); GC_PROP(r); json_free(r); r = NULL;
    }
    r = gc_print_text(s, NULL); GC_PROP(r); json_free(r); r = NULL;
    r = gc_print_text(s, "  Temporary compatibility path while multiplexing gaps are closed: set"); GC_PROP(r); json_free(r); r = NULL;
    home4 = gc_call(s, "get_hermes_home", gc_args0());
    GC_PROP(home4);
    json_t *da = gc_args0(); gc_set_str(da, "home", jstr(home4));
    dh = gc_call(s, "display_hermes_home", da);
    GC_PROP(dh);
    {
        char *m = xsf("  gateway.standalone: true in %s/config.yaml,", jstr(dh) ? jstr(dh) : "");
        r = gc_print_text(s, m); free(m); GC_PROP(r); json_free(r); r = NULL;
    }
    r = gc_print_text(s, "  then wait for the host gateway to rescan (<=30s) or send its rescan-profiles control verb."); GC_PROP(r); json_free(r); r = NULL;
    {
        char *m = xsf("  (%s)", "gateway.standalone is a temporary compatibility shim while multiplexing gaps are fixed; it will be removed once they are \xe2\x80\x94 plan to fold this profile with `hermes gateway migrate --multiplex`.");
        r = gc_print_text(s, m); free(m); GC_PROP(r); json_free(r); r = NULL;
    }
    ret = json_bool(true);
done:
    json_free(suffix); json_free(owner); json_free(r);
    json_free(home1); json_free(pnfh1); json_free(home2); json_free(pis);
    json_free(home3); json_free(pnfh2); json_free(isi); json_free(desc);
    json_free(home4); json_free(dh);
    return ret;
}

/* PoP: gc_attach_to_host_gateway_or_guard @ hermes_cli/gateway.py:_attach_to_host_gateway_or_guard */
json_t *gc_attach_to_host_gateway_or_guard(bool force, bool replace, const gc_seams_t *s) {
    json_t *ret = NULL, *home = NULL, *decision = NULL, *r = NULL;
    if (force) { ret = NULL; goto done; }
    home = gc_call(s, "get_hermes_home", gc_args0());
    if (gc_is_raise(home)) {
        if (gc_is_base_exc(gc_raise_class(home))) { ret = home; home = NULL; goto done; }
        goto probe_catch;
    }
    json_t *da = gc_args0();
    gc_set_str(da, "home", jstr(home));
    gc_set_bool(da, "replace", replace);
    decision = gc_call(s, "decide", da);
    if (gc_is_raise(decision)) {
        if (gc_is_base_exc(gc_raise_class(decision))) { ret = decision; decision = NULL; goto done; }
        goto probe_catch;
    }
    goto after_probe;
probe_catch:
    {
        json_free(decision); decision = NULL;
        json_t *la = gc_args0();
        gc_set_str(la, "msg", "Host gateway attach probe failed");
        gc_set_bool(la, "exc_info", true);
        r = gc_call(s, "logger.debug", la);
        if (gc_is_raise(r)) { ret = r; r = NULL; goto done; }
        json_free(r); r = NULL;
    }
after_probe:
    {
        const char *outcome = is_obj(decision) ? json_get_str(decision, "outcome", "") : "";
        const char *message = is_obj(decision) ? json_get_str(decision, "message", "") : "";
        if (jpresent(decision) && !strcmp(outcome, "replace-host")) { ret = NULL; goto done; }
        if (jpresent(decision) && (!strcmp(outcome, "attach") || !strcmp(outcome, "refuse"))) {
            r = gc_print_text(s, message);
            GC_PROP(r); json_free(r); r = NULL;
            if (!strcmp(outcome, "refuse")) {
                json_t *code = gc_host_decision_exit_code(decision);
                double c = code->num_val; json_free(code);
                json_t *wa = gc_args0();
                gc_set_str(wa, "msg", "gateway run refused (exit %d): %s");
                json_t *arr = json_array();
                json_append(arr, json_number(c));
                json_append(arr, json_string(message));
                json_set(wa, "args", arr);
                r = gc_call(s, "logger.warning", wa);
                GC_PROP(r); json_free(r); r = NULL;
                char *cs = py_float_str(c);
                ret = gc_raise("SystemExit", cs); free(cs);
                goto done;
            }
            r = gc_call(s, "running_under_gateway_supervisor", gc_args0());
            GC_PROP(r);
            bool sup = jtruth(r); json_free(r); r = NULL;
            json_t *code = gc_host_decision_exit_code(decision);
            double c = sup ? code->num_val : 0.0;
            json_free(code);
            char *cs = py_float_str(c);
            ret = gc_raise("SystemExit", cs); free(cs);
            goto done;
        }
        /* No host record: config-derived refusal still applies. */
        json_t *ga = gc_args0(); gc_set_bool(ga, "force", force);
        r = gc_call(s, "guard_named_profile_under_multiplexer", ga);
        GC_PROP(r); json_free(r); r = NULL;
        ret = NULL;
    }
done:
    json_free(home); json_free(decision); json_free(r);
    return ret;
}

/* Build the exc projection passed to _systemd_error_indicates_start_limit. */
static json_t *gc_exc_projection(const json_t *spec) {
    json_t *e = json_object();
    gc_set_str(e, "__raise__", gc_raise_class(spec));
    gc_set_str(e, "msg", json_get_str(spec, "msg", ""));
    const json_t *rc = json_obj_get(spec, "returncode");
    if (rc && rc->type != JSON_NULL) json_set(e, "returncode", json_copy(rc));
    const json_t *cmd = json_obj_get(spec, "cmd");
    if (cmd && cmd->type != JSON_NULL) json_set(e, "cmd", json_copy(cmd));
    return e;
}

/* PoP: gc_systemd_reset_and_run @ hermes_cli/gateway.py:_systemd_reset_and_run */
json_t *gc_systemd_reset_and_run(const char *action, bool system,
                                 const json_t *previous_pid, const gc_seams_t *s) {
    json_t *ret = NULL, *svc = NULL, *r = NULL, *result = NULL, *scope = NULL;
    svc = gc_call(s, "get_service_name", gc_args0());
    GC_PROP(svc);
    const char *sv = jstr(svc) ? jstr(svc) : "";
    /* reset-failed (check=False, timeout=30) — outside try */
    json_t *argv1 = json_array();
    json_append(argv1, json_string("reset-failed"));
    json_append(argv1, json_string(sv));
    json_t *ra1 = gc_args0();
    json_set(ra1, "args", argv1);
    gc_set_bool(ra1, "system", system);
    gc_set_bool(ra1, "check", false);
    gc_set_num(ra1, "timeout", 30);
    r = gc_call(s, "run_systemctl", ra1);
    GC_PROP(r); json_free(r); r = NULL;
    /* action (check=True, timeout=90) — inside try */
    json_t *argv2 = json_array();
    json_append(argv2, json_string(action ? action : ""));
    json_append(argv2, json_string(sv));
    json_t *ra2 = gc_args0();
    json_set(ra2, "args", argv2);
    gc_set_bool(ra2, "system", system);
    gc_set_bool(ra2, "check", true);
    gc_set_num(ra2, "timeout", 90);
    result = gc_call(s, "run_systemctl", ra2);
    if (gc_is_raise(result)) {
        const char *cls = gc_raise_class(result);
        if (!strcmp(cls, "CalledProcessError")) {
            json_t *ea = gc_args0();
            json_set(ea, "exc", gc_exc_projection(result));
            json_t *se = gc_call(s, "systemd_error_indicates_start_limit", ea);
            GC_PROP(se);
            bool limit = jtruth(se); json_free(se);
            if (!limit) {
                json_t *sa = gc_args0(); gc_set_bool(sa, "system", system);
                json_t *ssl = gc_call(s, "systemd_service_is_start_limited", sa);
                GC_PROP(ssl);
                limit = jtruth(ssl); json_free(ssl);
            }
            if (limit) {
                json_t *pa = gc_args0(); gc_set_bool(pa, "system", system);
                r = gc_call(s, "print_systemd_start_limit_wait", pa);
                GC_PROP(r); json_free(r); r = NULL;
                ret = NULL; goto done;
            }
            ret = result; result = NULL; goto done;  /* re-raise exc */
        }
        if (gc_cls_timeoutexpired(cls)) {
            json_t *sa = gc_args0(); gc_set_bool(sa, "system", system);
            scope = gc_call(s, "service_scope_label", sa);
            GC_PROP(scope);
            char *m = xsf("Gateway %s service is still restarting after 90s; "
                          "check `hermes gateway status` or logs for final state.",
                          jstr(scope) ? jstr(scope) : "");
            r = gc_print_text(s, m); free(m);
            GC_PROP(r); json_free(r); r = NULL;
            ret = NULL; goto done;
        }
        ret = result; result = NULL; goto done;  /* other raise propagates */
    }
    json_free(result); result = NULL;
    /* success: wait for relaunch */
    json_t *wa = gc_args0();
    gc_set_bool(wa, "system", system);
    gc_set_node(wa, "previous_pid", previous_pid);
    r = gc_call(s, "wait_for_systemd_service_restart", wa);
    GC_PROP(r); json_free(r); r = NULL;
    ret = NULL;
done:
    json_free(svc); json_free(r); json_free(result); json_free(scope);
    return ret;
}
