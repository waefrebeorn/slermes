/*
 * t_port_gateway_revive.c — oracle harness for the revived gateway gap ports:
 *   src/hermes_cli/gateway_gaps2.c  (gw2_* universal seams)
 *   src/gateway/gateway_gaps.c      (gwg_* universal seams)
 *   src/gateway/run_gaps.c          (grg_* universal seams)
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * One generic recording seam serves all three ports: it looks up the case's
 * in.io[<seam name>] script, records {"ev": name, "args": ..., "ret": script}
 * in call order, and returns the scripted value. Script shapes:
 *   - a plain JSON value: returned on EVERY call (ret marker = the value)
 *   - {"__seq__": [v1, v2, ...]}: consumed one per call; after exhaustion the
 *     LAST element repeats (stable clocks / repeated probes)
 *   - {"__raise__": kind, "msg": text}: returned as-is; the port detects it
 *   - absent: NULL (Python None), ret marker null
 * A scripted JSON null returns a NULL pointer (not a json_null node), so
 * truthiness-tested seam results short-circuit exactly like Python None.
 *
 * Emits {"func": ..., "in": ..., "out": {"ret": ..., "trace": [...],
 * "state": ...}}; tests/oracle_gateway_revive.py drives the LIVE Python with
 * the same scripts and diffs the ordered trace + ret (+ mutated config).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway_gaps2.c"
#include "gateway_gaps.c"
#include "run_gaps.c"

/* run_gaps.c extern (telegram topic normalization lives elsewhere). */
char *normalize_telegram_topic_profile_name(const char *profile_name) {
    return strdup(profile_name ? profile_name : "default");
}

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
    json_set(e, "ret", val ? json_copy(val) : json_null());
    json_append(c->trace, e);
    if (!val || val->type == JSON_NULL) return NULL;
    return json_copy(val);
}

static const json_t *node(const json_t *in, const char *k) {
    return json_obj_get(in, k);
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, json_array(), json_object() };
    gw2_seams_t s2 = { &ctx, script_call };
    gwg_seams_t sg = { &ctx, script_call };
    grg_seams_t sr = { &ctx, script_call };
    json_t *r = NULL;

    if (strcmp(func, "probe_loop_tick_socket") == 0) {
        r = probe_loop_tick_socket((int)json_get_num(in, "pid", 0),
                                   json_get_str(in, "home", NULL),
                                   json_get_num(in, "timeout", 1.0), &s2);
    } else if (strcmp(func, "probe_loop_tick_tcp") == 0) {
        r = probe_loop_tick_tcp(node(in, "port"), json_get_num(in, "timeout", 1.0), &s2);
    } else if (strcmp(func, "probe_loop_tick_socket_sustained") == 0) {
        r = probe_loop_tick_socket_sustained((int)json_get_num(in, "pid", 0),
                                             json_get_str(in, "home", NULL),
                                             json_get_num(in, "timeout", 1.0),
                                             node(in, "strikes"),
                                             json_get_num(in, "gap_s", 0.2),
                                             node(in, "tcp_port"), &s2);
    } else if (strcmp(func, "parse_launchd_pid_from_print_output") == 0) {
        r = parse_launchd_pid_from_print_output(json_get_str(in, "output", ""));
    } else if (strcmp(func, "launchd_print_service_pid") == 0) {
        r = launchd_print_service_pid(json_get_str(in, "domain", ""),
                                      json_get_str(in, "label", ""), &s2);
    } else if (strcmp(func, "reaper_candidate_is_supervisor_owned") == 0) {
        r = reaper_candidate_is_supervisor_owned((int)json_get_num(in, "pid", 0), &s2);
    } else if (strcmp(func, "windows_scheduled_task_supervises") == 0) {
        r = windows_scheduled_task_supervises(json_get_str(in, "task_name", ""), &s2);
    } else if (strcmp(func, "windows_console_window_attached") == 0) {
        r = windows_console_window_attached(&s2);
    } else if (strcmp(func, "windows_gateway_breakaway_state") == 0) {
        r = windows_gateway_breakaway_state(&s2);
    } else if (strcmp(func, "launchctl_label_supervising_process") == 0) {
        r = launchctl_label_supervising_process(json_get_str(in, "label", ""), &s2);
    } else if (strcmp(func, "wait_for_launchd_service_pid") == 0) {
        r = wait_for_launchd_service_pid(json_get_str(in, "label", ""),
                                         node(in, "old_pid"),
                                         json_get_num(in, "timeout", 10.0),
                                         json_get_str(in, "domain", ""), &s2);
    } else if (strcmp(func, "named_profile_served_by_running_multiplexer") == 0) {
        r = named_profile_served_by_running_multiplexer(node(in, "profile_name"), &s2);
    } else if (strcmp(func, "gateway_plugin_keyless_inject_launched_by_classic_cli") == 0) {
        r = gateway_plugin_keyless_inject_launched_by_classic_cli(node(in, "runner"),
                                                                  node(in, "live"), &sg);
    } else if (strcmp(func, "gateway_session_command_sink_active_sink") == 0) {
        r = gateway_session_command_sink_active_sink(&sg);
    } else if (strcmp(func, "gateway_status_scoped_locks_scoped_lock_record_is_stale") == 0) {
        r = gateway_status_scoped_locks_scoped_lock_record_is_stale(node(in, "existing"),
                                                                    node(in, "existing_pid"), &sg);
    } else if (strcmp(func, "gateway_systemd_stop_mark_resolve_target_pid") == 0) {
        r = gateway_systemd_stop_mark_resolve_target_pid(node(in, "argv"), &sg);
    } else if (strcmp(func, "gateway_session_skin_resolved_skin") == 0) {
        r = gateway_session_skin_resolved_skin(&sg);
    } else if (strcmp(func, "hermes_cli_gateway_side_connection_for") == 0) {
        r = hermes_cli_gateway_side_connection_for(node(in, "cli"), &sg);
    } else if (strcmp(func, "warm_turn_machinery_sync") == 0) {
        r = warm_turn_machinery_sync(&sr);
    } else if (strcmp(func, "active_deferred_agent_worker_count") == 0) {
        json_t *runner = json_copy(node(in, "runner"));
        /* The oracle builds a dict keyed by future id; the fixture carries a
         * list. Convert it so the C port sees the same shape. */
        if (runner && runner->type == JSON_OBJECT) {
            json_t *w = json_obj_get(runner, "_deferred_agent_workers");
            if (w && w->type == JSON_ARRAY) {
                json_t *map = json_object();
                for (size_t i = 0; i < w->c.count; i++) {
                    json_t *e = w->c.items[i];
                    const char *fid = json_get_str(e, "future", "");
                    json_set(map, fid, json_copy(json_obj_get(e, "agent")));
                }
                json_set(runner, "_deferred_agent_workers", map);
            }
        }
        r = active_deferred_agent_worker_count(runner, &sr);
        json_free(runner);
    } else if (strcmp(func, "interrupt_deferred_agent_workers") == 0) {
        json_t *runner = json_copy(node(in, "runner"));
        if (runner && runner->type == JSON_OBJECT) {
            json_t *w = json_obj_get(runner, "_deferred_agent_workers");
            if (w && w->type == JSON_ARRAY) {
                json_t *map = json_object();
                for (size_t i = 0; i < w->c.count; i++) {
                    json_t *e = w->c.items[i];
                    json_set(map, json_get_str(e, "future", ""),
                             json_copy(json_obj_get(e, "agent")));
                }
                json_set(runner, "_deferred_agent_workers", map);
            }
        }
        r = interrupt_deferred_agent_workers(runner,
                                             json_get_str(in, "reason", ""), &sr);
        json_free(runner);
    } else if (strcmp(func, "redeliver_failed_obligations_for_platform") == 0) {
        r = redeliver_failed_obligations_for_platform(node(in, "gateway"),
                                                      json_get_str(in, "platform", ""),
                                                      node(in, "profile"), &sr);
    } else if (strcmp(func, "instantiate_adapter") == 0) {
        const json_t *cfg_in = node(in, "config");
        json_t *config = cfg_in ? json_copy(cfg_in) : json_object();
        r = instantiate_adapter(node(in, "self_config"), node(in, "platform"), config, &sr);
        json_set(out, "state", config);
    } else if (strcmp(func, "drain_restart_safe_cron_deliveries") == 0) {
        r = drain_restart_safe_cron_deliveries(node(in, "adapters"), node(in, "loop"),
                                               node(in, "runner"), &sr);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", ctx.trace);
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