/* t_port_prompt_caching_flow.c — harness for the agent/prompt_caching.py
 * FLOW port (src/agent/prompt_caching_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}} where "in" carries
 * the call arguments plus "io" (seam-name -> script: single value, {"$seq":[...]}
 * per-call queue, or {"__raise__":cls,"msg":m,...}; a MISSING key fail-closes).
 * Emits JSON LINES:
 *   {"func", "in", "out": {"ret", "trace", "state"}}
 * "ret" is the canonical return (dataclass -> field dict, NotImplemented ->
 * {"__notimplemented__":true}, uncaught exception -> {"__raise__":cls}); "trace"
 * is the ordered seam-call sequence [{"ev","args","ret"}]; "state" is the
 * post-call mutation window ({"agent":...} for the auto/refresh flows,
 * {"messages":...} for split/strip, {} otherwise) — echoed on RAISE too.
 * The oracle drives LIVE agent/prompt_caching.py under the SAME scripts and
 * diffs out. Dispatch runs on a deep COPY of "in" so the emitted "in" stays
 * pristine (the oracle re-drives from it).
 */
#include "hermes_json.h"
#include "agent/prompt_caching_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *pc_state_win(const char *key, json_t *node) {
    json_t *o = json_object();
    json_set(o, key, node ? json_copy(node) : json_null());
    return o;
}

static json_t *pc_harness_run(pc_ctx *ctx, const char *func, json_t *in, json_t **state_out) {
    *state_out = NULL;
    if (strcmp(func, "_text_part") == 0)
        return pc_text_part(json_obj_get(in, "text"), json_obj_get(in, "cache_marker"));
    if (strcmp(func, "_has_part_marker") == 0)
        return json_bool(pc_has_part_marker(json_obj_get(in, "content")));
    if (strcmp(func, "is_one_shot_request") == 0)
        return pc_is_one_shot(ctx, json_obj_get(in, "messages"), json_obj_get(in, "tools"));
    if (strcmp(func, "_scaffold_parts") == 0)
        return pc_scaffold_parts(ctx, json_obj_get(in, "msg"), json_obj_get(in, "cache_marker"));
    if (strcmp(func, "_split_unmarked_scaffolds") == 0) {
        json_t *m = json_obj_get(in, "messages");
        pc_split_unmarked_scaffolds(ctx, m);
        *state_out = pc_state_win("messages", m);
        return NULL;
    }
    if (strcmp(func, "_strip_prior_decoration") == 0) {
        json_t *m = json_obj_get(in, "messages");
        json_t *r = pc_strip_prior_decoration(ctx, m);
        *state_out = pc_state_win("messages", m);
        return r;
    }
    if (strcmp(func, "auto_cache_ttl_for_source") == 0)
        return pc_auto_cache_ttl_for_source(ctx, json_obj_get(in, "source"));
    if (strcmp(func, "auto_cache_ttl_for_agent") == 0) {
        json_t *ag = json_obj_get(in, "agent");
        json_t *r = pc_auto_cache_ttl_for_agent(ctx, ag);
        *state_out = pc_state_win("agent", ag);
        return r;
    }
    if (strcmp(func, "auto_start_cache_ttl") == 0) {
        json_t *ag = json_obj_get(in, "agent");
        json_t *r = pc_auto_start_cache_ttl(ctx, ag);
        *state_out = pc_state_win("agent", ag);
        return r;
    }
    if (strcmp(func, "refresh_auto_cache_ttl") == 0) {
        json_t *ag = json_obj_get(in, "agent");
        pc_refresh_auto_cache_ttl(ctx, ag);
        *state_out = pc_state_win("agent", ag);
        return NULL;
    }
    if (strcmp(func, "_route_names") == 0)
        return pc_route_names(ctx, json_obj_get(in, "provider"), json_obj_get(in, "base_url"));
    if (strcmp(func, "_catalog_route") == 0)
        return pc_catalog_route(ctx, json_obj_get(in, "provider"), json_obj_get(in, "base_url"));
    if (strcmp(func, "catalog_cache_markers") == 0)
        return pc_catalog_cache_markers(ctx, json_obj_get(in, "provider"),
                                        json_obj_get(in, "base_url"), json_obj_get(in, "model"),
                                        json_obj_get(in, "api_key"));
    if (strcmp(func, "learned_or_catalog_markers") == 0)
        return pc_learned_or_catalog_markers(ctx, json_obj_get(in, "provider"),
                                             json_obj_get(in, "base_url"), json_obj_get(in, "model"),
                                             json_obj_get(in, "api_key"));
    if (strcmp(func, "route_ttl_ceiling") == 0)
        return pc_route_ttl_ceiling(ctx, json_obj_get(in, "provider"), json_obj_get(in, "base_url"));
    if (strcmp(func, "_tool_run_end") == 0)
        return pc_tool_run_end(json_obj_get(in, "messages"), json_get_num(in, "start", 0));
    if (strcmp(func, "PromptCachePlan.__init__") == 0)
        return pc_plan_init(ctx, json_obj_get(in, "args"), json_obj_get(in, "kwargs"));
    if (strcmp(func, "PromptCachePlan.__eq__") == 0)
        return pc_plan_eq(ctx, json_obj_get(in, "self"), json_obj_get(in, "other"));
    if (strcmp(func, "PromptCachePlan.__setattr__") == 0)
        return pc_plan_setattr(ctx, json_obj_get(in, "name"));
    if (strcmp(func, "PromptCachePlan.__delattr__") == 0)
        return pc_plan_delattr(ctx, json_obj_get(in, "name"));
    if (strcmp(func, "PromptCachePlan.__hash__") == 0)
        return pc_plan_hash(ctx, json_obj_get(in, "fields"));
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
        pc_ctx ctx;
        ctx.io = json_obj_get(work, "io");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *state = NULL;
        json_t *ret = pc_harness_run(&ctx, func, work, &state);
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
