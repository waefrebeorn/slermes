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
static void arh2_h_strip_pool_methods(json_t *pool) {
    /* Oracle mk_pool_full() turns these POOL_METHODS keys into callable SeamFns;
     * norm() then drops them from every projected state/trace object.  Mirror
     * that so loaded pools don't leak method-script values into C state. */
    static const char *M[] = {"next_available_at", "has_available", "select", "lift_reopened_cooldowns"};
    if (!pool || pool->type != JSON_OBJECT) return;
    for (size_t i = 0; i < sizeof(M) / sizeof(M[0]); i++) json_obj_del(pool, M[i]);
}
static json_t *arh2_h_echo(json_t *agent, const char *const *fields, size_t n) {
    json_t *st = json_object();
    for (size_t i = 0; i < n; i++) {
        json_t *v = arh2_copy_or_null(arh2_get(agent, fields[i]));
        if (strcmp(fields[i], "_credential_pool") == 0) arh2_h_strip_pool_methods(v);
        json_set(st, fields[i], v);
    }
    return st;
}
static json_t *arh2_h_agent(json_t *in) {
    /* Mirror the oracle's mk_agent_full() default fields: a Python
     * SimpleNamespace exposes these as None even when the fixture omits them;
     * keep the C attribute dict identical so seam arg projections match. */
    static const char *DEF[] = {
        "requested_provider", "api_key", "client", "_anthropic_client", "_anthropic_api_key",
        "_anthropic_base_url", "_is_anthropic_oauth", "_client_kwargs", "_credential_pool",
        "_credential_pool_entry_id", "request_overrides", "_config_context_length",
        "_reasoning_echo_flag", "runtime_capabilities", "_fallback_chain", "_fallback_model",
        "_restore_wait_logged", "_fallback_activated",
        "_provider_fallback_active", "_provider_fallback_route", "_fallback_index",
        "_credential_pool_revert_id", "context_compressor", "model", "provider", "base_url",
        "api_mode"
    };
    json_t *src = arh2_get(in, "agent");
    json_t *a = src ? json_copy(src) : json_object();
    for (size_t i = 0; i < sizeof(DEF) / sizeof(DEF[0]); i++) {
        if (arh2_has(a, DEF[i])) continue;
        /* Oracle mk_agent_full() always sets context_compressor to a namespace
         * whose only member is a callable seam; norm() strips the callable, so
         * the live double projects an EMPTY OBJECT, not null.  Mirror that. */
        if (strcmp(DEF[i], "context_compressor") == 0)
            json_set(a, DEF[i], json_object());
        else
            json_set(a, DEF[i], json_null());
    }
    return a;
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
    /* ── UNCLAIMED remainder (27 fns) ───────────────────────────────────── */
    if (strcmp(func, "_cursor_skip_prefix") == 0)
        return arh2_cursor_skip_prefix(ctx, arh2_get(in, "messages"), arh2_get(in, "cursor"));
    if (strcmp(func, "_recover_rate_limit") == 0)
        return arh2_recover_rate_limit(ctx, arh2_get(in, "pool"), arh2_bdef(in, "has_retried_429", false),
                arh2_get(in, "error_context"), arh2_get(in, "api_key_hint"), arh2_get(in, "credential_id"),
                arh2_bdef(in, "rotate_and_swap", false));
    if (strcmp(func, "_build_anthropic_client_from_runtime") == 0) {
        json_t *agent = arh2_h_agent(in);
        json_t *r = arh2_build_anthropic_client_from_runtime(ctx, agent, arh2_get(in, "rt"));
        if (!arh2_exc_pending(ctx)) {
            static const char *F[] = {"_anthropic_api_key", "_anthropic_base_url", "_anthropic_client",
                                      "_is_anthropic_oauth", "client"};
            *state_out = arh2_h_echo(agent, F, 5);
        }
        json_free(agent); return r;
    }
    if (strcmp(func, "_rebuild_primary_client") == 0) {
        json_t *agent = arh2_h_agent(in);
        json_t *r = arh2_rebuild_primary_client(ctx, agent, arh2_get(in, "rt"), json_get_str(in, "reason", ""));
        if (!arh2_exc_pending(ctx)) {
            static const char *F[] = {"client", "_anthropic_client", "_anthropic_api_key",
                                      "_anthropic_base_url", "_is_anthropic_oauth"};
            *state_out = arh2_h_echo(agent, F, 5);
        }
        json_free(agent); return r;
    }
    if (strcmp(func, "_primary_reset_gate_blocks") == 0) {
        json_t *agent = arh2_h_agent(in);
        json_t *r = arh2_primary_reset_gate_blocks(ctx, agent, arh2_get(in, "rt"),
                json_get_str(in, "primary_provider", ""), json_get_str(in, "primary_runtime_base_url", ""));
        if (!arh2_exc_pending(ctx)) {
            static const char *F[] = {"_restore_wait_logged"};
            *state_out = arh2_h_echo(agent, F, 1);
        }
        json_free(agent); return r;
    }
    if (strcmp(func, "_rebind_primary_credential_pool") == 0) {
        json_t *agent = arh2_h_agent(in);
        json_t *r = arh2_rebind_primary_credential_pool(ctx, agent,
                json_get_str(in, "primary_provider", ""), json_get_str(in, "primary_model", ""),
                arh2_get(in, "prefetched_pool"), arh2_bdef(in, "prefetched", false));
        if (!arh2_exc_pending(ctx)) {
            static const char *F[] = {"_credential_pool", "_credential_pool_entry_id"};
            *state_out = arh2_h_echo(agent, F, 2);
        }
        json_free(agent); return r;
    }
    if (strcmp(func, "_primary_quota_reopened_early") == 0) {
        json_t *agent = arh2_h_agent(in);
        json_t *r = arh2_primary_quota_reopened_early(ctx, agent,
                json_get_str(in, "primary_provider", ""), json_get_str(in, "primary_model", ""));
        if (!arh2_exc_pending(ctx)) {
            static const char *F[] = {"_codex_reopen_checked_at"};
            *state_out = arh2_h_echo(agent, F, 1);
        }
        json_free(agent); return r;
    }
    if (strcmp(func, "_moa_aggregator_cache_policy") == 0)
        return arh2_moa_aggregator_cache_policy(ctx, arh2_get(in, "agent"), json_get_str(in, "eff_model", ""));
    if (strcmp(func, "_route_may_be_custom") == 0)
        return arh2_route_may_be_custom(ctx, arh2_get(in, "agent"), json_get_str(in, "eff_provider", ""),
                json_get_str(in, "provider_lower", ""), json_get_str(in, "eff_base_url", ""));
    if (strcmp(func, "_provider_supplied_client") == 0)
        return arh2_provider_supplied_client(ctx, arh2_get(in, "agent"), arh2_get(in, "client_kwargs"));
    if (strcmp(func, "_ensure_route_client_headers") == 0) {
        json_t *ck = json_copy(arh2_get(in, "client_kwargs"));
        if (!ck) ck = json_object();
        json_t *r = arh2_ensure_route_client_headers(ctx, ck, json_get_str(in, "provider", ""));
        if (!arh2_exc_pending(ctx)) {
            json_t *st = json_object(); json_set(st, "client_kwargs", ck);
            *state_out = st;
        } else json_free(ck);
        return r;
    }
    if (strcmp(func, "_gemini_native_client") == 0)
        return arh2_gemini_native_client(ctx, arh2_get(in, "agent"), arh2_get(in, "client_kwargs"),
                arh2_get(in, "httpx_verify"), json_get_str(in, "reason", ""), arh2_bdef(in, "shared", false));
    if (strcmp(func, "_apply_switched_provider_request_overrides") == 0) {
        json_t *agent = arh2_h_agent(in);
        json_t *r = arh2_apply_switched_provider_request_overrides(ctx, agent, json_get_str(in, "new_provider", ""));
        if (!arh2_exc_pending(ctx)) {
            static const char *F[] = {"request_overrides"};
            *state_out = arh2_h_echo(agent, F, 1);
        }
        json_free(agent); return r;
    }
    if (strcmp(func, "_resolve_switch_destination") == 0)
        return arh2_resolve_switch_destination(ctx, arh2_get(in, "agent"),
                json_get_str(in, "new_model", ""), json_get_str(in, "new_provider", ""),
                json_get_str(in, "base_url", ""), json_get_str(in, "api_mode", ""),
                arh2_get(in, "capabilities"), json_get_str(in, "old_norm", ""), json_get_str(in, "new_norm", ""));
    if (strcmp(func, "_build_switched_client") == 0) {
        json_t *agent = arh2_h_agent(in);
        json_t *r = arh2_build_switched_client(ctx, agent, json_get_str(in, "new_provider", ""),
                arh2_get(in, "api_key"), json_get_str(in, "base_url", ""),
                json_get_str(in, "api_mode", ""), json_get_str(in, "new_norm", ""));
        if (!arh2_exc_pending(ctx)) {
            static const char *F[] = {"api_key", "_anthropic_api_key", "_anthropic_base_url", "_anthropic_client",
                                      "_is_anthropic_oauth", "client", "_client_kwargs"};
            *state_out = arh2_h_echo(agent, F, 7);
        }
        json_free(agent); return r;
    }
    if (strcmp(func, "_swap_switch_runtime") == 0) {
        json_t *agent = arh2_h_agent(in);
        json_t *r = arh2_swap_switch_runtime(ctx, agent, json_get_str(in, "new_model", ""),
                json_get_str(in, "new_provider", ""), arh2_get(in, "api_key"),
                json_get_str(in, "base_url", ""), json_get_str(in, "api_mode", ""),
                json_get_str(in, "old_provider", ""), json_get_str(in, "old_norm", ""), json_get_str(in, "new_norm", ""));
        if (!arh2_exc_pending(ctx)) {
            static const char *F[] = {"_config_context_length", "model", "provider", "requested_provider",
                                      "_reasoning_echo_flag", "base_url", "api_mode", "_transport_cache",
                                      "api_key", "_credential_pool", "_credential_pool_entry_id",
                                      "_anthropic_client", "client", "_client_kwargs"};
            *state_out = arh2_h_echo(agent, F, 14);
        }
        json_free(agent); return r;
    }
    if (strcmp(func, "_resolve_switch_context_length") == 0) {
        json_t *agent = arh2_h_agent(in);
        json_t *r = arh2_resolve_switch_context_length(ctx, agent, arh2_get(in, "snapshot"));
        if (!arh2_exc_pending(ctx)) {
            static const char *F[] = {"_config_context_length"};
            *state_out = arh2_h_echo(agent, F, 1);
        }
        json_free(agent); return r;
    }
    if (strcmp(func, "_update_switch_compressor") == 0) {
        json_t *agent = arh2_h_agent(in);
        json_t *r = arh2_update_switch_compressor(ctx, agent, arh2_get(in, "custom_providers"),
                arh2_get(in, "effective_context_length"), arh2_get(in, "snapshot"));
        json_free(agent); return r;
    }
    if (strcmp(func, "_finish_switch") == 0) {
        json_t *agent = arh2_h_agent(in);
        json_t *r = arh2_finish_switch(ctx, agent, json_get_str(in, "new_provider", ""),
                json_get_str(in, "old_norm", ""), json_get_str(in, "new_norm", ""));
        if (!arh2_exc_pending(ctx)) {
            static const char *F[] = {"_fallback_activated", "_provider_fallback_active", "_provider_fallback_route",
                                      "_fallback_index", "_credential_pool_revert_id", "_fallback_chain",
                                      "_fallback_model", "request_overrides"};
            *state_out = arh2_h_echo(agent, F, 8);
        }
        json_free(agent); return r;
    }
    if (strcmp(func, "_persist_switch_billing_route") == 0) {
        json_t *agent = arh2_h_agent(in);
        json_t *r = arh2_persist_switch_billing_route(ctx, agent);
        json_free(agent); return r;
    }
    if (strcmp(func, "_log_empty_non_final_heal") == 0)
        return arh2_log_empty_non_final_heal(ctx, json_get_num(in, "healed", 0));
    if (strcmp(func, "looks_like_degenerate_final") == 0)
        return arh2_looks_like_degenerate_final(ctx, json_get_str(in, "text", ""), arh2_get(in, "user_message"));
    if (strcmp(func, "promoted_reasoning_announces_action") == 0)
        return arh2_promoted_reasoning_announces_action(ctx, json_get_str(in, "text", ""));
    if (strcmp(func, "_socket_from_stream") == 0)
        return arh2_socket_from_stream(ctx, arh2_get(in, "stream"));
    if (strcmp(func, "_socket_from_candidate") == 0)
        return arh2_socket_from_candidate(ctx, arh2_get(in, "candidate"));
    if (strcmp(func, "_socket_from_response") == 0)
        return arh2_socket_from_response(ctx, arh2_get(in, "response"));
    if (strcmp(func, "_shutdown_socket") == 0)
        return arh2_shutdown_socket(ctx, arh2_get(in, "sock"));
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