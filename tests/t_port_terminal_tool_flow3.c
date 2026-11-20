/*
 * t_port_terminal_tool_flow3.c — oracle harness for the tools/terminal_tool.py
 * UNCLAIMED-remainder FLOW cores ported in src/tools/terminal_tool_flow3.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * One generic recording seam serves every flow: it looks up the case's
 * in.io[<seam name>] script, records {"ev": name, "args": ..., "ret": script}
 * in call order, and returns the scripted value. Script shapes:
 *   - plain JSON value: returned on EVERY call
 *   - {"__seq__": [v1, v2, ...]}: consumed one per call; last repeats; [] -> None
 *   - {"__raise__": kind, "msg": text}: returned as-is; the port detects it
 *   - scripted JSON null: returns NULL (Python None)
 *   - ABSENT key: FAIL-CLOSED RuntimeError "no script for <name>" (recorded,
 *     then raised), so an unscripted seam raises instead of reading None.
 *
 * Module state (in.state) is copied, normalized to the full key set
 * (docker_orphan_reaper_ran, container_aliases, task_env_overrides), mutated in
 * place by the flow and echoed as out.state. Emits
 * {"func": ..., "in": ..., "out": {"ret": ..., "trace": [...], "state": ...}};
 * tests/oracle_terminal_tool_flow3.py drives the LIVE Python with the same
 * scripts and diffs ordered trace + ret + mutated state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "tools/terminal_tool_flow3.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *seq_pos;
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

/* Normalize injected state to the full key set the oracle echoes. */
static json_t *norm_state(const json_t *in_state) {
    json_t *st = in_state ? json_copy(in_state) : json_object();
    if (st->type != JSON_OBJECT) { json_free(st); st = json_object(); }
    if (!json_obj_get(st, "docker_orphan_reaper_ran"))
        json_set(st, "docker_orphan_reaper_ran", json_bool(false));
    static const char *OBJ_KEYS[] = { "container_aliases", "task_env_overrides" };
    for (size_t i = 0; i < sizeof(OBJ_KEYS) / sizeof(OBJ_KEYS[0]); i++) {
        const json_t *o = json_obj_get(st, OBJ_KEYS[i]);
        if (!o || o->type != JSON_OBJECT) json_set(st, OBJ_KEYS[i], json_object());
    }
    return st;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, json_array(), json_object() };
    tt3_seams_t s = { &ctx, script_call };
    json_t *st = norm_state(node(in, "state"));
    json_t *r = NULL;

    if (strcmp(func, "command_requires_pipe_stdin") == 0) {
        r = tt3_command_requires_pipe_stdin(json_get_str(in, "command", ""));
    } else if (strcmp(func, "resolve_notification_flag_conflict") == 0) {
        r = tt3_resolve_notification_flag_conflict(
            json_get_bool(in, "notify_on_complete", false), node(in, "watch_patterns"),
            json_get_bool(in, "background", false));
    } else if (strcmp(func, "isolation_key") == 0) {
        r = tt3_isolation_key(json_get_str(in, "key", ""));
    } else if (strcmp(func, "session_isolated") == 0) {
        r = tt3_session_isolated(json_get_str(in, "env_type", ""),
                                 json_get_bool(in, "persistent", false), &s);
    } else if (strcmp(func, "docker_session_isolated") == 0) {
        r = tt3_docker_session_isolated(json_get_str(in, "env_type", ""),
                                        json_get_bool(in, "persistent", false), &s);
    } else if (strcmp(func, "docker_profile_scoped") == 0) {
        r = tt3_docker_profile_scoped(json_get_str(in, "env_type", ""),
                                      json_get_bool(in, "persistent", false));
    } else if (strcmp(func, "session_scope") == 0) {
        r = tt3_session_scope(&s);
    } else if (strcmp(func, "docker_session_isolation_enabled") == 0) {
        r = tt3_docker_session_isolation_enabled(&s);
    } else if (strcmp(func, "has_isolation_overrides") == 0) {
        r = tt3_has_isolation_overrides(json_get_str(in, "task_id", NULL), st, &s);
    } else if (strcmp(func, "register_container_alias") == 0) {
        r = tt3_register_container_alias(json_get_str(in, "child", ""), node(in, "parent"), st);
    } else if (strcmp(func, "maybe_reap_docker_orphans") == 0) {
        r = tt3_maybe_reap_docker_orphans(node(in, "config"), st, &s);
    } else if (strcmp(func, "refuse_missing_local_workdir") == 0) {
        r = tt3_refuse_missing_local_workdir(json_get_str(in, "workdir", ""), node(in, "env"), &s);
    } else if (strcmp(func, "resolve_command_cwd") == 0) {
        r = tt3_resolve_command_cwd(json_get_str(in, "workdir", NULL),
                                    json_get_str(in, "default_cwd", ""),
                                    json_get_str(in, "session_key", NULL),
                                    json_get_str(in, "env_type", NULL),
                                    node(in, "mounted_host"), node(in, "env"), &s);
    } else if (strcmp(func, "resolve_task_host_cwd") == 0) {
        r = tt3_resolve_task_host_cwd(node(in, "config"), json_get_str(in, "task_id", NULL), &s);
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
