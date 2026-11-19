/*
 * t_port_terminal_lifecycle_flow.c — oracle harness for the terminal-tool
 * execution-lifecycle FLOW cores ported in src/tools/terminal_tool_lifecycle_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * Wires each flow's seams to the SCRIPTED results in "in" (recording stubs
 * that append one trace event per seam call, in call order) and emits
 * {"func": ..., "in": ..., "out": {"ret": ..., "trace": [...], ...}} per case.
 * tests/oracle_terminal_lifecycle_flow.py drives the LIVE Python with the same
 * scripts and compares the ordered trace + return value + mutated state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "libjson/json.h"
#include "terminal_tool_lifecycle_flow.h"

typedef struct {
    const json_t *in;
    ttlf_trace_t *trace;
} script_ctx_t;

/* ── event builders ── */
static json_t *ev(const char *name) {
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    return e;
}
static void evs(json_t *e, const char *k, const char *v) {
    json_set(e, k, v ? json_string(v) : json_null());
}
static void evb(json_t *e, const char *k, bool v) { json_set(e, k, json_bool(v)); }
static void evn(json_t *e, const char *k, double v) { json_set(e, k, json_number(v)); }
static void evj(json_t *e, const char *k, const json_t *v) {
    json_set(e, k, v ? json_copy(v) : json_null());
}
static void rec(script_ctx_t *c, json_t *e) { ttlf_trace_add(c->trace, e); }

static const json_t *in_obj(const script_ctx_t *c, const char *k) {
    const json_t *v = json_obj_get(c->in, k);
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}
static bool in_bool(const script_ctx_t *c, const char *k) {
    return json_get_bool(c->in, k, false);
}
static const char *in_str(const script_ctx_t *c, const char *k, const char *def) {
    return json_get_str(c->in, k, def);
}
/* scripted string-or-null field: returns the node (NULL when absent). */
static const json_t *in_node(const script_ctx_t *c, const char *k) {
    return json_obj_get(c->in, k);
}
static char *xstrdup_h(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *d = malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

/* ── shared seams ── */
static void sc_log(void *ctx, const char *level, const char *msg) {
    script_ctx_t *c = ctx;
    json_t *e = ev("log");
    evs(e, "level", level);
    evs(e, "msg", msg);
    rec(c, e);
}

static char *sc_redact(void *ctx, const char *text) {
    script_ctx_t *c = ctx;
    const char *r = in_str(c, "redact_ret", text);
    json_t *e = ev("redact");
    evs(e, "text", text);
    evs(e, "ret", r);
    rec(c, e);
    return xstrdup_h(r);
}

/* resolve_command_cwd seam (foreground + spawn share the script field). */
static char *sc_resolve_cwd(void *ctx, const json_t *kw) {
    script_ctx_t *c = ctx;
    const char *def = json_get_str(kw, "default_cwd", "");
    const char *r = in_str(c, "command_cwd", def);
    json_t *e = ev("resolve_command_cwd");
    evj(e, "workdir", json_obj_get(kw, "workdir"));
    evs(e, "default_cwd", def);
    evj(e, "session_key", json_obj_get(kw, "session_key"));
    evj(e, "env_type", json_obj_get(kw, "env_type"));
    evj(e, "mounted_host", json_obj_get(kw, "mounted_host"));
    evj(e, "env", json_obj_get(kw, "env"));
    evs(e, "ret", r);
    rec(c, e);
    return xstrdup_h(r);
}

/* ── run_approval_guards seams ── */
static bool ap_host_access(void *ctx, const json_t *config) {
    script_ctx_t *c = ctx;
    bool r = in_bool(c, "host_access");
    json_t *e = ev("docker_has_host_access");
    evj(e, "config", config);
    evb(e, "ret", r);
    rec(c, e);
    return r;
}
static const json_t *ap_check_guards(void *ctx, const char *command, const char *env_type,
                                     bool has_host_access) {
    script_ctx_t *c = ctx;
    const json_t *r = in_obj(c, "approval");
    static json_t *def = NULL;
    if (!r) {
        if (!def) { def = json_object(); json_set(def, "approved", json_bool(true)); }
        r = def;
    }
    json_t *e = ev("check_all_guards");
    evs(e, "command", command);
    evs(e, "env_type", env_type);
    evb(e, "has_host_access", has_host_access);
    evj(e, "ret", r);
    rec(c, e);
    return r;
}
static char *ap_preapproval_note(void *ctx, const json_t *pre_approved) {
    script_ctx_t *c = ctx;
    const char *r = in_str(c, "preapproval_note_ret", "");
    json_t *e = ev("preapproval_note");
    evj(e, "pre_approved", pre_approved);
    evs(e, "ret", r);
    rec(c, e);
    return xstrdup_h(r);
}

/* ── pre_exec_block seams ── */
static char *pe_lifecycle(void *ctx, const json_t *kw) {
    script_ctx_t *c = ctx;
    const json_t *r = in_node(c, "lifecycle_block");
    const char *rs = (r && r->type == JSON_STRING) ? r->str_val : NULL;
    json_t *e = ev("gateway_lifecycle_block");
    evs(e, "command", json_get_str(kw, "command", ""));
    evs(e, "env_type", json_get_str(kw, "env_type", ""));
    evs(e, "cwd", json_get_str(kw, "cwd", ""));
    evj(e, "workdir", json_obj_get(kw, "workdir"));
    evs(e, "session_key", json_get_str(kw, "session_key", ""));
    evs(e, "ret", rs);
    rec(c, e);
    return xstrdup_h(rs);
}
static char *pe_validate_workdir(void *ctx, const char *workdir) {
    script_ctx_t *c = ctx;
    const json_t *r = in_node(c, "workdir_error");
    const char *rs = (r && r->type == JSON_STRING) ? r->str_val : NULL;
    json_t *e = ev("validate_workdir");
    evs(e, "workdir", workdir);
    evs(e, "ret", rs);
    rec(c, e);
    return xstrdup_h(rs);
}
static char *pe_refuse_local(void *ctx, const char *workdir, const json_t *env) {
    script_ctx_t *c = ctx;
    const json_t *r = in_node(c, "refuse_local_reject");
    const char *rs = (r && r->type == JSON_STRING) ? r->str_val : NULL;
    json_t *e = ev("refuse_missing_local_workdir");
    evs(e, "workdir", workdir);
    evj(e, "env", env);
    evs(e, "ret", rs);
    rec(c, e);
    return xstrdup_h(rs);
}
static char *pe_block(void *ctx, const json_t *kw, const char *evname, const char *field) {
    script_ctx_t *c = ctx;
    const json_t *r = in_node(c, field);
    const char *rs = (r && r->type == JSON_STRING) ? r->str_val : NULL;
    json_t *e = ev(evname);
    evs(e, "command", json_get_str(kw, "command", ""));
    evs(e, "cwd", json_get_str(kw, "cwd", ""));
    evj(e, "workdir", json_obj_get(kw, "workdir"));
    evs(e, "session_key", json_get_str(kw, "session_key", ""));
    evs(e, "ret", rs);
    rec(c, e);
    return xstrdup_h(rs);
}
static char *pe_self_repo(void *ctx, const json_t *kw) {
    return pe_block(ctx, kw, "self_repo_block", "self_repo_block");
}
static char *pe_locked_skill(void *ctx, const json_t *kw) {
    return pe_block(ctx, kw, "locked_skill_block", "locked_skill_block");
}

/* ── plan_execution seams ── */
static const json_t *pl_config(void *ctx) {
    script_ctx_t *c = ctx;
    const json_t *r = in_obj(c, "config");
    static json_t *def = NULL;
    if (!r) { if (!def) def = json_object(); r = def; }
    json_t *e = ev("get_env_config");
    evj(e, "ret", r);
    rec(c, e);
    return r;
}
static bool pl_enforce(void *ctx, char **exc_msg) {
    script_ctx_t *c = ctx;
    bool raised = in_bool(c, "refusal_raises");
    json_t *e = ev("enforce_no_refusal");
    evb(e, "raised", raised);
    rec(c, e);
    if (raised) *exc_msg = xstrdup_h(in_str(c, "refusal_msg", "refusal scope"));
    return raised;
}
static char *pl_container_task_id(void *ctx, const char *task_id) {
    script_ctx_t *c = ctx;
    const char *r = in_str(c, "container_task_id", task_id ? task_id : "default");
    json_t *e = ev("resolve_container_task_id");
    evs(e, "task_id", task_id);
    evs(e, "ret", r);
    rec(c, e);
    return xstrdup_h(r);
}
static const json_t *pl_overrides(void *ctx, const char *task_id) {
    script_ctx_t *c = ctx;
    const json_t *r = in_obj(c, "overrides");
    static json_t *def = NULL;
    if (!r) { if (!def) def = json_object(); r = def; }
    json_t *e = ev("resolve_task_overrides");
    evs(e, "task_id", task_id);
    evj(e, "ret", r);
    rec(c, e);
    return r;
}
static char *pl_image(void *ctx, const char *env_type, const json_t *overrides,
                      const json_t *config) {
    script_ctx_t *c = ctx;
    const char *r = in_str(c, "image", "");
    json_t *e = ev("select_image");
    evs(e, "env_type", env_type);
    evj(e, "overrides", overrides);
    evj(e, "config", config);
    evs(e, "ret", r);
    rec(c, e);
    return xstrdup_h(r);
}
static char *pl_session_cwd(void *ctx, const char *session_key) {
    script_ctx_t *c = ctx;
    const json_t *r = in_node(c, "session_cwd");
    const char *rs = (r && r->type == JSON_STRING) ? r->str_val : NULL;
    json_t *e = ev("get_session_cwd");
    evs(e, "session_key", session_key);
    evs(e, "ret", rs);
    rec(c, e);
    return xstrdup_h(rs);
}
static json_t *pl_coerce(void *ctx, const json_t *cwd, const char *env_type) {
    script_ctx_t *c = ctx;
    const json_t *r = in_node(c, "coerced_cwd");
    json_t *ret = r ? json_copy(r) : (cwd ? json_copy(cwd) : json_null());
    json_t *e = ev("coerce_ssh_remote_cwd");
    evj(e, "cwd", cwd);
    evs(e, "env_type", env_type);
    evj(e, "ret", ret);
    rec(c, e);
    return ret;
}
static json_t *pl_host_cwd(void *ctx, const json_t *config, const char *task_id) {
    script_ctx_t *c = ctx;
    const json_t *r = in_node(c, "host_cwd");
    json_t *ret = r ? json_copy(r) : json_null();
    json_t *e = ev("resolve_task_host_cwd");
    evj(e, "config", config);
    evs(e, "task_id", task_id);
    evj(e, "ret", ret);
    rec(c, e);
    return ret;
}
static bool pl_is_container(void *ctx, const char *env_type) {
    script_ctx_t *c = ctx;
    bool r = in_bool(c, "is_container");
    json_t *e = ev("is_container_backend");
    evs(e, "env_type", env_type);
    evb(e, "ret", r);
    rec(c, e);
    return r;
}
static bool pl_unusable(void *ctx, const char *cwd, const json_t *mounted_host) {
    script_ctx_t *c = ctx;
    bool r = in_bool(c, "unusable_cwd");
    json_t *e = ev("is_unusable_container_cwd");
    evs(e, "cwd", cwd);
    evj(e, "mounted_host", mounted_host);
    evb(e, "ret", r);
    rec(c, e);
    return r;
}
static char *pl_trailing_amp(void *ctx, const char *command) {
    script_ctx_t *c = ctx;
    const json_t *r = in_node(c, "auto_bg");
    const char *rs = (r && r->type == JSON_STRING) ? r->str_val : NULL;
    json_t *e = ev("trailing_amp_background_command");
    evs(e, "command", command);
    evs(e, "ret", rs);
    rec(c, e);
    return xstrdup_h(rs);
}
static char *pl_guidance(void *ctx, const char *command) {
    script_ctx_t *c = ctx;
    const json_t *r = in_node(c, "guidance");
    const char *rs = (r && r->type == JSON_STRING) ? r->str_val : NULL;
    json_t *e = ev("foreground_background_guidance");
    evs(e, "command", command);
    evs(e, "ret", rs);
    rec(c, e);
    return xstrdup_h(rs);
}

/* ── run_foreground seams ── */
static void fg_clear_interrupt(void *ctx) {
    rec(ctx, ev("clear_current_thread_interrupt"));
}
static bool fg_yield_handler(void *ctx, const json_t *kw) {
    script_ctx_t *c = ctx;
    bool r = in_bool(c, "yield_handler");
    json_t *e = ev("yield_to_background_handler");
    evs(e, "command", json_get_str(kw, "command", ""));
    evj(e, "env_type", json_obj_get(kw, "env_type"));
    evj(e, "cwd", json_obj_get(kw, "cwd"));
    evs(e, "effective_task_id", json_get_str(kw, "effective_task_id", ""));
    evj(e, "task_id", json_obj_get(kw, "task_id"));
    evj(e, "session_key", json_obj_get(kw, "session_key"));
    evb(e, "ret", r);
    rec(c, e);
    return r;
}
/* fg_execute needs a real empty object when the script has no "ret". */
static json_t *fg_empty_obj(void) {
    static json_t *empty = NULL;
    if (!empty) empty = json_object();
    return empty;
}

static const json_t *fg_execute(void *ctx, const json_t *kw, int attempt,
                                char **exc_type, char **exc_msg) {
    script_ctx_t *c = ctx;
    const json_t *scripts = json_obj_get(c->in, "execute_results");
    const json_t *sc = NULL;
    if (scripts && scripts->type == JSON_ARRAY && (size_t)attempt < scripts->c.count)
        sc = scripts->c.items[attempt];
    const json_t *ret = sc ? json_obj_get(sc, "ret") : NULL;
    const json_t *et = sc ? json_obj_get(sc, "exc_type") : NULL;
    const json_t *em = sc ? json_obj_get(sc, "exc_msg") : NULL;
    const char *ets = (et && et->type == JSON_STRING) ? et->str_val : NULL;
    const char *ems = (em && em->type == JSON_STRING) ? em->str_val : "";
    json_t *e = ev("env_execute");
    evn(e, "attempt", attempt);
    evs(e, "command", json_get_str(kw, "command", ""));
    evn(e, "timeout", json_get_num(kw, "timeout", 0));
    evj(e, "cwd", json_obj_get(kw, "cwd"));
    evb(e, "bounded_capture", json_get_bool(kw, "bounded_capture", false));
    evb(e, "allow_cwd_fallback", json_get_bool(kw, "allow_cwd_fallback", false));
    evb(e, "yield_handler", json_get_bool(kw, "yield_handler", false));
    evj(e, "ret", ret);
    evs(e, "exc_type", ets);
    evs(e, "exc_msg", ets ? ems : NULL);
    rec(c, e);
    if (ets) {
        *exc_type = xstrdup_h(ets);
        *exc_msg = xstrdup_h(ems);
        return NULL;
    }
    if (ret) return ret;
    return fg_empty_obj();
}
static void fg_sleep(void *ctx, double seconds) {
    json_t *e = ev("sleep");
    evn(e, "seconds", seconds);
    rec(ctx, e);
}
static void fg_record_outcome(void *ctx, const char *command, const char *env_type,
                              const json_t *result) {
    json_t *e = ev("record_terminal_outcome");
    evs(e, "command", command);
    evs(e, "env_type", env_type);
    evj(e, "result", result);
    rec(ctx, e);
}
static char *fg_finalize(void *ctx, const json_t *kw) {
    script_ctx_t *c = ctx;
    const char *r = in_str(c, "finalize_ret", "{\"finalized\":true}");
    json_t *e = ev("finalize_foreground_result");
    evs(e, "command", json_get_str(kw, "command", ""));
    evj(e, "result", json_obj_get(kw, "result"));
    evj(e, "env", json_obj_get(kw, "env"));
    evj(e, "env_type", json_obj_get(kw, "env_type"));
    evs(e, "effective_task_id", json_get_str(kw, "effective_task_id", ""));
    evj(e, "task_id", json_obj_get(kw, "task_id"));
    evj(e, "session_id", json_obj_get(kw, "session_id"));
    evj(e, "session_key", json_obj_get(kw, "session_key"));
    evj(e, "workdir", json_obj_get(kw, "workdir"));
    evj(e, "command_cwd", json_obj_get(kw, "command_cwd"));
    evj(e, "approval_note", json_obj_get(kw, "approval_note"));
    evs(e, "ret", r);
    rec(c, e);
    return xstrdup_h(r);
}

/* ── terminal_deadline_floor seam ── */
static json_t *dl_parse_env(void *ctx, const char *name, const char *def, bool *raised) {
    script_ctx_t *c = ctx;
    bool r = in_bool(c, "parse_env_raises");
    const json_t *v = in_node(c, "parse_env_ret");
    json_t *shown = NULL;
    if (!r) {
        if (v && v->type == JSON_NUMBER) shown = json_copy(v);
        else shown = json_number(atof(def));
    }
    json_t *e = ev("parse_env_var");
    evs(e, "name", name);
    evs(e, "default", def);
    evj(e, "ret", shown);
    evb(e, "raised", r);
    rec(c, e);
    if (r) { *raised = true; json_free(shown); return NULL; }
    return shown;
}

/* ── degraded_result seams ── */
static char *dg_tenv(void *ctx, const char *name, const char *def) {
    script_ctx_t *c = ctx;
    const char *r = in_str(c, "tenv_ret", def);
    json_t *e = ev("tenv");
    evs(e, "name", name);
    evs(e, "default", def);
    evs(e, "ret", r);
    rec(c, e);
    return xstrdup_h(r);
}
static char *dg_fatal(void *ctx, const json_t *e) {
    script_ctx_t *c = ctx;
    (void)e;
    const char *r = in_str(c, "fatal_ret", "{}");
    json_t *evn2 = ev("fatal_error_json");
    evs(evn2, "ret", r);
    rec(c, evn2);
    return xstrdup_h(r);
}
static bool dg_evict(void *ctx, const char *task_id) {
    script_ctx_t *c = ctx;
    bool raised = in_bool(c, "evict_raises");
    json_t *e = ev("evict_environment_for_task");
    evs(e, "task_id", task_id);
    evb(e, "raised", raised);
    rec(c, e);
    return raised;
}

/* ── handle_terminal seams ── */
static void ht_validate(void *ctx, const json_t *args) {
    json_t *e = ev("validate_prepared_terminal");
    evj(e, "args", args);
    rec(ctx, e);
}
static char *ht_terminal_tool(void *ctx, const json_t *kw) {
    script_ctx_t *c = ctx;
    const char *r = in_str(c, "terminal_tool_ret", "{}");
    json_t *e = ev("terminal_tool");
    evj(e, "kwargs", kw);
    evs(e, "ret", r);
    rec(c, e);
    return xstrdup_h(r);
}

/* ── spawn_background seams ── */
static json_t *sp_spawn(void *ctx, const char *branch, const json_t *kw, char **exc_msg) {
    script_ctx_t *c = ctx;
    const json_t *sess = in_obj(c, "proc_session");
    const json_t *exc = in_node(c, "spawn_exc");
    const char *excs = (exc && exc->type == JSON_STRING) ? exc->str_val : NULL;
    json_t *e = ev("spawn");
    evs(e, "branch", branch);
    evj(e, "kwargs", kw);
    evj(e, "ret", sess);
    evs(e, "exc_msg", excs);
    rec(c, e);
    if (excs) { *exc_msg = xstrdup_h(excs); return NULL; }
    return sess ? json_copy(sess) : json_object();
}
static char *sp_msys(void *ctx, const char *path) {
    script_ctx_t *c = ctx;
    const char *r = in_str(c, "msys_path_ret", path);
    json_t *e = ev("msys_to_windows_path");
    evs(e, "path", path);
    evs(e, "ret", r);
    rec(c, e);
    return xstrdup_h(r);
}
static void sp_async(void *ctx, json_t *session, json_t *result_data,
                     bool notify, const json_t *watch, bool *out_notify, json_t **out_watch) {
    script_ctx_t *c = ctx;
    const json_t *nn = in_node(c, "async_notify");
    const json_t *nw = in_node(c, "async_watch");
    bool rn = (nn && nn->type == JSON_BOOL) ? nn->bool_val : notify;
    json_t *rw = nw ? json_copy(nw) : (watch ? json_copy(watch) : NULL);
    json_t *e = ev("apply_async_support");
    evj(e, "session_id", json_obj_get(session, "id"));
    evb(e, "notify_on_complete", notify);
    evj(e, "watch_patterns", watch);
    evb(e, "ret_notify", rn);
    evj(e, "ret_watch", rw);
    rec(c, e);
    const json_t *rp = in_obj(c, "async_result_patch");
    if (rp) for (size_t i = 0; i < rp->c.count; i++)
        json_set(result_data, rp->c.keys[i], json_copy(rp->c.items[i]));
    const json_t *sp = in_obj(c, "async_session_patch");
    if (sp) for (size_t i = 0; i < sp->c.count; i++)
        json_set(session, sp->c.keys[i], json_copy(sp->c.items[i]));
    *out_notify = rn;
    *out_watch = rw;
}
static void sp_watcher(void *ctx, json_t *session, const char *session_key) {
    json_t *e = ev("register_completion_watcher");
    evj(e, "session_id", json_obj_get(session, "id"));
    evs(e, "session_key", session_key);
    rec(ctx, e);
    json_set(session, "watcher_interval", json_number(5));
}
static bool sp_delegated(void *ctx) {
    script_ctx_t *c = ctx;
    bool r = in_bool(c, "delegated_child");
    json_t *e = ev("is_delegated_child_context");
    evb(e, "ret", r);
    rec(c, e);
    return r;
}
static int sp_heartbeat(void *ctx, json_t *session, int seconds) {
    script_ctx_t *c = ctx;
    int r = (int)json_get_num(c->in, "arm_heartbeat_ret", seconds);
    json_t *e = ev("arm_heartbeat");
    evj(e, "session_id", json_obj_get(session, "id"));
    evn(e, "heartbeat_seconds", seconds);
    evn(e, "ret", r);
    rec(c, e);
    return r;
}

/* ── finalize_foreground seams ── */
static void fn_record_cwd(void *ctx, const char *session_key, const char *cwd) {
    json_t *e = ev("record_session_cwd");
    evs(e, "session_key", session_key);
    evs(e, "cwd", cwd);
    rec(ctx, e);
}
static json_t *fn_sudo(void *ctx, const char *command, const char *output, const char *env_type) {
    script_ctx_t *c = ctx;
    const json_t *sa = in_obj(c, "sudo_ann");
    const char *ro = sa ? json_get_str(sa, "output", output) : output;
    bool af = sa ? json_get_bool(sa, "auth_failed", false) : false;
    bool cc = sa ? json_get_bool(sa, "cache_cleared", false) : false;
    json_t *e = ev("sudo_annotations");
    evs(e, "command", command);
    evs(e, "output", output);
    evs(e, "env_type", env_type);
    evs(e, "ret_output", ro);
    evb(e, "auth_failed", af);
    evb(e, "cache_cleared", cc);
    rec(c, e);
    json_t *r = json_object();
    json_set(r, "output", json_string(ro));
    json_set(r, "auth_failed", json_bool(af));
    json_set(r, "cache_cleared", json_bool(cc));
    return r;
}
static char *fn_transform(void *ctx, const json_t *kw, bool *raised) {
    script_ctx_t *c = ctx;
    bool r = in_bool(c, "transform_raises");
    const char *out = json_get_str(kw, "output", "");
    const char *ret = in_str(c, "transform_ret", out);
    json_t *e = ev("transform_hook");
    evs(e, "command", json_get_str(kw, "command", ""));
    evs(e, "output", out);
    evn(e, "returncode", json_get_num(kw, "returncode", 0));
    evs(e, "task_id", json_get_str(kw, "task_id", ""));
    evs(e, "env_type", json_get_str(kw, "env_type", ""));
    evs(e, "ret", ret);
    evb(e, "raised", r);
    rec(c, e);
    *raised = r;
    /* the seam models the whole helper incl. its _quiet fail-open fallback */
    return xstrdup_h(r ? out : ret);
}
static char *fn_truncate(void *ctx, const char *output) {
    script_ctx_t *c = ctx;
    const char *r = in_str(c, "truncate_ret", output);
    json_t *e = ev("truncate");
    evs(e, "output", output);
    evs(e, "ret", r);
    rec(c, e);
    return xstrdup_h(r);
}
static char *fn_strip_ansi(void *ctx, const char *output) {
    script_ctx_t *c = ctx;
    const char *r = in_str(c, "strip_ansi_ret", output);
    json_t *e = ev("strip_ansi");
    evs(e, "output", output);
    evs(e, "ret", r);
    rec(c, e);
    return xstrdup_h(r);
}
static char *fn_redact_output(void *ctx, const char *output, const char *command) {
    script_ctx_t *c = ctx;
    const char *r = in_str(c, "redact_output_ret", output);
    json_t *e = ev("redact_output");
    evs(e, "output", output);
    evs(e, "command", command);
    evs(e, "ret", r);
    rec(c, e);
    return xstrdup_h(r);
}
static char *fn_interpret(void *ctx, const char *command, int returncode) {
    script_ctx_t *c = ctx;
    const json_t *r = in_node(c, "exit_note");
    const char *rs = (r && r->type == JSON_STRING) ? r->str_val : NULL;
    json_t *e = ev("interpret_exit_code");
    evs(e, "command", command);
    evn(e, "exit_code", returncode);
    evs(e, "ret", rs);
    rec(c, e);
    return xstrdup_h(rs);
}
static char *fn_failure_hint(void *ctx, const json_t *kw, bool *raised) {
    script_ctx_t *c = ctx;
    bool r = in_bool(c, "failure_hint_raises");
    const json_t *hint = in_node(c, "failure_hint");
    const char *hs = (hint && hint->type == JSON_STRING) ? hint->str_val : NULL;
    json_t *e = ev("failure_hint");
    evs(e, "command", json_get_str(kw, "command", ""));
    evn(e, "returncode", json_get_num(kw, "returncode", 0));
    evs(e, "output", json_get_str(kw, "output", ""));
    evj(e, "exit_note", json_obj_get(kw, "exit_note"));
    evs(e, "ret", hs);
    evb(e, "raised", r);
    rec(c, e);
    *raised = r;
    return r ? NULL : xstrdup_h(hs);
}
static char *fn_realpath(void *ctx, const char *path, bool *raised) {
    script_ctx_t *c = ctx;
    bool r = in_bool(c, "realpath_raises");
    const json_t *map = in_obj(c, "realpaths");
    const char *ret = map ? json_get_str(map, path, path) : path;
    json_t *e = ev("realpath");
    evs(e, "path", path);
    evs(e, "ret", ret);
    evb(e, "raised", r);
    rec(c, e);
    if (r) { *raised = true; return NULL; }
    return xstrdup_h(ret);
}
static json_t *fn_spill(void *ctx, const json_t *path, const json_t *total_chars,
                        const char *command) {
    script_ctx_t *c = ctx;
    const json_t *pairs = json_obj_get(c->in, "spill_pairs");
    json_t *e = ev("redact_spill");
    evj(e, "path", path);
    evj(e, "total_chars", total_chars);
    evs(e, "command", command);
    /* live _redact_spill_file returns [] when path is falsy */
    json_t *empty_spill = pairs ? NULL : json_array();
    evj(e, "ret", pairs ? pairs : empty_spill);
    if (empty_spill) json_free(empty_spill);
    rec(c, e);
    return pairs ? json_copy(pairs) : json_array();
}
static json_t *fn_verification(void *ctx, const json_t *kw, bool *raised) {
    script_ctx_t *c = ctx;
    bool r = in_bool(c, "verification_raises");
    const json_t *ver = in_obj(c, "verification");
    json_t *e = ev("verification_evidence");
    evs(e, "command", json_get_str(kw, "command", ""));
    evj(e, "cwd", json_obj_get(kw, "cwd"));
    evs(e, "session_id", json_get_str(kw, "session_id", ""));
    evn(e, "returncode", json_get_num(kw, "returncode", 0));
    evs(e, "output", json_get_str(kw, "output", ""));
    evj(e, "pipeline_status", json_obj_get(kw, "pipeline_status"));
    evj(e, "ret", ver);
    evb(e, "raised", r);
    rec(c, e);
    *raised = r;
    if (r) return NULL;
    return ver ? json_copy(ver) : NULL;
}

/* ── interpret_exit_code seam ── */
static char *iec_signal(void *ctx, int exit_code) {
    script_ctx_t *c = ctx;
    const json_t *r = in_node(c, "signal_note");
    const char *rs = (r && r->type == JSON_STRING) ? r->str_val : NULL;
    json_t *e = ev("interpret_signal_exit");
    evn(e, "exit_code", exit_code);
    evs(e, "ret", rs);
    rec(c, e);
    return xstrdup_h(rs);
}

/* ── coerce_ssh_remote_cwd seams ── */
static bool ssh_launch_dir(void *ctx, const char *path) {
    script_ctx_t *c = ctx;
    bool r = in_bool(c, "launch_dir");
    json_t *e = ev("is_container_launch_dir");
    evs(e, "path", path);
    evb(e, "ret", r);
    rec(c, e);
    return r;
}
static char *ssh_subprocess_home(void *ctx) {
    script_ctx_t *c = ctx;
    const json_t *r = in_node(c, "subprocess_home");
    const char *rs = (r && r->type == JSON_STRING) ? r->str_val : NULL;
    json_t *e = ev("get_subprocess_home");
    evs(e, "ret", rs);
    rec(c, e);
    return xstrdup_h(rs);
}
static char *ssh_real_home(void *ctx) {
    script_ctx_t *c = ctx;
    const json_t *r = in_node(c, "real_home");
    const char *rs = (r && r->type == JSON_STRING) ? r->str_val : NULL;
    json_t *e = ev("get_real_home");
    evs(e, "ret", rs);
    rec(c, e);
    return xstrdup_h(rs);
}

/* ── per-case dispatch ── */
static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, NULL };
    ttlf_trace_t trace = { NULL };
    ctx.trace = &trace;

    if (strcmp(func, "error_json") == 0) {
        const json_t *errn = json_obj_get(in, "error");
        json_t *err_def = errn ? NULL : json_string("");
        const json_t *status = json_obj_get(in, "status");
        if (status && status->type != JSON_STRING) status = NULL;
        char *r = ttlf_error_json_flow(errn ? errn : err_def,
                                       (int)json_get_num(in, "exit_code", -1),
                                       status, in_obj(&ctx, "extra"));
        json_free(err_def);
        json_set(out, "ret", r ? json_string(r) : json_null());
        free(r);
    } else if (strcmp(func, "run_approval_guards") == 0) {
        ttlf_approval_seams_t s = {
            .ctx = &ctx, .docker_has_host_access = ap_host_access,
            .check_all_guards = ap_check_guards, .preapproval_note = ap_preapproval_note,
        };
        char *rejected = NULL;
        json_t *r = ttlf_run_approval_guards_flow(
            in_str(&ctx, "command", "ls"), in_str(&ctx, "env_type", "local"),
            in_obj(&ctx, "config"), in_bool(&ctx, "force"), &s, &rejected);
        json_set(out, "ret", r ? r : json_null());
        json_set(out, "rejected", rejected ? json_string(rejected) : json_null());
        free(rejected);
    } else if (strcmp(func, "pre_exec_block") == 0) {
        ttlf_preexec_seams_t s = {
            .ctx = &ctx, .gateway_lifecycle_block = pe_lifecycle,
            .validate_workdir = pe_validate_workdir, .refuse_missing_local = pe_refuse_local,
            .self_repo_block = pe_self_repo, .locked_skill_block = pe_locked_skill,
            .log = sc_log,
        };
        char *r = ttlf_pre_exec_block_flow(
            in_str(&ctx, "command", "ls"), in_obj(&ctx, "env"),
            in_str(&ctx, "env_type", "local"), in_str(&ctx, "cwd", "/w"),
            json_get_str(in, "workdir", NULL), in_str(&ctx, "session_key", "sk"), &s);
        json_set(out, "ret", json_null());
        json_set(out, "rejected", r ? json_string(r) : json_null());
        free(r);
    } else if (strcmp(func, "plan_execution") == 0) {
        ttlf_plan_seams_t s = {
            .ctx = &ctx, .get_env_config = pl_config, .enforce_no_refusal = pl_enforce,
            .resolve_container_task_id = pl_container_task_id,
            .resolve_task_overrides = pl_overrides, .select_image = pl_image,
            .get_session_cwd = pl_session_cwd, .coerce_ssh_remote_cwd = pl_coerce,
            .resolve_task_host_cwd = pl_host_cwd, .is_container_backend = pl_is_container,
            .is_unusable_container_cwd = pl_unusable, .trailing_amp = pl_trailing_amp,
            .guidance = pl_guidance, .log = sc_log,
        };
        const json_t *command = json_obj_get(in, "command");
        const char *tn = json_get_str(in, "command_py_type", NULL);
        char tn_buf[32];
        if (!tn) {
            if (!command || command->type == JSON_NULL) tn = "NoneType";
            else if (command->type == JSON_STRING) tn = "str";
            else if (command->type == JSON_BOOL) tn = "bool";
            else if (command->type == JSON_NUMBER) tn = "int";
            else if (command->type == JSON_ARRAY) tn = "list";
            else tn = "dict";
            (void)tn_buf;
        }
        char *rejected = NULL, *exception = NULL;
        json_t *r = ttlf_plan_execution_flow(
            command, tn, json_get_str(in, "task_id", NULL), json_obj_get(in, "timeout"),
            in_bool(&ctx, "background"), in_bool(&ctx, "host_local"), &s, &rejected, &exception);
        json_set(out, "ret", r ? r : json_null());
        json_set(out, "rejected", rejected ? json_string(rejected) : json_null());
        json_set(out, "exception", exception ? json_string(exception) : json_null());
        free(rejected); free(exception);
    } else if (strcmp(func, "with_promoted_note") == 0) {
        char *r = ttlf_with_promoted_note_flow(in_str(&ctx, "result_json", ""),
                                               (int)json_get_num(in, "requested_timeout", 900));
        json_set(out, "ret", r ? json_string(r) : json_null());
        free(r);
    } else if (strcmp(func, "run_foreground") == 0) {
        ttlf_foreground_seams_t s = {
            .ctx = &ctx, .clear_interrupt = fg_clear_interrupt, .resolve_cwd = sc_resolve_cwd,
            .yield_handler = fg_yield_handler, .env_execute = fg_execute, .sleep_fn = fg_sleep,
            .redact = sc_redact, .record_outcome = fg_record_outcome, .finalize = fg_finalize,
            .log = sc_log,
        };
        /* live env is an object carrying .execute; a null fixture env models
         * an attribute-less env object ({}), not None — the oracle builds
         * SimpleNamespace(execute=...) and records its non-callable attrs. */
        json_t *env_node = in_obj(&ctx, "env");
        bool env_tmp = false;
        if (!env_node || env_node->type == JSON_NULL) { env_node = json_object(); env_tmp = true; }
        char *r = ttlf_run_foreground_flow(
            in_str(&ctx, "command", "ls"), env_node,
            in_str(&ctx, "env_type", "local"), in_str(&ctx, "effective_task_id", "t1"),
            (int)json_get_num(in, "effective_timeout", 180),
            in_str(&ctx, "plan_cwd", "/w"), json_get_str(in, "plan_host_cwd", NULL),
            json_get_str(in, "task_id", NULL), json_get_str(in, "session_id", NULL),
            in_str(&ctx, "session_key", "sk"), in_str(&ctx, "cwd_key", "sk"),
            json_get_str(in, "workdir", NULL), json_get_str(in, "approval_note", NULL),
            in_bool(&ctx, "clear_interrupt"), json_get_bool(in, "metered", true), &s);
        if (env_tmp) json_free(env_node);
        json_set(out, "ret", r ? json_string(r) : json_null());
        free(r);
    } else if (strcmp(func, "terminal_deadline_floor") == 0) {
        ttlf_deadline_seams_t s = { .ctx = &ctx, .parse_env_var = dl_parse_env };
        double v = 0;
        bool has = ttlf_terminal_deadline_floor_flow(in_obj(&ctx, "args"), &s, &v);
        json_set(out, "ret", has ? json_number(v) : json_null());
    } else if (strcmp(func, "degraded_result") == 0) {
        ttlf_degraded_seams_t s = {
            .ctx = &ctx, .tenv = dg_tenv, .fatal_error_json = dg_fatal,
            .evict = dg_evict, .log = sc_log,
        };
        json_t *e = json_object();
        json_set(e, "reason", json_string(in_str(&ctx, "reason", "ssh down")));
        json_set(e, "retry_hint", json_string(in_str(&ctx, "retry_hint", "retry")));
        char *r = ttlf_degraded_result_flow(e, json_get_str(in, "task_id", NULL), &s);
        json_free(e);
        json_set(out, "ret", r ? json_string(r) : json_null());
        free(r);
    } else if (strcmp(func, "handle_terminal") == 0) {
        ttlf_handle_seams_t s = {
            .ctx = &ctx, .validate_prepared = ht_validate,
            .terminal_tool = ht_terminal_tool, .log = sc_log,
        };
        const json_t *args = in_obj(&ctx, "args");
        json_t *args_def = args ? NULL : json_object();
        char *r = ttlf_handle_terminal_flow(
            args ? args : args_def, json_get_str(in, "heartbeat_py_type", NULL),
            json_get_str(in, "task_id", NULL), json_get_str(in, "session_id", NULL), &s);
        json_free(args_def);
        json_set(out, "ret", r ? json_string(r) : json_null());
        free(r);
    } else if (strcmp(func, "spawn_background") == 0) {
        ttlf_spawn_seams_t s = {
            .ctx = &ctx, .resolve_cwd = sc_resolve_cwd, .spawn = sp_spawn,
            .msys_path = sp_msys, .apply_async_support = sp_async,
            .register_watcher = sp_watcher, .is_delegated_child = sp_delegated,
            .arm_heartbeat = sp_heartbeat, .redact = sc_redact, .log = sc_log,
        };
        json_t *params = json_object();
        json_set(params, "command", json_string(in_str(&ctx, "command", "sleep 1")));
        const json_t *env = in_obj(&ctx, "env");
        json_set(params, "env", env ? json_copy(env) : json_null());
        json_set(params, "env_type", json_string(in_str(&ctx, "env_type", "local")));
        json_set(params, "effective_task_id", json_string(in_str(&ctx, "effective_task_id", "t1")));
        const json_t *tid = json_obj_get(in, "task_id");
        json_set(params, "task_id", tid ? json_copy(tid) : json_null());
        json_set(params, "session_key", json_string(in_str(&ctx, "session_key", "sk")));
        const json_t *wd = json_obj_get(in, "workdir");
        json_set(params, "workdir", wd ? json_copy(wd) : json_null());
        json_set(params, "cwd", json_string(in_str(&ctx, "cwd", "/w")));
        json_set(params, "effective_pty", json_bool(in_bool(&ctx, "effective_pty")));
        const json_t *ck = json_obj_get(in, "cwd_key");
        json_set(params, "cwd_key", ck ? json_copy(ck) : json_null());
        json_set(params, "notify_on_complete", json_bool(in_bool(&ctx, "notify_on_complete")));
        const json_t *wp = json_obj_get(in, "watch_patterns");
        json_set(params, "watch_patterns", wp ? json_copy(wp) : json_null());
        const json_t *an = json_obj_get(in, "approval_note");
        json_set(params, "approval_note", an ? json_copy(an) : json_null());
        json_set(params, "completion_output_chars", json_number(json_get_num(in, "completion_output_chars", 0)));
        const json_t *pdr = json_obj_get(in, "pty_disabled_reason");
        json_set(params, "pty_disabled_reason", pdr ? json_copy(pdr) : json_null());
        json_set(params, "heartbeat_seconds", json_number(json_get_num(in, "heartbeat_seconds", 0)));
        json_set(params, "persist_on_release", json_bool(in_bool(&ctx, "persist_on_release")));
        const json_t *mh = json_obj_get(in, "mounted_host");
        json_set(params, "mounted_host", mh ? json_copy(mh) : json_null());
        json_set(params, "auto_backgrounded", json_bool(in_bool(&ctx, "auto_backgrounded")));
        json_t *session = NULL;
        char *r = ttlf_spawn_background_flow(params, &s, &session);
        json_free(params);
        json_set(out, "ret", r ? json_string(r) : json_null());
        json_set(out, "session", session ? session : json_null());
        free(r);
    } else if (strcmp(func, "finalize_foreground") == 0) {
        ttlf_finalize_seams_t s = {
            .ctx = &ctx, .record_session_cwd = fn_record_cwd, .sudo_annotations = fn_sudo,
            .transform_hook = fn_transform, .truncate = fn_truncate, .strip_ansi = fn_strip_ansi,
            .redact_output = fn_redact_output, .interpret_exit_code = fn_interpret,
            .failure_hint = fn_failure_hint, .realpath = fn_realpath, .redact_spill = fn_spill,
            .verification = fn_verification, .log = sc_log,
        };
        const json_t *result = in_obj(&ctx, "result");
        json_t *result_def = result ? NULL : json_object();
        char *r = ttlf_finalize_foreground_result_flow(
            in_str(&ctx, "command", "ls"), result ? result : result_def,
            in_obj(&ctx, "env"), in_str(&ctx, "env_type", "local"),
            in_str(&ctx, "effective_task_id", "t1"),
            json_get_str(in, "task_id", NULL), json_get_str(in, "session_id", NULL),
            in_str(&ctx, "session_key", "sk"), json_get_str(in, "workdir", NULL),
            json_get_str(in, "command_cwd", NULL), json_get_str(in, "approval_note", NULL), &s);
        json_free(result_def);
        json_set(out, "ret", r ? json_string(r) : json_null());
        free(r);
    } else if (strcmp(func, "cwd_fallback_note") == 0) {
        char *r = ttlf_cwd_fallback_note(json_obj_get(in, "fallback"));
        json_set(out, "ret", r ? json_string(r) : json_null());
        free(r);
    } else if (strcmp(func, "interpret_exit_code") == 0) {
        ttlf_iec_seams_t s = { .ctx = &ctx, .signal_note = iec_signal };
        char *r = ttlf_interpret_exit_code_flow(in_str(&ctx, "command", ""),
                                                (int)json_get_num(in, "exit_code", 0), &s);
        json_set(out, "ret", r ? json_string(r) : json_null());
        free(r);
    } else if (strcmp(func, "host_path_key") == 0) {
        char *r = ttlf_host_path_key(json_get_str(in, "path", NULL));
        json_set(out, "ret", r ? json_string(r) : json_null());
        free(r);
    } else if (strcmp(func, "translate_mounted_host_path") == 0) {
        char *r = ttlf_translate_mounted_host_path(
            json_get_str(in, "path", NULL), json_get_str(in, "host_root", NULL),
            json_get_str(in, "container_root", NULL));
        json_set(out, "ret", r ? json_string(r) : json_null());
        free(r);
    } else if (strcmp(func, "coerce_ssh_remote_cwd") == 0) {
        ttlf_ssh_seams_t s = {
            .ctx = &ctx, .is_container_launch_dir = ssh_launch_dir,
            .get_subprocess_home = ssh_subprocess_home, .get_real_home = ssh_real_home,
        };
        json_t *r = ttlf_coerce_ssh_remote_cwd_flow(json_obj_get(in, "cwd"),
                                                    json_get_str(in, "env_type", NULL), &s);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "looks_like_homebrew_ci_poller") == 0) {
        bool r = ttlf_looks_like_homebrew_ci_poller(in_str(&ctx, "command", ""));
        json_set(out, "ret", json_bool(r));
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    json_set(out, "trace", trace.events ? trace.events : json_array());
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    FILE *f = (argc > 1) ? fopen(argv[1], "rb") : stdin;
    if (!f) { fprintf(stderr, "cannot open fixture\n"); return 1; }
    char line[1 << 20];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '#') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case line: %s\n", err ? err : "not an object");
            free(err);
            json_free(c);
            continue;
        }
        const char *func = json_get_str(c, "func", "");
        const json_t *in = json_obj_get(c, "in");
        json_t *empty = json_object();
        json_t *out = json_object();
        run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
        /* Serialize a string node for func (bounds-safe); do NOT call
         * json_escape_string (off-by-one heap overflow in libjson). */
        json_t *func_node = json_string(func);
        char *func_s = json_serialize(func_node);
        json_free(func_node);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out);
        json_free(c);
    }
    if (f != stdin) fclose(f);
    return 0;
}
