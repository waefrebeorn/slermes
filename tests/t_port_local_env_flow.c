/*
 * t_port_local_env_flow.c — oracle harness for the local execution-environment
 * FLOW cores ported in src/tools/environments/local_env_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * Wires the universal seam to a SCRIPTED recording stub (in["io"][name]: a single
 * value reused, a bare array consumed per call, or {"__script__":true,"ret":X}
 * for a constant list) that appends one {ev,args,ret} trace event per call, in
 * call order. Runs the flow on COPIES of the state objects (env / result) and
 * emits {"func","in","out":{"ret","exc","trace",<state>}} per case. The original
 * "in" is echoed unmutated. tests/oracle_local_env_flow.py drives the LIVE Python
 * with the same scripts and compares ordered trace + ret + state + exception.
 *
 * Self-contained: #includes the port .c; links only libjson.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "tools/environments/local_env_flow.c"

typedef struct {
    const json_t *in;
    lef_trace_t *trace;
    json_t *counts;   /* object: seam name -> call count (array scripts) */
} script_ctx_t;

/* ── generic scripted recording seam ── */
static json_t *sc_call(void *ctx, const char *name, const json_t *args) {
    script_ctx_t *c = ctx;
    const json_t *io = json_obj_get(c->in, "io");
    const json_t *script = io ? json_obj_get(io, name) : NULL;
    const json_t *val = NULL;
    if (script && script->type == JSON_OBJECT && json_get_bool(script, "__script__", false)) {
        val = json_obj_get(script, "ret");   /* constant (list) result */
    } else if (script && script->type == JSON_ARRAY) {
        double cnt = json_get_num(c->counts, name, 0);
        size_t idx = (size_t)cnt;
        if (script->c.count) {
            if (idx >= script->c.count) idx = script->c.count - 1;
            val = script->c.items[idx];
        }
        json_set(c->counts, name, json_number(cnt + 1));
    } else {
        val = script;   /* single value reused (NULL when absent) */
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args ? json_copy(args) : json_object());
    json_set(e, "ret", val ? json_copy(val) : json_null());
    lef_trace_add(c->trace, e);
    /* a scripted JSON null returns a NULL pointer (None), not a json_null node */
    if (!val || val->type == JSON_NULL) return NULL;
    return json_copy(val);
}

/* ── input readers ── */
static const json_t *in_node(const json_t *in, const char *k) { return json_obj_get(in, k); }
static const char *in_str(const json_t *in, const char *k, const char *def) { return json_get_str(in, k, def); }
static bool in_bool(const json_t *in, const char *k) { return json_get_bool(in, k, false); }
static double in_num(const json_t *in, const char *k, double def) { return json_get_num(in, k, def); }
static json_t *copy_obj(const json_t *in, const char *k) {
    const json_t *n = json_obj_get(in, k);
    json_t *c = n ? json_copy(n) : json_object();
    if (!c || c->type != JSON_OBJECT) { json_free(c); c = json_object(); }
    return c;
}

/* ── per-case dispatch ── */
static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, NULL, json_object() };
    lef_trace_t trace = { NULL };
    ctx.trace = &trace;
    lef_seams_t s = { &ctx, sc_call };
    json_t *exc = NULL;
    json_t *ret = NULL;
    char *ret_str = NULL;
    bool ret_bool = false;
    bool has_bool = false;

    /* reserve envelope positions (ret, exc, trace) before state keys */
    json_set(out, "ret", json_null());
    json_set(out, "exc", json_null());
    json_set(out, "trace", json_array());

    if (strcmp(func, "prepend_missing_path_entries") == 0) {
        ret_str = lef_prepend_missing_path_entries(in_str(in, "existing_path", ""),
                                                   in_node(in, "dirs"),
                                                   in_str(in, "pathsep", ":"));
    } else if (strcmp(func, "append_dropped_path_entries") == 0) {
        ret_str = lef_append_dropped_path_entries(in_str(in, "cmd_string", ""),
                                                  in_str(in, "parent_path", ""),
                                                  in_bool(in, "is_windows"), &s, &exc);
    } else if (strcmp(func, "compute_git_bash_bin_dirs") == 0) {
        ret = lef_compute_git_bash_bin_dirs(&s, &exc);
    } else if (strcmp(func, "user_tools_first") == 0) {
        ret_str = lef_user_tools_first(in_str(in, "existing_path", ""),
                                       in_str(in, "pathsep", ":"), &s, &exc);
    } else if (strcmp(func, "terminal_env") == 0) {
        json_t *env = copy_obj(in, "env");
        json_t *r = lef_terminal_env(env, in_str(in, "pathsep", ":"), &s, &exc);
        json_set(out, "env", env);
        if (r) ret = json_copy(r);
    } else if (strcmp(func, "apply_profile_home") == 0) {
        json_t *env = copy_obj(in, "env");
        lef_apply_profile_home(env, &s, &exc);
        json_set(out, "env", env);
    } else if (strcmp(func, "carry_bridge_markers") == 0) {
        json_t *env = copy_obj(in, "env");
        lef_carry_bridge_markers(env, in_node(in, "source"),
                                 in_bool(in, "own_profile"), &s, &exc);
        json_set(out, "env", env);
    } else if (strcmp(func, "restore_bootstrap_tokens") == 0) {
        json_t *env = copy_obj(in, "env");
        lef_restore_bootstrap_tokens(env, in_node(in, "source"), &s, &exc);
        json_set(out, "env", env);
    } else if (strcmp(func, "usable_cwd") == 0) {
        ret = lef_usable_cwd(in_str(in, "cwd", ""), in_str(in, "self_cwd", ""), &s, &exc);
    } else if (strcmp(func, "parse_cwd_marker") == 0) {
        json_t *result = copy_obj(in, "result");
        lef_parse_cwd_marker(result, &s, &exc);
        json_set(out, "result", result);
    } else if (strcmp(func, "leader_is_ours") == 0) {
        ret_bool = lef_leader_is_ours(in_node(in, "pgid"), in_node(in, "expected_start"), &s, &exc);
        has_bool = true;
    } else if (strcmp(func, "wait_for_group_exit") == 0) {
        ret_bool = lef_wait_for_group_exit(in_node(in, "proc"), in_node(in, "pgid"),
                                           in_num(in, "timeout", 1.0), &s, &exc);
        has_bool = true;
    } else if (strcmp(func, "sweep_escaped_descendants") == 0) {
        lef_sweep_escaped_descendants(in_node(in, "descendants"), in_node(in, "pgid"), &s, &exc);
    } else if (strcmp(func, "kill_known_pids") == 0) {
        lef_kill_known_pids(in_node(in, "proc"), in_node(in, "descendants"), &s, &exc);
    } else if (strcmp(func, "kill_process_group_posix") == 0) {
        lef_kill_process_group_posix(in_node(in, "proc"), in_bool(in, "psutil_import_fail"), &s, &exc);
    } else if (strcmp(func, "kill_process_windows") == 0) {
        lef_kill_process_windows(in_node(in, "proc"), in_bool(in, "status_import_fail"), &s, &exc);
    } else if (strcmp(func, "force_kill_process") == 0) {
        lef_force_kill_process(in_node(in, "proc"), in_bool(in, "is_windows"), &s, &exc);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    /* ret envelope: on a propagated exception the reference projects ret=None */
    if (exc) {
        json_set(out, "ret", json_null());
        if (ret_str) free(ret_str);
        if (ret) json_free(ret);
    } else if (ret_str) {
        json_set(out, "ret", json_string(ret_str));
        free(ret_str);
    } else if (ret) {
        json_set(out, "ret", ret);
    } else if (has_bool) {
        json_set(out, "ret", json_bool(ret_bool));
    } else {
        json_set(out, "ret", json_null());
    }
    json_set(out, "exc", exc ? exc : json_null());
    json_set(out, "trace", trace.events ? trace.events : json_array());
    json_free(ctx.counts);
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