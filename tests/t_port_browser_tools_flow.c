/* t_port_browser_tools_flow.c — harness for the tools/browser_use_cli.py
 * UNCLAIMED browser-tools-surface FLOW port (src/tools/browser_tools_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...args...,
 * "env": {env dict}, "state": {real_profile_fallback,real_profile_note_delivered},
 * "io": {seam: script}}}. Emits JSON LINES: {"func","in","out":{"ret","trace",
 * "state"}}. "ret" is the canonical return (tuples/NamedTuples -> arrays,
 * None -> null, uncaught exception -> {"__raise__": cls}); "trace" is the ordered
 * seam-call sequence [{"ev","args","ret"}]; "state" is the post-call state
 * ({env, real_profile_fallback, real_profile_note_delivered}; {} on any raise).
 * The oracle drives LIVE tools/browser_use_cli.py under the SAME scripts and
 * diffs out.
 *
 * Self-contained: #includes the area-qualified port .c (libjson-only).
 * Gate: bash tests/run_one_oracle.sh browser_tools_flow \
 *           oracle_browser_tools_flow.py "" "lib/libjson/json.o"
 */
#include "hermes_json.h"
#include "tools/browser_tools_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* materialize the module state object from in.state (owned) */
static json_t *bth_state(const json_t *in) {
    json_t *st = json_object();
    const json_t *src = bt_get(in, "state");
    json_t *rpf = json_object();
    const json_t *sr = bt_get(src, "real_profile_fallback");
    if (bt_is_obj(sr))
        for (size_t i = 0; i < sr->c.count; i++)
            json_set(rpf, sr->c.keys[i], json_copy(sr->c.items[i]));
    json_t *rpnd = json_array();
    const json_t *sd = bt_get(src, "real_profile_note_delivered");
    if (bt_is_arr(sd))
        for (size_t i = 0; i < sd->c.count; i++)
            json_append(rpnd, json_copy(sd->c.items[i]));
    json_set(st, "real_profile_fallback", rpf);
    json_set(st, "real_profile_note_delivered", rpnd);
    return st;
}

/* sort a string array in place (insertion order -> sorted, to match the
 * oracle's sorted set echo for real_profile_note_delivered). */
static void bth_sort_strs(json_t *arr) {
    if (!bt_is_arr(arr)) return;
    for (size_t i = 1; i < arr->c.count; i++) {
        json_t *key = arr->c.items[i];
        size_t j = i;
        while (j > 0 && strcmp(bt_jstr(arr->c.items[j - 1]), bt_jstr(key)) > 0) {
            arr->c.items[j] = arr->c.items[j - 1];
            j--;
        }
        arr->c.items[j] = key;
    }
}

/* does this function take a mutable env argument? */
static bool bth_takes_env(const char *func) {
    static const char *E[] = {
        "_set_cdp_env", "_has_cdp_env", "_mark_local_browser", "_export_real_profile_note",
        "_attach_vault_supervisor", "_record_driven_tab", "_published_daemon_pid",
        "_route_backend", "_route_exec",
    };
    for (size_t i = 0; i < sizeof(E) / sizeof(E[0]); i++)
        if (bt_str_eq(func, E[i])) return true;
    return false;
}

static json_t *bth_run(bt_ctx *ctx, const char *func, const json_t *in, json_t *env) {
    const char *s = json_get_str(in, "session", "");
    const char *task_id = json_get_str(in, "task_id", NULL);
    const char *key = json_get_str(in, "key", "");
    const char *route_key = json_get_str(in, "route_key", "");
    const char *code = json_get_str(in, "code", NULL);
    const char *name = json_get_str(in, "name", NULL);
    const char *cdp = json_get_str(in, "cdp", "");
    const char *module = json_get_str(in, "module", "");
    const char *lname = json_get_str(in, "lname", "");
    const char *log_prefix = json_get_str(in, "log_prefix", "");
    const char *fn_seam = json_get_str(in, "fn_seam", "fn");
    bool local = json_get_bool(in, "local", false);
    bool local_sidecar = json_get_bool(in, "local_sidecar", false);
    bool force_local_sidecar = json_get_bool(in, "force_local_sidecar", false);
    bool real_profile = json_get_bool(in, "real_profile", true);
    const json_t *session = bt_get(in, "session_obj");
    const json_t *browser_cfg = bt_get(in, "browser_cfg");
    const json_t *timeout_s = bt_get(in, "timeout_s");
    json_t *def = bt_get(in, "default");

    if (bt_str_eq(func, "_set_cdp_env")) return bt_set_cdp_env(ctx, env, cdp);
    if (bt_str_eq(func, "_has_cdp_env")) return bt_has_cdp_env(ctx, env);
    if (bt_str_eq(func, "_clamp_timeout")) return bt_clamp_timeout(ctx, timeout_s);
    if (bt_str_eq(func, "_no_code_error")) return bt_no_code_error(ctx, code);
    if (bt_str_eq(func, "_sidecar_daemon_name")) return bt_sidecar_daemon_name(ctx, name);
    if (bt_str_eq(func, "_served_profile_tag")) return bt_served_profile_tag(ctx);
    if (bt_str_eq(func, "_browser_use_api_key_set")) return bt_browser_use_api_key_set(ctx);
    if (bt_str_eq(func, "_use_gateway")) return bt_use_gateway(ctx, browser_cfg);
    if (bt_str_eq(func, "_harness_credentials")) return bt_harness_credentials(ctx);
    if (bt_str_eq(func, "_export_real_profile_note")) return bt_export_real_profile_note(ctx, env, key);
    if (bt_str_eq(func, "_mark_local_browser")) return bt_mark_local_browser(ctx, env, session, key);
    if (bt_str_eq(func, "_quiet")) return bt_quiet(ctx, fn_seam, def, log_prefix);
    if (bt_str_eq(func, "_lazy_call")) return bt_lazy_call(ctx, module, lname, def, log_prefix);
    if (bt_str_eq(func, "_real_profile_consented")) return bt_real_profile_consented(ctx);
    if (bt_str_eq(func, "_code_url_verdict")) return bt_code_url_verdict(ctx, code);
    if (bt_str_eq(func, "_attach_vault_supervisor")) return bt_attach_vault_supervisor(ctx, env, task_id);
    if (bt_str_eq(func, "_record_driven_tab")) return bt_record_driven_tab(ctx, env, task_id);
    if (bt_str_eq(func, "_published_daemon_pid")) return bt_published_daemon_pid(ctx, env);
    if (bt_str_eq(func, "_take_real_profile_note")) return bt_take_real_profile_note(ctx, route_key);
    if (bt_str_eq(func, "_route_backend"))
        return bt_route_backend(ctx, env, s, task_id, local, force_local_sidecar, real_profile);
    if (bt_str_eq(func, "_route_exec"))
        return bt_route_exec(ctx, env, s, task_id, local, local_sidecar, route_key);
    if (bt_str_eq(func, "browser_exec_available")) return bt_browser_exec_available(ctx);
    return NULL;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    static char line[1 << 20];
    while (fgets(line, sizeof(line), stdin)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '\r') continue;
        json_t *rec = json_parse(line, NULL);
        if (!rec) { fprintf(stderr, "harness: unparseable case line\n"); return 1; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        json_t *empty_in = NULL;
        if (!in) { empty_in = json_object(); in = empty_in; }
        bt_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.state = bth_state(in);
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        bool takes_env = bth_takes_env(func);
        json_t *env = NULL;
        if (takes_env) {
            const json_t *ie = bt_get(in, "env");
            env = bt_is_obj(ie) ? json_copy(ie) : json_object();
        }
        json_t *ret = bth_run(&ctx, func, in, env);
        json_t *ret_out;
        json_t *state_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_string(bt_exc_class(&ctx)));
            json_free(ctx.exc);
            json_free(ret);
            json_free(ctx.state);
            json_free(env);
            state_out = json_object();  /* {} on any raise (both sides) */
        } else {
            ret_out = ret ? ret : json_null();
            bth_sort_strs(bt_get(ctx.state, "real_profile_note_delivered"));
            state_out = json_object();
            json_set(state_out, "env", env ? env : json_null());
            json_set(state_out, "real_profile_fallback", json_copy(bt_get(ctx.state, "real_profile_fallback")));
            json_set(state_out, "real_profile_note_delivered", json_copy(bt_get(ctx.state, "real_profile_note_delivered")));
            json_free(ctx.state);
        }
        json_t *out = json_object();
        json_set(out, "ret", ret_out);
        json_set(out, "trace", ctx.trace);
        json_set(out, "state", state_out);
        json_t *emit = json_object();
        json_set(emit, "func", json_string(func));
        json_set(emit, "in", json_copy(in));
        json_set(emit, "out", out);
        char *s = json_serialize(emit);
        if (s) { fputs(s, stdout); fputc('\n', stdout); free(s); }
        json_free(emit);
        json_free(ctx.counters);
        json_free(empty_in);
        json_free(rec);
    }
    return 0;
}