/*
 * t_port_gw_service_lifecycle.c — oracle harness for the hermes_cli/gateway.py
 * service-lifecycle FLOW cores ported in src/cli/port_gw_service_lifecycle.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * Wires each flow's seams to the SCRIPTED results in "in" (recording stubs that
 * append one trace event per seam call, in call order) and emits
 * {"func": ..., "in": ..., "out": {"ret": ..., "exit": ..., "raised": ...,
 * "trace": [...]}} per case. tests/oracle_gw_service_lifecycle.py drives the
 * LIVE Python with the same scripts.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hermes_json.h"
#include "port_gw_service_lifecycle.h"

typedef struct {
    const json_t *in;
    gwsl_trace_t *trace;
    size_t gw_idx, systemd_idx, systemd_status_idx, launchd_idx, launchd_install_idx,
           launchd_status_idx, guard_idx, sessions_idx, dispatch_idx, dispatch_all_idx,
           install_cli_idx, refuse_restart_idx, start_repl_idx, reap_idx;
} script_ctx_t;

/* ── event builders ── */
static json_t *ev(const char *name) {
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    return e;
}
static void evs(json_t *e, const char *k, const char *v) { json_set(e, k, v ? json_string(v) : json_null()); }
static void evb(json_t *e, const char *k, bool v) { json_set(e, k, json_bool(v)); }
static void evn(json_t *e, const char *k, double v) { json_set(e, k, json_number(v)); }
static void evj(json_t *e, const char *k, const json_t *v) { json_set(e, k, v ? json_copy(v) : json_null()); }
static void rec(script_ctx_t *c, json_t *e) { gwsl_trace_add(c->trace, e); }

static bool in_bool(const script_ctx_t *c, const char *k) { return json_get_bool(c->in, k, false); }
static const char *in_str(const script_ctx_t *c, const char *k, const char *def) { return json_get_str(c->in, k, def); }

/* Scripted object sequence: array consumed in call order (last repeats); a
 * single object is constant; absent -> NULL. */
static const json_t *next_obj(script_ctx_t *c, const char *k, size_t *idx) {
    const json_t *v = json_obj_get(c->in, k);
    if (!v) return NULL;
    if (v->type == JSON_OBJECT) return v;
    if (v->type == JSON_ARRAY && v->c.count > 0) {
        size_t i = *idx; if (i >= v->c.count) i = v->c.count - 1; (*idx)++;
        const json_t *it = v->c.items[i];
        return (it && it->type == JSON_OBJECT) ? it : NULL;
    }
    return NULL;
}
/* Scripted string sequence (array consume / single constant). */
static const char *next_str(script_ctx_t *c, const char *k, size_t *idx, const char *def) {
    const json_t *v = json_obj_get(c->in, k);
    if (!v) return def;
    if (v->type == JSON_STRING) return v->str_val;
    if (v->type == JSON_ARRAY && v->c.count > 0) {
        size_t i = *idx; if (i >= v->c.count) i = v->c.count - 1; (*idx)++;
        const json_t *it = v->c.items[i];
        return (it && it->type == JSON_STRING) ? it->str_val : def;
    }
    return def;
}
static double next_num(script_ctx_t *c, const char *k, size_t *idx, double def) {
    const json_t *v = json_obj_get(c->in, k);
    if (!v) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_ARRAY && v->c.count > 0) {
        size_t i = *idx; if (i >= v->c.count) i = v->c.count - 1; (*idx)++;
        const json_t *it = v->c.items[i];
        return (it && it->type == JSON_NUMBER) ? it->num_val : def;
    }
    return def;
}
/* Apply a script object's exception/exit to *exc; return the raised type name
 * (NULL when none). "exit" -> SystemExit(code); "exc" -> named exception. */
static const char *apply_exc(const json_t *script, gwsl_exc_t *exc) {
    exc->kind = GWSL_EXC_NONE; exc->exit_code = 0;
    if (!script) return NULL;
    const json_t *ex = json_obj_get(script, "exit");
    if (ex && ex->type == JSON_NUMBER) {
        exc->kind = GWSL_EXC_SYSTEM_EXIT; exc->exit_code = (int)ex->num_val;
        return "SystemExit";
    }
    const char *t = json_get_str(script, "exc", NULL);
    if (t && *t) { exc->kind = gwsl_exc_kind_from_name(t); return t; }
    return NULL;
}
static void rec_raised(json_t *e, const char *raised) {
    if (raised) evs(e, "raised", raised);
    else json_set(e, "ret", json_null());
}

/* ── bool predicate seams ── */
#define BOOL_SEAM(fname, evname, key) \
    static bool fname(void *ctx) { \
        script_ctx_t *c = ctx; bool r = in_bool(c, key); \
        json_t *e = ev(evname); evb(e, "ret", r); rec(c, e); return r; \
    }
BOOL_SEAM(sf_supports_systemd, "supports_systemd", "supports_systemd")
BOOL_SEAM(sf_is_macos, "is_macos", "is_macos")
BOOL_SEAM(sf_is_windows, "is_windows", "is_windows")
BOOL_SEAM(sf_is_wsl, "is_wsl", "is_wsl")
BOOL_SEAM(sf_is_container, "is_container", "is_container")
BOOL_SEAM(sf_is_termux, "is_termux", "is_termux")
BOOL_SEAM(sf_is_apt_termux, "is_apt_termux_install", "is_apt_termux")
BOOL_SEAM(sf_is_supervised, "is_supervised_gateway_process", "is_supervised")
BOOL_SEAM(sf_systemd_unit_installed, "systemd_unit_installed", "systemd_unit_installed")
BOOL_SEAM(sf_launchd_plist_exists, "launchd_plist_exists", "launchd_plist_exists")
BOOL_SEAM(sf_is_managed, "is_managed", "is_managed")
BOOL_SEAM(sf_named_served, "named_profile_served_by_running_multiplexer", "named_served")
BOOL_SEAM(sf_stop_profile, "stop_profile_gateway", "stop_profile")
BOOL_SEAM(sf_spawn_detached, "spawn_detached_gateway", "spawn_detached")
BOOL_SEAM(sf_print_parked, "print_parked_status", "print_parked_status")

static bool sf_profile_is_standalone(void *ctx, const char *home) {
    script_ctx_t *c = ctx; bool r = in_bool(c, "profile_is_standalone");
    json_t *e = ev("profile_is_standalone"); evs(e, "home", home); evb(e, "ret", r); rec(c, e);
    return r;
}

static char *sf_detect_service_mgr(void *ctx) {
    script_ctx_t *c = ctx;
    const char *r = in_str(c, "detect_service_mgr", "");
    json_t *e = ev("detect_service_manager"); evs(e, "ret", r); rec(c, e);
    return strdup(r);
}
static const char *sf_service_name(void *ctx) {
    script_ctx_t *c = ctx;
    const char *r = in_str(c, "service_name", "hermes-gateway");
    json_t *e = ev("get_service_name"); evs(e, "ret", r); rec(c, e);
    return r;
}
static const char *sf_scope_label(void *ctx, bool system) {
    script_ctx_t *c = ctx;
    const char *def = system ? "system" : "user";
    const char *r = in_str(c, "scope_label", def);
    json_t *e = ev("service_scope_label"); evb(e, "system", system); evs(e, "ret", r); rec(c, e);
    return r;
}
static const char *sf_current_profile_name(void *ctx) {
    script_ctx_t *c = ctx;
    const char *r = in_str(c, "current_profile_name", "default");
    json_t *e = ev("current_profile_name"); evs(e, "ret", r); rec(c, e);
    return r;
}
static const char *sf_get_hermes_home(void *ctx) {
    script_ctx_t *c = ctx;
    const char *r = in_str(c, "get_hermes_home", "C:/h");
    json_t *e = ev("get_hermes_home"); evs(e, "ret", r); rec(c, e);
    return r;
}
static const char *sf_active_profile_name(void *ctx) {
    script_ctx_t *c = ctx;
    const json_t *v = json_obj_get(c->in, "active_profile_name");
    const char *r = (v && v->type == JSON_STRING) ? v->str_val : NULL;
    json_t *e = ev("get_active_profile_name"); evs(e, "ret", r); rec(c, e);
    return r;
}
static const char *sf_standalone_notice(void *ctx) {
    script_ctx_t *c = ctx;
    return in_str(c, "standalone_notice", "NOTICE");   /* constant access: no event */
}
static const char *sf_routing_home(void *ctx) {
    script_ctx_t *c = ctx;
    const char *r = in_str(c, "routing_home", "C:/h");
    json_t *e = ev("get_routing_process_hermes_home"); evs(e, "ret", r); rec(c, e);
    return r;
}
static const char *sf_named_profile_home(void *ctx, const char *home) {
    script_ctx_t *c = ctx;
    const json_t *v = json_obj_get(c->in, "named_profile_home");
    const char *r = (v && v->type == JSON_STRING) ? v->str_val : NULL;
    json_t *e = ev("named_profile_home"); evs(e, "home", home); evs(e, "ret", r); rec(c, e);
    return r;
}
static bool sf_refuses_container(void *ctx, bool system) {
    script_ctx_t *c = ctx; bool r = in_bool(c, "refuses_container");
    json_t *e = ev("refuses_container_user_scope_install"); evb(e, "system", system); evb(e, "ret", r); rec(c, e);
    return r;
}
static void sf_managed_error(void *ctx, const char *action) {
    script_ctx_t *c = ctx; json_t *e = ev("managed_error"); evs(e, "action", action); rec(c, e);
}
static void sf_record_service_choice(void *ctx, const char *action) {
    script_ctx_t *c = ctx; json_t *e = ev("record_service_choice"); evs(e, "action", action); rec(c, e);
}
static bool sf_profile_lifecycle(void *ctx, const char *action, const json_t *args) {
    (void)args; script_ctx_t *c = ctx; bool r = in_bool(c, "profile_lifecycle");
    json_t *e = ev("profile_lifecycle"); evs(e, "action", action); evb(e, "ret", r); rec(c, e);
    return r;
}
static bool sf_host_is_ours(void *ctx, const json_t *owner) {
    (void)owner; script_ctx_t *c = ctx; bool r = in_bool(c, "host_is_ours");
    json_t *e = ev("host_multiplexer_is_ours"); evb(e, "ret", r); rec(c, e);
    return r;
}
static json_t *sf_find_pids(void *ctx) {
    script_ctx_t *c = ctx;
    const json_t *v = json_obj_get(c->in, "find_pids");
    json_t *r = (v && v->type == JSON_ARRAY) ? json_copy(v) : json_array();
    json_t *e = ev("find_gateway_pids"); evj(e, "ret", r); rec(c, e);
    return r;
}
static int sf_kill_processes(void *ctx, bool all_profiles) {
    script_ctx_t *c = ctx; int r = (int)json_get_num(c->in, "kill_processes", 0);
    json_t *e = ev("kill_gateway_processes"); evb(e, "all_profiles", all_profiles); evn(e, "ret", r); rec(c, e);
    return r;
}
static int sf_stop_host_multiplexer(void *ctx, const json_t *owner) {
    script_ctx_t *c = ctx; int r = (int)json_get_num(c->in, "stop_host_multiplexer", 0);
    json_t *e = ev("stop_host_multiplexer"); evn(e, "owner_pid", json_get_num(owner, "pid", 0)); evn(e, "ret", r); rec(c, e);
    return r;
}
static void sf_report_stop_failure(void *ctx) {
    script_ctx_t *c = ctx; rec(c, ev("report_profile_stop_failure"));
}
static void sf_wait_gateway_exit(void *ctx, double timeout, double force_after) {
    script_ctx_t *c = ctx; json_t *e = ev("wait_for_gateway_exit"); evn(e, "timeout", timeout); evn(e, "force_after", force_after); rec(c, e);
}
static void sf_wait_port_free(void *ctx) { script_ctx_t *c = ctx; rec(c, ev("wait_for_api_server_port_free")); }
static void sf_discard_host_record(void *ctx) { script_ctx_t *c = ctx; rec(c, ev("discard_dead_host_record")); }
static void sf_forget_host_probe(void *ctx) { script_ctx_t *c = ctx; rec(c, ev("forget_host_gateway_probe")); }
static void sf_run_gateway(void *ctx, int verbose, bool replace) {
    script_ctx_t *c = ctx; json_t *e = ev("run_gateway"); evn(e, "verbose", verbose); evb(e, "replace", replace); rec(c, e);
}
static json_t *sf_get_snapshot(void *ctx, bool system) {
    script_ctx_t *c = ctx;
    const json_t *v = json_obj_get(c->in, "snapshot");
    json_t *r = (v && v->type == JSON_OBJECT) ? json_copy(v) : json_object();
    json_t *e = ev("get_gateway_runtime_snapshot"); evb(e, "system", system); evj(e, "ret", r); rec(c, e);
    return r;
}
static bool sf_report_incompat_owner(void *ctx, const json_t *pids) {
    script_ctx_t *c = ctx; bool r = in_bool(c, "report_incompat_owner");
    json_t *e = ev("report_incompatible_owner"); evj(e, "pids", pids); evb(e, "ret", r); rec(c, e);
    return r;
}
static void sf_print_standalone_cfg(void *ctx) { script_ctx_t *c = ctx; rec(c, ev("print_standalone_by_config")); }
static void sf_print_served_urls(void *ctx, const char *profile) {
    script_ctx_t *c = ctx; json_t *e = ev("print_served_ingress_urls"); evs(e, "profile", profile); rec(c, e);
}
static void sf_print_unserved_urls(void *ctx, const char *profile) {
    script_ctx_t *c = ctx; json_t *e = ev("print_unserved_shared_ingress"); evs(e, "profile", profile); rec(c, e);
}
static void sf_print_multiplex_reason(void *ctx) { script_ctx_t *c = ctx; rec(c, ev("print_multiplex_standalone_reason")); }
static void sf_print_runtime_health(void *ctx) { script_ctx_t *c = ctx; rec(c, ev("print_runtime_health")); }
static void sf_print_dup_creds(void *ctx) { script_ctx_t *c = ctx; rec(c, ev("print_duplicate_credential_warnings")); }
static void sf_print_other_profiles(void *ctx) { script_ctx_t *c = ctx; rec(c, ev("print_other_profiles_gateway_status")); }
static void sf_print_unfolded_note(void *ctx, const json_t *owner) {
    script_ctx_t *c = ctx; json_t *e = ev("print_unfolded_gateway_note"); evn(e, "owner_pid", json_get_num(owner, "pid", 0)); rec(c, e);
}
static void sf_print_mismatch(void *ctx, const json_t *snapshot) {
    script_ctx_t *c = ctx; json_t *e = ev("print_gateway_process_mismatch"); evj(e, "snapshot", snapshot); rec(c, e);
}
static const char *sf_owner_describe(void *ctx, const json_t *owner) {
    script_ctx_t *c = ctx; (void)c;
    const char *r = json_get_str(owner, "describe", "");
    json_t *e = ev("owner_describe"); evs(e, "ret", r); rec(c, e);
    return r;
}
static json_t *sf_host_owner(void *ctx) {
    script_ctx_t *c = ctx;
    const json_t *v = json_obj_get(c->in, "host_owner");
    json_t *r = (v && v->type == JSON_OBJECT) ? json_copy(v) : NULL;
    json_t *e = ev("host_multiplexer_for_all_verb"); evj(e, "ret", r); rec(c, e);
    return r;
}
static json_t *sf_served_owner(void *ctx) {
    script_ctx_t *c = ctx;
    const json_t *v = json_obj_get(c->in, "served_owner");
    json_t *r = (v && v->type == JSON_OBJECT) ? json_copy(v) : NULL;
    json_t *e = ev("served_by_another_host_gateway"); evj(e, "ret", r); rec(c, e);
    return r;
}
static void sf_host_scope_enter(void *ctx, const json_t *owner) {
    script_ctx_t *c = ctx; json_t *e = ev("host_scope_enter"); evj(e, "owner", owner); rec(c, e);
}
static void sf_host_scope_exit(void *ctx) { script_ctx_t *c = ctx; rec(c, ev("host_scope_exit")); }
static void sf_restart_scope_enter(void *ctx, const json_t *owner) {
    script_ctx_t *c = ctx; json_t *e = ev("restart_all_scope_enter"); evj(e, "owner", owner); rec(c, e);
}
static void sf_restart_scope_exit(void *ctx) { script_ctx_t *c = ctx; rec(c, ev("restart_all_scope_exit")); }
static void sf_owe_online_notice(void *ctx) { script_ctx_t *c = ctx; rec(c, ev("owe_online_notice")); }
static double sf_restart_wait_budget(void *ctx) {
    script_ctx_t *c = ctx; double r = json_get_num(c->in, "restart_wait_budget", 30.0);
    json_t *e = ev("get_restart_exit_wait_budget"); evn(e, "ret", r); rec(c, e);
    return r;
}
static double sf_process_start_time(void *ctx, int pid, bool *has) {
    script_ctx_t *c = ctx;
    const json_t *v = json_obj_get(c->in, "process_start_time");
    *has = (v && v->type == JSON_NUMBER);
    double r = *has ? v->num_val : 0.0;
    json_t *e = ev("get_process_start_time"); evn(e, "pid", pid);
    if (*has) evn(e, "ret", r); else json_set(e, "ret", json_null());
    rec(c, e);
    return r;
}
static const char *sf_drain_reporter(void *ctx, double budget_s) {
    script_ctx_t *c = ctx;
    const char *r = in_str(c, "drain_reporter", "reporter");
    json_t *e = ev("drain_progress_reporter"); evn(e, "budget_s", budget_s); evs(e, "ret", r); rec(c, e);
    return r;
}
static bool sf_graceful_sigusr1(void *ctx, int pid, double budget, const char *on_progress) {
    script_ctx_t *c = ctx; bool r = in_bool(c, "graceful_sigusr1");
    json_t *e = ev("graceful_restart_via_sigusr1"); evn(e, "pid", pid); evn(e, "budget", budget);
    evs(e, "on_progress", on_progress); evb(e, "ret", r); rec(c, e);
    return r;
}
static bool sf_leave_restart_pending(void *ctx, int pid, const char *label, double start_time, bool has_start) {
    script_ctx_t *c = ctx; bool r = in_bool(c, "leave_restart_pending");
    json_t *e = ev("leave_restart_pending"); evn(e, "pid", pid); evs(e, "label", label);
    if (has_start) evn(e, "start_time", start_time); else json_set(e, "start_time", json_null());
    evb(e, "ret", r); rec(c, e);
    return r;
}
static bool sf_wait_systemd_restart(void *ctx, bool system, int previous_pid, bool *replacement_observed) {
    script_ctx_t *c = ctx; bool r = in_bool(c, "wait_systemd_restart");
    const json_t *ro = json_obj_get(c->in, "replacement_observed");
    json_t *roa = (ro && ro->type == JSON_ARRAY) ? json_copy(ro) : json_array();
    *replacement_observed = (roa->c.count > 0);
    json_t *e = ev("wait_for_systemd_service_restart"); evb(e, "system", system); evn(e, "previous_pid", previous_pid);
    evj(e, "replacement_observed", roa); evb(e, "ret", r); rec(c, e);
    json_free(roa);
    return r;
}
static bool sf_systemd_start_limited(void *ctx, bool system) {
    script_ctx_t *c = ctx; bool r = in_bool(c, "systemd_start_limited");
    json_t *e = ev("systemd_service_is_start_limited"); evb(e, "system", system); evb(e, "ret", r); rec(c, e);
    return r;
}
static json_t *sf_read_unit_props(void *ctx, bool system) {
    script_ctx_t *c = ctx;
    const json_t *v = json_obj_get(c->in, "unit_props");
    json_t *r = (v && v->type == JSON_OBJECT) ? json_copy(v) : json_object();
    json_t *e = ev("read_systemd_unit_properties"); evb(e, "system", system); evj(e, "ret", r); rec(c, e);
    return r;
}
static char *sf_unit_read_text(void *ctx, const char *path, bool *raised) {
    script_ctx_t *c = ctx;
    const json_t *v = json_obj_get(c->in, "unit_text");
    *raised = in_bool(c, "unit_read_raises");
    char *r = NULL;
    if (!*raised && v && v->type == JSON_STRING) r = strdup(v->str_val);
    json_t *e = ev("unit_read_text"); evs(e, "path", path); evs(e, "ret", r); evb(e, "raised", *raised); rec(c, e);
    return r;
}
static void sf_reap_orphans(void *ctx, gwsl_exc_t *exc, const char **exc_msg) {
    script_ctx_t *c = ctx;
    const json_t *script = next_obj(c, "reap_orphans", &c->reap_idx);
    const char *raised = apply_exc(script, exc);
    *exc_msg = json_get_str(script, "msg", NULL);
    json_t *e = ev("reap_orphans"); evs(e, "raised", raised); rec(c, e);
}
static void sf_logger_debug(void *ctx, const char *msg) {
    script_ctx_t *c = ctx; json_t *e = ev("logger_debug"); evs(e, "msg", msg); rec(c, e);
}
static bool sf_get_linger_status(void *ctx, bool *ok) {
    script_ctx_t *c = ctx; *ok = in_bool(c, "linger_ok");
    json_t *e = ev("get_systemd_linger_status"); evb(e, "ok", *ok); rec(c, e);
    return *ok;
}
static const char *sf_getuser(void *ctx) {
    script_ctx_t *c = ctx; const char *r = in_str(c, "getuser", "user");
    json_t *e = ev("getuser"); evs(e, "ret", r); rec(c, e);
    return r;
}
static long sf_get_running_pid(void *ctx) {
    script_ctx_t *c = ctx;
    const json_t *v = json_obj_get(c->in, "get_running_pid");
    long r = (v && v->type == JSON_NUMBER) ? (long)v->num_val : -1;
    json_t *e = ev("get_running_pid");
    if (r >= 0) evn(e, "ret", r); else json_set(e, "ret", json_null());
    rec(c, e);
    return r;
}
static bool sf_declares_external_supervisor(void *ctx, long pid) {
    script_ctx_t *c = ctx; bool r = in_bool(c, "declares_external_supervisor");
    json_t *e = ev("gateway_declares_external_supervisor"); evn(e, "pid", pid); evb(e, "ret", r); rec(c, e);
    return r;
}
static void sf_restart_externally_supervised(void *ctx, long pid) {
    script_ctx_t *c = ctx; json_t *e = ev("restart_externally_supervised_gateway"); evn(e, "pid", pid); rec(c, e);
}
static void sf_refuse_restart_service_managed(void *ctx, long pid, gwsl_exc_t *exc) {
    script_ctx_t *c = ctx;
    const json_t *script = next_obj(c, "refuse_restart_service_managed", &c->refuse_restart_idx);
    const char *raised = apply_exc(script, exc);
    json_t *e = ev("refuse_restart_of_service_managed_gateway");
    if (pid >= 0) evn(e, "pid", pid); else json_set(e, "pid", json_null());
    rec_raised(e, raised); rec(c, e);
}
static void sf_start_restart_replacement(void *ctx, const json_t *args, bool force, gwsl_exc_t *exc) {
    (void)args; script_ctx_t *c = ctx;
    const json_t *script = next_obj(c, "start_restart_replacement", &c->start_repl_idx);
    const char *raised = apply_exc(script, exc);
    json_t *e = ev("start_restart_replacement"); evb(e, "force", force); rec_raised(e, raised); rec(c, e);
}
static void sf_print(void *ctx, const char *text) {
    script_ctx_t *c = ctx; json_t *e = ev("print"); evs(e, "text", text); rec(c, e);
}
static void sf_print_error(void *ctx, const char *text) {
    script_ctx_t *c = ctx; json_t *e = ev("print_error"); evs(e, "text", text); rec(c, e);
}

/* ── raising dispatch seams ── */
static json_t *sf_gw_windows_call(void *ctx, const char *method, const json_t *kwargs, gwsl_exc_t *exc) {
    script_ctx_t *c = ctx;
    const json_t *script = next_obj(c, "gw_windows_calls", &c->gw_idx);
    const char *raised = apply_exc(script, exc);
    json_t *e = ev("gw_windows"); evs(e, "method", method); evj(e, "kwargs", kwargs);
    if (raised) { evs(e, "raised", raised); rec(c, e); return NULL; }
    const json_t *ret = script ? json_obj_get(script, "ret") : NULL;
    json_t *r = ret ? json_copy(ret) : json_null();
    evj(e, "ret", r); rec(c, e);
    return r;
}
static void sf_systemd_call(void *ctx, const char *verb, int system_tristate, gwsl_exc_t *exc) {
    script_ctx_t *c = ctx;
    const json_t *script = next_obj(c, "systemd_calls", &c->systemd_idx);
    const char *raised = apply_exc(script, exc);
    json_t *kw = json_object();
    if (system_tristate >= 0) json_set(kw, "system", json_bool(system_tristate == 1));
    json_t *e = ev("systemd_call"); evs(e, "verb", verb); evj(e, "kwargs", kw); rec_raised(e, raised); rec(c, e);
    json_free(kw);
}
static void sf_systemd_status(void *ctx, bool deep, bool system, bool full, gwsl_exc_t *exc) {
    script_ctx_t *c = ctx;
    const json_t *script = next_obj(c, "systemd_status_calls", &c->systemd_status_idx);
    const char *raised = apply_exc(script, exc);
    json_t *e = ev("systemd_status"); evb(e, "deep", deep); evb(e, "system", system); evb(e, "full", full);
    rec_raised(e, raised); rec(c, e);
}
static void sf_launchd_call(void *ctx, const char *verb, gwsl_exc_t *exc) {
    script_ctx_t *c = ctx;
    const json_t *script = next_obj(c, "launchd_calls", &c->launchd_idx);
    const char *raised = apply_exc(script, exc);
    json_t *e = ev("launchd_call"); evs(e, "verb", verb); rec_raised(e, raised); rec(c, e);
}
static void sf_launchd_install(void *ctx, bool force, bool start_now, bool force_unit_path, gwsl_exc_t *exc) {
    script_ctx_t *c = ctx;
    const json_t *script = next_obj(c, "launchd_install_calls", &c->launchd_install_idx);
    const char *raised = apply_exc(script, exc);
    json_t *e = ev("launchd_install"); evb(e, "force", force); evb(e, "start_now", start_now);
    evb(e, "force_unit_path", force_unit_path); rec_raised(e, raised); rec(c, e);
}
static void sf_launchd_status(void *ctx, bool deep, gwsl_exc_t *exc) {
    script_ctx_t *c = ctx;
    const json_t *script = next_obj(c, "launchd_status_calls", &c->launchd_status_idx);
    const char *raised = apply_exc(script, exc);
    json_t *e = ev("launchd_status"); evb(e, "deep", deep); rec_raised(e, raised); rec(c, e);
}
static void sf_install_systemd_cli(void *ctx, const json_t *args, bool force, bool system,
                                   const char *run_as_user, gwsl_exc_t *exc) {
    (void)args; script_ctx_t *c = ctx;
    const json_t *script = next_obj(c, "install_systemd_cli_calls", &c->install_cli_idx);
    const char *raised = apply_exc(script, exc);
    json_t *e = ev("install_systemd_from_cli"); evb(e, "force", force); evb(e, "system", system);
    evs(e, "run_as_user", run_as_user); rec_raised(e, raised); rec(c, e);
}
static void sf_guard_named_profile(void *ctx, bool force, gwsl_exc_t *exc) {
    script_ctx_t *c = ctx;
    const json_t *script = next_obj(c, "guard_named_profile_calls", &c->guard_idx);
    const char *raised = apply_exc(script, exc);
    json_t *e = ev("guard_named_profile"); evb(e, "force", force); rec_raised(e, raised); rec(c, e);
}
static void sf_refuse_sessions_live(void *ctx, const char *action, const json_t *args, gwsl_exc_t *exc) {
    (void)args; script_ctx_t *c = ctx;
    const json_t *script = next_obj(c, "refuse_sessions_live_calls", &c->sessions_idx);
    const char *raised = apply_exc(script, exc);
    json_t *e = ev("refuse_sessions_live"); evs(e, "action", action); rec_raised(e, raised); rec(c, e);
}
static bool sf_dispatch_s6(void *ctx, const char *action, gwsl_exc_t *exc) {
    script_ctx_t *c = ctx;
    const json_t *script = next_obj(c, "dispatch_s6_calls", &c->dispatch_idx);
    const char *raised = apply_exc(script, exc);
    bool r = json_get_bool(script, "ret", false);
    json_t *e = ev("dispatch_s6"); evs(e, "action", action);
    if (raised) evs(e, "raised", raised); else evb(e, "ret", r);
    rec(c, e);
    return r;
}
static bool sf_dispatch_all_s6(void *ctx, const char *action, gwsl_exc_t *exc) {
    script_ctx_t *c = ctx;
    const json_t *script = next_obj(c, "dispatch_all_s6_calls", &c->dispatch_all_idx);
    const char *raised = apply_exc(script, exc);
    bool r = json_get_bool(script, "ret", false);
    json_t *e = ev("dispatch_all_s6"); evs(e, "action", action);
    if (raised) evs(e, "raised", raised); else evb(e, "ret", r);
    rec(c, e);
    return r;
}

/* ── seam table ── */
static void fill_seams(gwsl_seams_t *s, script_ctx_t *c) {
    memset(s, 0, sizeof(*s));
    s->ctx = c;
    s->supports_systemd = sf_supports_systemd;
    s->is_macos = sf_is_macos;
    s->is_windows = sf_is_windows;
    s->is_wsl = sf_is_wsl;
    s->is_container = sf_is_container;
    s->is_termux = sf_is_termux;
    s->is_apt_termux = sf_is_apt_termux;
    s->is_supervised = sf_is_supervised;
    s->detect_service_mgr = sf_detect_service_mgr;
    s->systemd_unit_installed = sf_systemd_unit_installed;
    s->launchd_plist_exists = sf_launchd_plist_exists;
    s->service_name = sf_service_name;
    s->scope_label = sf_scope_label;
    s->current_profile_name = sf_current_profile_name;
    s->get_hermes_home = sf_get_hermes_home;
    s->active_profile_name = sf_active_profile_name;
    s->profile_is_standalone = sf_profile_is_standalone;
    s->standalone_notice = sf_standalone_notice;
    s->routing_home = sf_routing_home;
    s->named_profile_home = sf_named_profile_home;
    s->gw_windows_call = sf_gw_windows_call;
    s->systemd_call = sf_systemd_call;
    s->systemd_status = sf_systemd_status;
    s->launchd_call = sf_launchd_call;
    s->launchd_install = sf_launchd_install;
    s->launchd_status = sf_launchd_status;
    s->is_managed = sf_is_managed;
    s->managed_error = sf_managed_error;
    s->refuses_container = sf_refuses_container;
    s->record_service_choice = sf_record_service_choice;
    s->install_systemd_cli = sf_install_systemd_cli;
    s->profile_lifecycle = sf_profile_lifecycle;
    s->guard_named_profile = sf_guard_named_profile;
    s->refuse_sessions_live = sf_refuse_sessions_live;
    s->dispatch_s6 = sf_dispatch_s6;
    s->dispatch_all_s6 = sf_dispatch_all_s6;
    s->host_owner = sf_host_owner;
    s->served_owner = sf_served_owner;
    s->host_is_ours = sf_host_is_ours;
    s->named_served = sf_named_served;
    s->find_pids = sf_find_pids;
    s->kill_processes = sf_kill_processes;
    s->stop_profile = sf_stop_profile;
    s->stop_host_multiplexer = sf_stop_host_multiplexer;
    s->report_stop_failure = sf_report_stop_failure;
    s->wait_gateway_exit = sf_wait_gateway_exit;
    s->wait_port_free = sf_wait_port_free;
    s->discard_host_record = sf_discard_host_record;
    s->forget_host_probe = sf_forget_host_probe;
    s->spawn_detached = sf_spawn_detached;
    s->run_gateway = sf_run_gateway;
    s->get_snapshot = sf_get_snapshot;
    s->print_parked_status = sf_print_parked;
    s->report_incompat_owner = sf_report_incompat_owner;
    s->print_standalone_cfg = sf_print_standalone_cfg;
    s->print_served_urls = sf_print_served_urls;
    s->print_unserved_urls = sf_print_unserved_urls;
    s->print_multiplex_reason = sf_print_multiplex_reason;
    s->print_runtime_health = sf_print_runtime_health;
    s->print_dup_creds = sf_print_dup_creds;
    s->print_other_profiles = sf_print_other_profiles;
    s->print_unfolded_note = sf_print_unfolded_note;
    s->print_mismatch = sf_print_mismatch;
    s->owner_describe = sf_owner_describe;
    s->host_scope_enter = sf_host_scope_enter;
    s->host_scope_exit = sf_host_scope_exit;
    s->restart_scope_enter = sf_restart_scope_enter;
    s->restart_scope_exit = sf_restart_scope_exit;
    s->owe_online_notice = sf_owe_online_notice;
    s->restart_wait_budget = sf_restart_wait_budget;
    s->process_start_time = sf_process_start_time;
    s->drain_reporter = sf_drain_reporter;
    s->graceful_sigusr1 = sf_graceful_sigusr1;
    s->leave_restart_pending = sf_leave_restart_pending;
    s->wait_systemd_restart = sf_wait_systemd_restart;
    s->systemd_start_limited = sf_systemd_start_limited;
    s->read_unit_props = sf_read_unit_props;
    s->unit_read_text = sf_unit_read_text;
    s->reap_orphans = sf_reap_orphans;
    s->logger_debug = sf_logger_debug;
    s->get_linger_status = sf_get_linger_status;
    s->getuser = sf_getuser;
    s->get_running_pid = sf_get_running_pid;
    s->declares_external_supervisor = sf_declares_external_supervisor;
    s->restart_externally_supervised = sf_restart_externally_supervised;
    s->refuse_restart_service_managed = sf_refuse_restart_service_managed;
    s->start_restart_replacement = sf_start_restart_replacement;
    s->print = sf_print;
    s->print_error = sf_print_error;
}

/* ── case dispatch ── */
static void emit_verdict(json_t *out, gwsl_verdict_t v, gwsl_exc_t *exc) {
    if (exc->kind != GWSL_EXC_NONE) {
        json_set(out, "raised", json_string(gwsl_exc_kind_name(exc->kind)));
        if (exc->kind == GWSL_EXC_SYSTEM_EXIT) json_set(out, "exit", json_number(exc->exit_code));
        else json_set(out, "exit", json_null());
    } else if (v.exited) {
        /* A C exit verdict models live sys.exit(code), which raises SystemExit. */
        json_set(out, "raised", json_string("SystemExit"));
        json_set(out, "exit", json_number(v.exit_code));
    } else {
        json_set(out, "raised", json_null());
        json_set(out, "exit", json_null());
    }
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx; memset(&ctx, 0, sizeof(ctx));
    ctx.in = in;
    gwsl_trace_t trace = {NULL};
    ctx.trace = &trace;
    gwsl_seams_t s; fill_seams(&s, &ctx);
    gwsl_exc_t exc = {GWSL_EXC_NONE, 0};
    const json_t *args = json_obj_get(in, "args");
    json_t *empty_args = json_object();
    if (!args) args = empty_args;

    if (strcmp(func, "service_backend") == 0) {
        const char *r = gwsl_service_backend(json_get_bool(in, "windows", true), &s);
        json_set(out, "ret", r ? json_string(r) : json_null());
    } else if (strcmp(func, "installed_service_kind_for") == 0) {
        /* thunk scripted as a constant bool ("windows_thunk") */
        bool w = json_get_bool(in, "windows_thunk", false);
        const char *r = gwsl_installed_service_kind_for(&s, NULL, NULL);
        if (!r && w) r = "windows";
        json_set(out, "ret", r ? json_string(r) : json_null());
    } else if (strcmp(func, "installed_service_kind") == 0) {
        const char *r = gwsl_installed_service_kind(&s);
        json_set(out, "ret", r ? json_string(r) : json_null());
    } else if (strcmp(func, "service_call") == 0) {
        const char *backend = json_get_str(in, "backend", "systemd");
        const char *verb = json_get_str(in, "verb", "start");
        const json_t *sy = json_obj_get(in, "system");
        int tristate = (sy && sy->type == JSON_BOOL) ? (sy->bool_val ? 1 : 0) : -1;
        gwsl_service_call(backend, verb, tristate, &s, &exc);
        json_set(out, "ret", json_null());
        emit_verdict(out, (gwsl_verdict_t){false, 0}, &exc);
    } else if (strcmp(func, "stop_installed_service") == 0) {
        bool r = gwsl_stop_installed_service(json_get_bool(in, "system", false), &s, &exc);
        json_set(out, "ret", exc.kind != GWSL_EXC_NONE ? json_null() : json_bool(r));
        emit_verdict(out, (gwsl_verdict_t){false, 0}, &exc);
    } else if (strcmp(func, "profile_name_from_home") == 0) {
        char *r = gwsl_profile_name_from_home(json_get_str(in, "home", ""), json_get_str(in, "default", ""));
        json_set(out, "ret", r ? json_string(r) : json_null());
        free(r);
    } else if (strcmp(func, "unit_environment_value") == 0) {
        char *r = gwsl_unit_environment_value(json_get_str(in, "unit_path", "unit.service"),
                                              json_get_str(in, "name", "HERMES_HOME"), &s);
        json_set(out, "ret", r ? json_string(r) : json_null());
        free(r);
    } else if (strcmp(func, "positive_pid") == 0) {
        long r = gwsl_positive_pid(json_obj_get(in, "value"));
        json_set(out, "ret", r >= 0 ? json_number(r) : json_null());
    } else if (strcmp(func, "no_backend_exit") == 0) {
        bool ke = false;
        gwsl_verdict_t v = gwsl_no_backend_exit(json_get_str(in, "subcommand", "install"),
                                                json_get_str(in, "reason", "unsupported"), &s, &ke);
        json_set(out, "ret", json_null());
        if (ke) { exc.kind = GWSL_EXC_KEY_ERROR; }
        emit_verdict(out, v, &exc);
    } else if (strcmp(func, "handle_no_backend") == 0) {
        bool ke = false;
        gwsl_verdict_t v = gwsl_handle_no_backend_flow(json_get_str(in, "subcommand", "install"),
                                                       json_get_bool(in, "wsl", false),
                                                       json_get_bool(in, "s6", false), &s, &ke);
        json_set(out, "ret", json_null());
        if (ke) { exc.kind = GWSL_EXC_KEY_ERROR; }
        emit_verdict(out, v, &exc);
    } else if (strcmp(func, "systemd_graceful_restart_action") == 0) {
        const char *r = gwsl_systemd_graceful_restart_action_flow(json_get_bool(in, "system", false),
                                                                  (int)json_get_num(in, "pid", 0), &s);
        json_set(out, "ret", r ? json_string(r) : json_null());
    } else if (strcmp(func, "cmd_install") == 0) {
        gwsl_verdict_t v = gwsl_cmd_install_flow(args, &s, &exc);
        json_set(out, "ret", json_null()); emit_verdict(out, v, &exc);
    } else if (strcmp(func, "cmd_uninstall") == 0) {
        gwsl_verdict_t v = gwsl_cmd_uninstall_flow(args, &s, &exc);
        json_set(out, "ret", json_null()); emit_verdict(out, v, &exc);
    } else if (strcmp(func, "cmd_start") == 0) {
        gwsl_verdict_t v = gwsl_cmd_start_flow(args, &s, &exc);
        json_set(out, "ret", json_null()); emit_verdict(out, v, &exc);
    } else if (strcmp(func, "cmd_stop") == 0) {
        gwsl_verdict_t v = gwsl_cmd_stop_flow(args, &s, &exc);
        json_set(out, "ret", json_null()); emit_verdict(out, v, &exc);
    } else if (strcmp(func, "cmd_restart") == 0) {
        gwsl_verdict_t v = gwsl_cmd_restart_flow(args, &s, &exc);
        json_set(out, "ret", json_null()); emit_verdict(out, v, &exc);
    } else if (strcmp(func, "cmd_status") == 0) {
        gwsl_verdict_t v = gwsl_cmd_status_flow(args, &s, &exc);
        json_set(out, "ret", json_null()); emit_verdict(out, v, &exc);
    } else if (strcmp(func, "restart_all") == 0) {
        gwsl_verdict_t v = gwsl_restart_all_flow(json_get_bool(in, "system", false),
                                                 json_get_bool(in, "detach", false), &s, &exc);
        json_set(out, "ret", json_null()); emit_verdict(out, v, &exc);
    } else if (strcmp(func, "restart_all_as_host") == 0) {
        const json_t *owner = json_obj_get(in, "owner");
        gwsl_verdict_t v = gwsl_restart_all_as_host_flow(owner, json_get_bool(in, "system", false),
                                                         json_get_bool(in, "detach", false), &s, &exc);
        json_set(out, "ret", json_null()); emit_verdict(out, v, &exc);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }
    json_free(empty_args);
    if (!json_has(out, "exit")) json_set(out, "exit", json_null());
    if (!json_has(out, "raised")) json_set(out, "raised", json_null());
    if (!json_has(out, "ret")) json_set(out, "ret", json_null());
    json_set(out, "trace", trace.events ? trace.events : json_array());
}

int main(int argc, char **argv) {
    FILE *f = (argc > 1) ? fopen(argv[1], "rb") : stdin;
    if (!f) { fprintf(stderr, "cannot open fixture\n"); return 1; }
    char line[1 << 20];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '#') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case line: %s\n", err ? err : "not an object");
            free(err); json_free(c); continue;
        }
        const char *func = json_get_str(c, "func", "");
        const json_t *in = json_obj_get(c, "in");
        json_t *empty = json_object();
        json_t *out = json_object();
        run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
        char *func_s = json_escape_string(func);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out); json_free(c);
    }
    if (f != stdin) fclose(f);
    return 0;
}