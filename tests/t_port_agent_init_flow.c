/* t_port_agent_init_flow.c — harness for the agent/agent_init.py UNCLAIMED-remainder
 * FLOW port (src/agent/agent_init_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...args..., "io": {seam:
 * script}, "env": {...}, "methods": [self-method names]}}. Emits JSON LINES:
 * {"func","in","out":{"ret","trace","state"}}. "ret" is the canonical return
 * (tuples/sets -> arrays, None -> null, uncaught exception -> {"__raise__": cls});
 * "trace" is the ordered seam-call sequence [{"ev","args","ret"}]; "state" is the
 * post-call out-state (selected agent fields; {} otherwise, and {} on any raise).
 * The oracle drives LIVE agent/agent_init.py under the SAME scripts and diffs out.
 *
 * Self-contained: #includes the area-qualified port .c (libjson-only).
 */
#include "hermes_json.h"
#include "agent/agent_init_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* echo selected agent fields as out-state (getattr default None). */
static json_t *ai_h_echo(json_t *agent, const char *const *fields, size_t n) {
    json_t *st = json_object();
    for (size_t i = 0; i < n; i++)
        json_set(st, fields[i], ai_copy_or_null(ai_get(agent, fields[i])));
    return st;
}
static json_t *ai_h_agent(json_t *in) {
    json_t *a = ai_get(in, "agent");
    return a ? json_copy(a) : json_object();
}

static const char *AI_TURN_FIELDS[] = {
    "_iteration_budget_warning_injected", "_budget_exhausted_injected", "_budget_grace_call",
    "_run_budget_started_at", "_run_budget_wrapup_injected", "_last_activity_ts",
    "_last_activity_desc", "_last_activity_provenance", "_session_activity_last_persist_mono",
    "_current_tool", "_api_call_count", "_skip_mcp_refresh", "_tool_snapshot_generation",
    "_rate_limit_state", "_credits_state", "_credits_session_start_micros", "_or_cache_hits",
    "run_budget_seconds", "_credits_latch",
};
static const char *AI_USAGE_FIELDS[] = {
    "_user_turn_count", "_is_user_initiated_turn", "_usage_anchor", "_turn_base_usage_anchor",
    "_request_pressure_anchored", "session_prompt_tokens", "session_completion_tokens",
    "session_total_tokens", "session_api_calls", "session_input_tokens", "session_output_tokens",
    "session_cache_read_tokens", "session_cache_write_tokens", "session_reasoning_tokens",
    "session_estimated_cost_usd", "session_cost_status", "session_cost_source",
    "_api_latency_history", "_api_output_history", "_subdirectory_hints",
};
static const char *AI_PC_FIELDS[] = {
    "_use_prompt_caching", "_use_native_cache_layout", "_cache_disabled", "_cache_ttl", "_cache_ttl_auto",
};

static json_t *ai_harness_run(ai_ctx *ctx, const char *func, json_t *in, json_t **state_out) {
    *state_out = NULL;

    if (strcmp(func, "_parse_config_int") == 0) {
        json_t *def = ai_get(in, "default");
        if (!def) def = json_number(0);
        return ai_parse_config_int(ctx, ai_get(in, "raw"), def);
    }
    if (strcmp(func, "_cfg_flag") == 0) {
        json_t *def = ai_get(in, "default");
        if (!def) def = json_bool(false);
        return ai_cfg_flag(ctx, ai_get(in, "cfg"), json_get_str(in, "key", ""), def);
    }
    if (strcmp(func, "_cfg_dict") == 0)
        return ai_cfg_dict(ctx, ai_get(in, "cfg"), json_get_str(in, "key", ""));
    if (strcmp(func, "_positive_int") == 0)
        return ai_positive_int(ctx, ai_get(in, "raw"), ai_get(in, "reject"));
    if (strcmp(func, "_set_defaults") == 0) {
        json_t *agent = ai_h_agent(in);
        json_t *table = ai_get(in, "table");
        json_t *r = ai_set_defaults(ctx, agent, table);
        if (!ai_exc_pending(ctx)) {
            json_t *st = json_object();
            if (ai_is_obj(table))
                for (size_t i = 0; i < table->c.count; i++)
                    json_set(st, table->c.keys[i], ai_copy_or_null(ai_get(agent, table->c.keys[i])));
            *state_out = st;
        }
        json_free(agent);
        return r;
    }
    if (strcmp(func, "_init_turn_state") == 0) {
        json_t *agent = ai_h_agent(in);
        json_t *r = ai_init_turn_state(ctx, agent, ai_get(in, "run_budget_seconds"));
        if (!ai_exc_pending(ctx))
            *state_out = ai_h_echo(agent, AI_TURN_FIELDS, sizeof(AI_TURN_FIELDS)/sizeof(AI_TURN_FIELDS[0]));
        json_free(agent);
        return r;
    }
    if (strcmp(func, "_init_usage_state") == 0) {
        json_t *agent = ai_h_agent(in);
        json_t *r = ai_init_usage_state(ctx, agent);
        if (!ai_exc_pending(ctx))
            *state_out = ai_h_echo(agent, AI_USAGE_FIELDS, sizeof(AI_USAGE_FIELDS)/sizeof(AI_USAGE_FIELDS[0]));
        json_free(agent);
        return r;
    }
    if (strcmp(func, "_setup_logging") == 0) {
        json_t *agent = ai_h_agent(in);
        json_t *r = ai_setup_logging(ctx, agent);
        json_free(agent);
        return r;
    }
    if (strcmp(func, "_init_prompt_cache_config") == 0) {
        json_t *agent = ai_h_agent(in);
        json_t *r = ai_init_prompt_cache_config(ctx, agent);
        if (!ai_exc_pending(ctx))
            *state_out = ai_h_echo(agent, AI_PC_FIELDS, sizeof(AI_PC_FIELDS)/sizeof(AI_PC_FIELDS[0]));
        json_free(agent);
        return r;
    }
    if (strcmp(func, "_resolve_api_mode") == 0) {
        json_t *agent = ai_h_agent(in);
        json_t *r = ai_resolve_api_mode(ctx, agent, ai_get(in, "api_mode"),
                                        ai_get(in, "provider_name"), ai_get(in, "base_url"));
        if (!ai_exc_pending(ctx)) {
            static const char *F[] = {"api_mode", "provider"};
            *state_out = ai_h_echo(agent, F, 2);
        }
        json_free(agent);
        return r;
    }
    if (strcmp(func, "_build_client") == 0) {
        json_t *agent = ai_h_agent(in);
        json_t *r = ai_build_client(ctx, agent, ai_get(in, "api_key"),
                                    ai_get(in, "base_url"), ai_get(in, "fallback_model"));
        if (!ai_exc_pending(ctx)) {
            static const char *F[] = {"_anthropic_client", "_is_anthropic_oauth"};
            *state_out = ai_h_echo(agent, F, 2);
        }
        json_free(agent);
        return r;
    }
    if (strcmp(func, "_compression_threshold") == 0) {
        json_t *agent = ai_h_agent(in);
        json_t *r = ai_compression_threshold(ctx, agent, ai_get(in, "cfg"));
        if (!ai_exc_pending(ctx)) {
            static const char *F[] = {"_compression_threshold_autoraised"};
            *state_out = ai_h_echo(agent, F, 1);
        }
        json_free(agent);
        return r;
    }
    if (strcmp(func, "_compression_codex_settings") == 0)
        return ai_compression_codex_settings(ctx, ai_get(in, "cfg"));
    if (strcmp(func, "_parse_compression_config") == 0) {
        json_t *agent = ai_h_agent(in);
        json_t *r = ai_parse_compression_config(ctx, agent, ai_get(in, "agent_cfg"));
        if (!ai_exc_pending(ctx)) {
            static const char *F[] = {"_compression_threshold_autoraised"};
            *state_out = ai_h_echo(agent, F, 1);
        }
        json_free(agent);
        return r;
    }
    if (strcmp(func, "_custom_provider_configured_base_url") == 0)
        return ai_custom_provider_configured_base_url(ctx, ai_get(in, "configured_provider"),
                                                      ai_get(in, "agent_cfg"), ai_get(in, "custom_providers"));
    if (strcmp(func, "_configured_default_base_url") == 0)
        return ai_configured_default_base_url(ctx, ai_get(in, "agent_cfg"),
                                              ai_get(in, "model_cfg"), ai_get(in, "custom_providers"));
    if (strcmp(func, "_active_route_url") == 0) {
        json_t *agent = ai_h_agent(in);
        json_t *r = ai_active_route_url(ctx, agent, ai_get(in, "base_url"));
        json_free(agent);
        return r;
    }
    if (strcmp(func, "_scope_context_length_to_default_runtime") == 0) {
        json_t *agent = ai_h_agent(in);
        json_t *r = ai_scope_context_length_to_default_runtime(ctx, agent, ai_get(in, "agent_cfg"),
                        ai_get(in, "model_cfg"), ai_get(in, "custom_providers"),
                        ai_get(in, "config_context_length"), ai_get(in, "base_url"));
        json_free(agent);
        return r;
    }
    if (strcmp(func, "set_config_context_length") == 0) {
        json_t *agent = ai_h_agent(in);
        json_t *r = ai_set_config_context_length(ctx, agent, ai_get(in, "value"));
        if (!ai_exc_pending(ctx)) {
            json_t *st = json_object();
            json_set(st, "_config_context_length", ai_copy_or_null(ai_get(agent, "_config_context_length")));
            json_set(st, "context_compressor", ai_copy_or_null(ai_get(agent, "context_compressor")));
            *state_out = st;
        }
        json_free(agent);
        return r;
    }
    if (strcmp(func, "config_context_length_for_runtime") == 0) {
        json_t *agent = ai_h_agent(in);
        json_t *r = ai_config_context_length_for_runtime(ctx, agent, ai_get(in, "config"));
        json_free(agent);
        return r;
    }
    if (strcmp(func, "_select_context_engine") == 0)
        return ai_select_context_engine(ctx, ai_get(in, "agent_cfg"));
    if (strcmp(func, "_compressor_max_tokens") == 0) {
        json_t *agent = ai_h_agent(in);
        json_t *r = ai_compressor_max_tokens(ctx, agent);
        json_free(agent);
        return r;
    }
    if (strcmp(func, "_enforce_minimum_context") == 0) {
        json_t *agent = ai_h_agent(in);
        json_t *r = ai_enforce_minimum_context(ctx, agent);
        json_free(agent);
        return r;
    }
    if (strcmp(func, "_warn_nonagentic_hermes_model") == 0) {
        json_t *agent = ai_h_agent(in);
        json_t *r = ai_warn_nonagentic_hermes_model(ctx, agent);
        json_free(agent);
        return r;
    }
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
        ai_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.env = json_obj_get(in, "env");
        ctx.methods = json_obj_get(in, "methods");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *state = NULL;
        json_t *ret = ai_harness_run(&ctx, func, in, &state);
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_string(ai_exc_class(&ctx)));
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