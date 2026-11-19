/* t_port_prompt_cache_clock_flow.c — harness for the agent/prompt_cache_clock.py
 * FLOW port (src/agent/prompt_cache_clock_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}} where "in" carries
 * the call arguments plus "env" (scripted constants/flags) and "io" (seam-name
 * -> script: single value, {"$seq": [...]} per-call queue, or {"__raise__": cls,
 * "msg": m, ...}). Emits JSON LINES:
 *   {"func", "in", "out": {"ret", "trace", "state"}}
 * "ret" is the canonical return (tuples/sets -> arrays, dataclasses -> asdict
 * dicts, uncaught exception -> {"__raise__": cls}); "trace" is the ordered
 * seam-call sequence [{"ev","args","ret"}]; "state" is the post-call out set for
 * _block_marker_ttls ({} otherwise). The oracle drives LIVE
 * agent/prompt_cache_clock.py under the SAME scripts and diffs out.
 */
#include "hermes_json.h"
#include "agent/prompt_cache_clock_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *pccf_harness_run(pccf_ctx *ctx, const char *func, json_t *in, json_t **state_out) {
    *state_out = NULL;
    if (strcmp(func, "_block_marker_ttls") == 0) {
        json_t *set = json_object();
        json_t *init = json_obj_get(in, "out");
        if (pccf_is_arr(init))
            for (size_t i = 0; i < init->c.count; i++)
                if (pccf_is_str(init->c.items[i])) pccf_set_add(set, init->c.items[i]->str_val);
        pccf_block_marker_ttls(ctx, json_obj_get(in, "block"), set);
        *state_out = pccf_set_sorted(set);
        json_free(set);
        return NULL;
    }
    if (strcmp(func, "_marker_ttl") == 0)
        return pccf_marker_ttl(ctx, json_obj_get(in, "api_kwargs"));
    if (strcmp(func, "_wire_field") == 0)
        return pccf_wire_field(json_obj_get(in, "api_kwargs"), json_get_str(in, "key", ""));
    if (strcmp(func, "attempt_facts") == 0)
        return pccf_attempt_facts(ctx, json_obj_get(in, "api_kwargs"));
    if (strcmp(func, "classify_route") == 0)
        return pccf_classify_route(json_get_bool(in, "has_markers", false));
    if (strcmp(func, "_written_tiers") == 0)
        return pccf_written_tiers(json_obj_get(in, "tiers"));
    if (strcmp(func, "_rule_anthropic_spec") == 0)
        return pccf_rule_anthropic_spec(ctx, json_obj_get(in, "ctx"));
    if (strcmp(func, "_rule_openai_spec") == 0)
        return pccf_rule_openai_spec(json_obj_get(in, "ctx"));
    if (strcmp(func, "_fold_coverage") == 0)
        return pccf_fold_coverage(ctx, json_obj_get(in, "previous"), json_obj_get(in, "verdict"),
                                  json_get_str(in, "route_class", ""), json_get_num(in, "at", 0),
                                  (long long)json_get_num(in, "read", 0), (long long)json_get_num(in, "write", 0));
    if (strcmp(func, "_ends") == 0)
        return pccf_ends(json_obj_get(in, "coverage"));
    if (strcmp(func, "_estimated_end") == 0)
        return pccf_estimated_end(ctx, json_obj_get(in, "coverage"), json_get_str(in, "base_url", ""),
                                  json_obj_get(in, "retention"), json_get_num(in, "at", 0),
                                  json_obj_get(in, "previous"), json_get_bool(in, "carried", false),
                                  json_get_num(in, "idle_from", 0));
    if (strcmp(func, "_confidence") == 0)
        return pccf_confidence(ctx, json_obj_get(in, "verdict"), json_obj_get(in, "previous"),
                               json_get_bool(in, "carried", false), (long long)json_get_num(in, "write", 0));
    if (strcmp(func, "_invalid_reason") == 0)
        return pccf_invalid_reason(json_obj_get(in, "event"), json_get_str(in, "session_id", ""),
                                   json_obj_get(in, "generation"), json_obj_get(in, "route"));
    if (strcmp(func, "current_generation") == 0)
        return pccf_current_generation(ctx, json_get_bool(ctx->env, "has_reader", false),
                                       json_obj_get(in, "session_id"));
    if (strcmp(func, "event_from_json") == 0)
        return pccf_event_from_json(ctx, json_obj_get(in, "raw"));
    if (strcmp(func, "cache_certainly_warm") == 0)
        return pccf_cache_certainly_warm(ctx, json_obj_get(in, "window"), json_obj_get(in, "now"));
    if (strcmp(func, "cache_certainly_cold") == 0)
        return pccf_cache_certainly_cold(ctx, json_obj_get(in, "window"), json_obj_get(in, "now"));
    if (strcmp(func, "_duration") == 0)
        return pccf_duration(ctx, json_obj_get(in, "seconds"));
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
        pccf_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.env = json_obj_get(in, "env");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *state = NULL;
        json_t *ret = pccf_harness_run(&ctx, func, in, &state);
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_copy(json_obj_get(ctx.exc, "__raise__")));
            json_free(ctx.exc);
            json_free(ret);
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