/* t_port_agent_runtime_helpers_flow2.c — harness for the agent/agent_runtime_helpers.py
 * UNCLAIMED-remainder FLOW port (src/agent/agent_runtime_helpers_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...args..., "io": {seam:
 * script}, "env": {...}}}. Emits JSON LINES: {"func","in","out":{"ret","trace",
 * "state"}}. "ret" is the canonical return (tuples/sets -> arrays, None -> null,
 * uncaught exception -> {"__raise__": cls}); "trace" is the ordered seam-call
 * sequence [{"ev","args","ret"}]; "state" is the post-call out-state (mutated
 * messages / tool_msg / selected agent fields; {} otherwise, and {} on any
 * raise). The oracle drives LIVE agent/agent_runtime_helpers.py under the SAME
 * scripts and diffs out.
 *
 * Self-contained: #includes the area-qualified port .c (libjson-only).
 */
#include "hermes_json.h"
#include "agent/agent_runtime_helpers_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* echo selected agent fields as out-state (getattr default None). */
static json_t *arh2_h_echo(json_t *agent, const char *const *fields, size_t n) {
    json_t *st = json_object();
    for (size_t i = 0; i < n; i++)
        json_set(st, fields[i], arh2_copy_or_null(arh2_get(agent, fields[i])));
    return st;
}
static json_t *arh2_h_agent(json_t *in) {
    json_t *a = arh2_get(in, "agent");
    return a ? json_copy(a) : json_object();
}

static json_t *arh2_harness_run(arh2_ctx *ctx, const char *func, json_t *in, json_t **state_out) {
    *state_out = NULL;

    if (strcmp(func, "_prepend_corruption_marker") == 0) {
        json_t *tm = json_copy(arh2_get(in, "tool_msg"));
        if (!tm) tm = json_object();
        json_t *r = arh2_prepend_corruption_marker(ctx, tm, json_get_str(in, "marker", ""));
        if (!arh2_exc_pending(ctx)) {
            json_t *st = json_object();
            json_set(st, "tool_msg", tm);
            *state_out = st;
        } else json_free(tm);
        return r;
    }
    if (strcmp(func, "_find_tool_result") == 0)
        return arh2_find_tool_result(ctx, arh2_get(in, "messages"), json_get_num(in, "start", 0),
                                     arh2_get(in, "tool_call"));
    if (strcmp(func, "_drop_invalid_roles") == 0)
        return arh2_drop_invalid_roles(ctx, arh2_get(in, "messages"));
    if (strcmp(func, "_drop_empty_tool_calls_arrays") == 0)
        return arh2_drop_empty_tool_calls_arrays(ctx, arh2_get(in, "messages"));
    if (strcmp(func, "_repair_invalid_tool_call_names") == 0) {
        json_t *msgs = json_copy(arh2_get(in, "messages"));
        if (!msgs) msgs = json_array();
        json_t *r = arh2_repair_invalid_tool_call_names(ctx, msgs);
        if (!arh2_exc_pending(ctx)) {
            json_t *st = json_object();
            json_set(st, "messages", msgs);
            *state_out = st;
        } else json_free(msgs);
        return r;
    }
    if (strcmp(func, "_drop_results_without_ids") == 0)
        return arh2_drop_results_without_ids(ctx, arh2_get(in, "messages"));
    if (strcmp(func, "_pair_tool_calls_positionally") == 0)
        return arh2_pair_tool_calls_positionally(ctx, arh2_get(in, "messages"));
    if (strcmp(func, "_dedupe_tool_call_ids") == 0)
        return arh2_dedupe_tool_call_ids(ctx, arh2_get(in, "messages"));
    if (strcmp(func, "_realign_tool_result_names") == 0)
        return arh2_realign_tool_result_names(ctx, arh2_get(in, "messages"));
    if (strcmp(func, "tool_results_this_turn") == 0)
        return arh2_tool_results_this_turn(ctx, arh2_get(in, "messages"));
    if (strcmp(func, "_api_error_debug_info") == 0)
        return arh2_api_error_debug_info(ctx, arh2_get(in, "error"));
    if (strcmp(func, "sync_credential_pool_entry_id") == 0) {
        json_t *agent = arh2_h_agent(in);
        json_t *r = arh2_sync_credential_pool_entry_id(ctx, agent);
        if (!arh2_exc_pending(ctx)) {
            static const char *F[] = {"_credential_pool_entry_id"};
            *state_out = arh2_h_echo(agent, F, 1);
        }
        json_free(agent);
        return r;
    }
    if (strcmp(func, "_failed_credential_identity") == 0)
        return arh2_failed_credential_identity(ctx, arh2_get(in, "agent"), arh2_get(in, "pool"));
    if (strcmp(func, "_restore_runtime_capabilities") == 0) {
        json_t *agent = arh2_h_agent(in);
        json_t *r = arh2_restore_runtime_capabilities(ctx, agent, arh2_get(in, "rt"));
        if (!arh2_exc_pending(ctx)) {
            static const char *F[] = {"runtime_capabilities"};
            *state_out = arh2_h_echo(agent, F, 1);
        }
        json_free(agent);
        return r;
    }
    if (strcmp(func, "_apply_primary_runtime_fields") == 0) {
        json_t *agent = arh2_h_agent(in);
        json_t *r = arh2_apply_primary_runtime_fields(ctx, agent, arh2_get(in, "rt"));
        if (!arh2_exc_pending(ctx)) {
            static const char *F[] = {"model", "provider", "requested_provider", "base_url",
                                      "api_mode", "api_key", "_reasoning_echo_flag",
                                      "request_overrides", "_client_kwargs", "_transport_cache"};
            *state_out = arh2_h_echo(agent, F, 10);
        }
        json_free(agent);
        return r;
    }
    if (strcmp(func, "_revert_credential_rotation") == 0) {
        json_t *agent = arh2_h_agent(in);
        json_t *r = arh2_revert_credential_rotation(ctx, agent);
        if (!arh2_exc_pending(ctx)) {
            static const char *F[] = {"_credential_pool_revert_id"};
            *state_out = arh2_h_echo(agent, F, 1);
        }
        json_free(agent);
        return r;
    }
    if (strcmp(func, "_requeue_pending_steer") == 0)
        return arh2_requeue_pending_steer(ctx, arh2_get(in, "agent"), json_get_str(in, "steer_text", ""));
    if (strcmp(func, "_pre_tool_block_message") == 0)
        return arh2_pre_tool_block_message(ctx, arh2_get(in, "agent"), in);
    if (strcmp(func, "_snapshot_switch_state") == 0)
        return arh2_snapshot_switch_state(ctx, arh2_get(in, "agent"));
    if (strcmp(func, "_restore_switch_snapshot") == 0) {
        json_t *agent = arh2_h_agent(in);
        json_t *snap = arh2_get(in, "snapshot");
        json_t *r = arh2_restore_switch_snapshot(ctx, agent, snap);
        if (!arh2_exc_pending(ctx)) {
            json_t *st = json_object();
            if (arh2_is_obj(snap)) {
                for (size_t i = 0; i < snap->c.count; i++) {
                    json_t *v = snap->c.items[i];
                    if (arh2_is_obj(v) && json_is_true(json_obj_get(v, "__missing__"))) continue;
                    json_set(st, snap->c.keys[i], arh2_copy_or_null(arh2_get(agent, snap->c.keys[i])));
                }
            }
            *state_out = st;
        }
        json_free(agent);
        return r;
    }
    if (strcmp(func, "_recover_auth_failure") == 0)
        return arh2_recover_auth_failure(ctx, arh2_get(in, "agent"), arh2_get(in, "pool"),
                                         arh2_get(in, "kwargs"));
    return NULL;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    static char line[1 << 16];
    while (fgets(line, sizeof(line), stdin)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '\r') continue;
        json_t *rec = json_parse(line, NULL);
        if (!rec) {
            fprintf(stderr, "harness: unparseable case line\n");
            return 1;
        }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        json_t *empty_in = NULL;
        if (!in) { empty_in = json_object(); in = empty_in; }
        arh2_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.env = json_obj_get(in, "env");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *state = NULL;
        json_t *ret = arh2_harness_run(&ctx, func, in, &state);
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_string(arh2_exc_class(&ctx)));
            json_free(ctx.exc);
            json_free(ret);
            json_free(state);
            state = NULL;
        } else {
            ret_out = ret ? ret : json_null();
        }
        json_t *out = json_object();
        json_set(out, "ret", ret_out);
        json_set(out, "trace", ctx.trace);
        json_set(out, "state", state ? state : json_object());
        json_t *emit = json_object();
        json_set(emit, "func", json_string(func));
        json_set(emit, "in", json_copy(in));
        json_set(emit, "out", out);
        char *s = json_serialize(emit);
        if (s) {
            fputs(s, stdout);
            fputc('\n', stdout);
            free(s);
        }
        json_free(emit);
        json_free(ctx.counters);
        json_free(empty_in);
        json_free(rec);
    }
    return 0;
}