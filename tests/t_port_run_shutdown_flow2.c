/*
 * t_port_run_shutdown_flow2.c — oracle harness for the gateway/run_shutdown.py
 * UNCLAIMED-remainder FLOW cores ported in src/gateway/run_shutdown_flow2.c.
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
 * Emits {"func": ..., "in": ..., "out": {"ret": ..., "trace": [...],
 * "state": ...}}; tests/oracle_run_shutdown_flow2.py drives the LIVE Python
 * with the same scripts and diffs the ordered trace + ret + mutated state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/run_shutdown_flow2.c"

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

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, json_array(), json_object() };
    rs2_seams_t s = { &ctx, script_call };
    json_t *r = NULL;
    json_t *state = NULL;
    const json_t *self = node(in, "self");

    if (strcmp(func, "send_failed") == 0) {
        r = rs2_send_failed(node(in, "result"));
    } else if (strcmp(func, "send_error") == 0) {
        r = rs2_send_error(node(in, "result"));
    } else if (strcmp(func, "notice_target_key") == 0) {
        r = rs2_notice_target_key(json_get_str(in, "platform", ""),
                                  node(in, "chat_id"), node(in, "thread_id"));
    } else if (strcmp(func, "delivery_target_key") == 0) {
        r = rs2_delivery_target_key(json_get_str(in, "platform", ""),
                                    node(in, "chat_id"), node(in, "thread_id"),
                                    json_get_str(in, "profile", NULL), &s);
    } else if (strcmp(func, "restart_notification_allowed") == 0) {
        r = rs2_restart_notification_allowed(self, json_get_str(in, "platform", ""));
    } else if (strcmp(func, "log_suppressed") == 0) {
        r = rs2_log_suppressed((int)json_get_num(in, "level", 10),
                               json_get_str(in, "msg", ""), node(in, "args"),
                               json_get_bool(in, "exc_info", false), &s);
    } else if (strcmp(func, "effective_watchdog_leash") == 0) {
        r = rs2_effective_watchdog_leash(self, &s);
    } else if (strcmp(func, "active_async_delegation_count") == 0) {
        r = rs2_active_async_delegation_count(&s);
    } else if (strcmp(func, "running_cron_job_count") == 0) {
        r = rs2_running_cron_job_count(&s);
    } else if (strcmp(func, "active_cron_job_count") == 0) {
        r = rs2_active_cron_job_count(self, &s);
    } else if (strcmp(func, "api_server_hook") == 0) {
        r = rs2_api_server_hook(self, json_get_str(in, "name", ""), node(in, "args"), &s);
    } else if (strcmp(func, "active_api_run_count") == 0) {
        r = rs2_active_api_run_count(self, &s);
    } else if (strcmp(func, "active_api_worker_count") == 0) {
        r = rs2_active_api_worker_count(self, &s);
    } else if (strcmp(func, "mark_api_runs_shutdown_requested") == 0) {
        r = rs2_mark_api_runs_shutdown_requested(self, &s);
    } else if (strcmp(func, "active_work_count") == 0) {
        r = rs2_active_work_count(self, &s);
    } else if (strcmp(func, "drain_work_counts") == 0) {
        r = rs2_drain_work_counts(self, &s);
    } else if (strcmp(func, "gateway_cfg_section") == 0) {
        r = rs2_gateway_cfg_section(json_get_str(in, "name", ""), &s);
    } else if (strcmp(func, "scale_to_zero_idle_timeout_seconds") == 0) {
        r = rs2_scale_to_zero_idle_timeout_seconds(self, &s);
    } else if (strcmp(func, "relay_wake_url_or_none") == 0) {
        r = rs2_relay_wake_url_or_none(&s);
    } else if (strcmp(func, "scale_to_zero_hold_redial") == 0) {
        r = rs2_scale_to_zero_hold_redial(self, json_get_bool(in, "held", true), &s);
    } else if (strcmp(func, "scale_to_zero_abandon_suspend") == 0) {
        json_t *selfc = self ? json_copy(self) : json_object();
        r = rs2_scale_to_zero_abandon_suspend(selfc, &s);
        state = selfc;
    } else if (strcmp(func, "restart_wait_cron_counts") == 0) {
        r = rs2_restart_wait_cron_counts(self, &s);
    } else if (strcmp(func, "wedged_cron_job_count") == 0) {
        r = rs2_wedged_cron_job_count(self, &s);
    } else if (strcmp(func, "restart_safe_cron_count") == 0) {
        r = rs2_restart_safe_cron_count(self, &s);
    } else if (strcmp(func, "wedged_agent_count") == 0) {
        r = rs2_wedged_agent_count(self, &s);
    } else if (strcmp(func, "awaitable_work_count") == 0) {
        r = rs2_awaitable_work_count(self, &s);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", ctx.trace);
    json_set(out, "state", state ? state : json_null());
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