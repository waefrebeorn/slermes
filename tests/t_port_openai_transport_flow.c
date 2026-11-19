/* t_port_openai_transport_flow.c — harness for the OpenAI Chat Completions
 * transport UNCLAIMED-remainder FLOW port
 * (src/agent/transports/openai_transport_flow.c; live module
 * agent/transports/chat_completions.py + agent/transports/types.py leaves).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...args..., "io": {seam:
 * script}}}. Emits JSON LINES: {"func","in","out":{"ret","trace","state"}}.
 * "ret" is the canonical return (tuples/sets -> arrays, None -> null, uncaught
 * exception -> {"__raise__": cls}); "trace" is the ordered seam-call sequence
 * [{"ev","args","ret"}]; "state" is the post-call mutation window
 * ({"api_kwargs":...} for the mutating flows, {} otherwise) — echoed on RAISE
 * too (partial history). The oracle drives LIVE agent/transports/chat_completions.py
 * under the SAME scripts and diffs out. Dispatch runs on a deep COPY of "in" so
 * the emitted "in" stays pristine.
 *
 * Self-contained: #includes the area-qualified port .c (libjson-only).
 */
#include "hermes_json.h"
#include "agent/transports/openai_transport_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *oai_state_win(const char *key, json_t *node) {
    json_t *o = json_object();
    json_set(o, key, node ? node : json_null());
    return o;
}

static json_t *oai_harness_run(oai_ctx *ctx, const char *func, json_t *in, json_t **state_out) {
    *state_out = NULL;
    if (strcmp(func, "_rename_tool_search_bridge_for_xai") == 0)
        return oai_rename_tool_search_bridge_for_xai(ctx, json_obj_get(in, "tools"));
    if (strcmp(func, "_profile_replays_tool_call_signatures") == 0)
        return oai_profile_replays_tool_call_signatures(ctx, json_obj_get(in, "provider"));
    if (strcmp(func, "_wire_reasoning_carriers") == 0)
        return oai_wire_reasoning_carriers(ctx, json_obj_get(in, "provider"), json_obj_get(in, "base_url"));
    if (strcmp(func, "_has_replayable_thought_signature") == 0)
        return json_bool(oai_has_replayable_thought_signature(json_obj_get(in, "extra_content")));
    if (strcmp(func, "_attr_or_model_extra") == 0)
        return oai_attr_or_model_extra(json_obj_get(in, "obj"), json_get_str(in, "name", ""));
    if (strcmp(func, "_dump_extra_content") == 0)
        return oai_dump_extra_content(ctx, json_obj_get(in, "extra"),
                                      oai_truthy(json_obj_get(in, "has_model_dump")));
    if (strcmp(func, "_pareto_score") == 0)
        return oai_pareto_score(json_obj_get(in, "raw"));
    if (strcmp(func, "_swap_developer_role") == 0)
        return oai_swap_developer_role(json_obj_get(in, "sanitized"), json_get_str(in, "model_lower", ""));
    if (strcmp(func, "_apply_max_tokens") == 0) {
        json_t *kw = json_obj_get(in, "api_kwargs");
        oai_apply_max_tokens(ctx, kw, json_obj_get(in, "model"), json_obj_get(in, "reasoning_config"),
                             json_obj_get(in, "params"), json_obj_get(in, "profile_max"));
        *state_out = oai_state_win("api_kwargs", json_copy(kw));
        return NULL;
    }
    if (strcmp(func, "_base_kwargs") == 0)
        return oai_base_kwargs(ctx, json_obj_get(in, "model"), json_obj_get(in, "sanitized"),
                               json_obj_get(in, "tools"), json_obj_get(in, "params"),
                               json_obj_get(in, "profile"));
    if (strcmp(func, "_finish_kwargs") == 0) {
        json_t *kw = json_obj_get(in, "api_kwargs");
        json_t *r = oai_finish_kwargs(ctx, kw, json_obj_get(in, "sanitized"),
                                      json_obj_get(in, "params"),
                                      json_obj_get(in, "supports_prompt_cache_key"));
        *state_out = oai_state_win("api_kwargs", json_copy(kw));
        return r;
    }
    if (strcmp(func, "_route_requires_mistral_tool_ids") == 0)
        return oai_route_requires_mistral_tool_ids(ctx, json_obj_get(in, "provider"),
                                                   json_obj_get(in, "model"), json_obj_get(in, "base_url"));
    if (strcmp(func, "_mistral_tool_id") == 0)
        return oai_mistral_tool_id(ctx, json_obj_get(in, "key"));
    if (strcmp(func, "_sanitize_message") == 0)
        return oai_sanitize_message(ctx, json_obj_get(in, "msg"),
                                    oai_truthy(json_obj_get(in, "strip_extra_content")),
                                    json_obj_get(in, "strip_reasoning"),
                                    json_obj_get(in, "native_reasoning_details_type"),
                                    oai_truthy(json_obj_get(in, "mistral_tool_ids")));
    if (strcmp(func, "_normalize_tool_call") == 0)
        return oai_normalize_tool_call(ctx, json_obj_get(in, "self"), json_obj_get(in, "tc"));
    if (strcmp(func, "ToolCall._pd") == 0)
        return oai_tool_call_pd(ctx, json_obj_get(in, "provider_data"), json_get_str(in, "key", ""));
    if (strcmp(func, "NormalizedResponse._pd") == 0)
        return oai_normalized_response_pd(ctx, json_obj_get(in, "provider_data"), json_get_str(in, "key", ""));
    if (strcmp(func, "Usage.from_openai") == 0)
        return oai_usage_from_openai(json_obj_get(in, "u"));
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
        json_t *work = json_copy(in);   /* dispatch mutates the copy, not "in" */
        oai_ctx ctx;
        ctx.io = json_obj_get(work, "io");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *state = NULL;
        json_t *ret = oai_harness_run(&ctx, func, work, &state);
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_copy(json_obj_get(ctx.exc, "__raise__")));
            json_free(ctx.exc);
            if (ret) json_free(ret);
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
        json_free(work);
        json_free(empty_in);
        json_free(rec);
    }
    return 0;
}