/*
 * t_port_terminal_tool_flow2.c — oracle harness for the tools/terminal_tool.py
 * UNCLAIMED-remainder FLOW cores ported in src/tools/terminal_tool_flow2.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * One generic recording seam serves every flow: it looks up the case's
 * in.io[<seam name>] script, records {"ev": name, "args": ..., "ret": script}
 * in call order, and returns the scripted value. Script shapes:
 *   - a plain JSON value: returned on EVERY call (ret marker = the value)
 *   - {"__seq__": [v1, v2, ...]}: consumed one per call; after exhaustion the
 *     LAST element repeats (stable clocks / repeated probes); empty -> None
 *   - {"__raise__": kind, "msg": text}: returned as-is; the port detects it
 *   - a scripted JSON null: returns a NULL pointer (Python None)
 *   - ABSENT key: FAIL-CLOSED — returns {"__raise__":"RuntimeError",
 *     "msg":"no script for <name>"} (recorded as ret), so an unscripted seam
 *     raises rather than silently reading None.
 *
 * Module state (in.state) is copied, normalized to the full key set
 * (active_environments, last_activity, task_env_overrides, container_aliases,
 * creation_locks, os_name), mutated in place by the flow and echoed as
 * out.state. Emits {"func": ..., "in": ..., "out": {"ret": ..., "trace": [...],
 * "state": ...}}; tests/oracle_terminal_tool_flow2.py drives the LIVE Python
 * with the same scripts and diffs the ordered trace + ret + mutated state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "tools/terminal_tool_flow2.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *seq_pos;   /* seam name -> next sequence index */
} script_ctx_t;

static json_t *script_call(void *ctx, const char *name, const json_t *args) {
    script_ctx_t *c = (script_ctx_t *)ctx;
    const json_t *io = json_obj_get(c->in, "io");
    const json_t *script = io ? json_obj_get(io, name) : NULL;
    const json_t *val = script;
    if (script && script->type == JSON_OBJECT) {
        const json_t *seq = json_obj_get(script, "__seq__");
        if (seq && seq->type == JSON_ARRAY) {
            const json_t *pos = json_obj_get(c->seq_pos, name);
            size_t idx = (pos && pos->type == JSON_NUMBER) ? (size_t)pos->num_val : 0;
            size_t n = json_len(seq);
            if (n == 0) {
                val = NULL;
            } else {
                if (idx >= n) idx = n - 1;
                val = json_get(seq, idx);
                json_set(c->seq_pos, name, json_number((double)(idx + 1)));
            }
        }
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args ? json_copy(args) : json_object());
    if (!script) {
        /* fail-closed: no script for this seam */
        json_t *rs = json_object();
        json_set(rs, "__raise__", json_string("RuntimeError"));
        char m[160];
        snprintf(m, sizeof(m), "no script for %s", name);
        json_set(rs, "msg", json_string(m));
        json_set(e, "ret", json_copy(rs));
        json_append(c->trace, e);
        return rs;
    }
    json_set(e, "ret", val ? json_copy(val) : json_null());
    json_append(c->trace, e);
    if (!val || val->type == JSON_NULL) return NULL;
    return json_copy(val);
}

static const json_t *node(const json_t *in, const char *k) {
    return json_obj_get(in, k);
}

/* Normalize the injected state to the full key set the oracle echoes. */
static json_t *norm_state(const json_t *in_state) {
    json_t *st = in_state ? json_copy(in_state) : json_object();
    if (st->type != JSON_OBJECT) { json_free(st); st = json_object(); }
    static const char *OBJ_KEYS[] = {
        "active_environments", "last_activity", "task_env_overrides",
        "container_aliases", "creation_locks",
    };
    for (size_t i = 0; i < sizeof(OBJ_KEYS) / sizeof(OBJ_KEYS[0]); i++) {
        const json_t *o = json_obj_get(st, OBJ_KEYS[i]);
        if (!o || o->type != JSON_OBJECT) json_set(st, OBJ_KEYS[i], json_object());
    }
    if (!json_obj_get(st, "os_name")) json_set(st, "os_name", json_string("posix"));
    return st;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, json_array(), json_object() };
    tt2_seams_t s = { &ctx, script_call };
    json_t *st = norm_state(node(in, "state"));
    json_t *r = NULL;

    if (strcmp(func, "container_profile") == 0) {
        r = tt2_container_profile(&s);
    } else if (strcmp(func, "sanitize_cwd_for_live_env") == 0) {
        r = tt2_sanitize_cwd_for_live_env(node(in, "env"), json_get_str(in, "new_cwd", ""), &s);
    } else if (strcmp(func, "shell_state_chain") == 0) {
        r = tt2_shell_state_chain(st, json_get_str(in, "task_id", NULL));
    } else if (strcmp(func, "session_scope") == 0) {
        r = tt2_session_scope(&s);
    } else if (strcmp(func, "routed_home_task_key") == 0) {
        r = tt2_routed_home_task_key(json_get_bool(in, "profile_scoped", false), &s);
    } else if (strcmp(func, "sessionless_container_key") == 0) {
        r = tt2_sessionless_container_key(json_get_bool(in, "profile_scoped", false), &s);
    } else if (strcmp(func, "workspace_key_for") == 0) {
        r = tt2_workspace_key_for(json_get_str(in, "cwd", ""), &s);
    } else if (strcmp(func, "workspace_container_key") == 0) {
        r = tt2_workspace_container_key(json_get_str(in, "base", ""),
                                        json_get_str(in, "task_id", NULL), st, &s);
    } else if (strcmp(func, "select_image") == 0) {
        r = tt2_select_image(json_get_str(in, "env_type", ""), node(in, "overrides"), node(in, "config"));
    } else if (strcmp(func, "lookup_active_env") == 0) {
        r = tt2_lookup_active_env(json_get_str(in, "effective_task_id", ""),
                                  json_get_str(in, "task_id", NULL), st, &s);
    } else if (strcmp(func, "session_workspace_mount_source") == 0) {
        r = tt2_session_workspace_mount_source(json_get_str(in, "task_id", NULL), &s);
    } else if (strcmp(func, "backend_default_cwd") == 0) {
        r = tt2_backend_default_cwd(json_get_str(in, "env_type", ""), &s);
    } else if (strcmp(func, "resolve_config_cwd") == 0) {
        r = tt2_resolve_config_cwd(json_get_str(in, "env_type", ""),
                                   json_get_bool(in, "mount_docker_cwd", false), st, &s);
    } else if (strcmp(func, "rewrite_via_env_mount") == 0) {
        r = tt2_rewrite_via_env_mount(json_get_str(in, "path", ""), node(in, "env"), &s);
    } else if (strcmp(func, "mount_envs") == 0) {
        r = tt2_mount_envs(node(in, "env"), st);
    } else if (strcmp(func, "container_visible_cwd") == 0) {
        r = tt2_container_visible_cwd(json_get_str(in, "path", ""), json_get_str(in, "env_type", NULL),
                                      node(in, "env"), st, &s);
    } else if (strcmp(func, "container_visible_default") == 0) {
        r = tt2_container_visible_default(json_get_str(in, "default_cwd", ""),
                                          json_get_str(in, "env_type", NULL), node(in, "env"), st, &s);
    } else if (strcmp(func, "fatal_error_json") == 0) {
        r = tt2_fatal_error_json(node(in, "exc"), &s);
    } else if (strcmp(func, "acquire_env") == 0) {
        r = tt2_acquire_env(node(in, "plan"), json_get_str(in, "task_id", NULL), st, &s);
    } else if (strcmp(func, "yield_kwargs") == 0) {
        r = tt2_yield_kwargs(json_get_str(in, "command", ""), node(in, "ctx"), &s);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", ctx.trace);
    json_set(out, "state", st);
    json_free(ctx.seq_pos);
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