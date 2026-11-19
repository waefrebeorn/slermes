/* t_port_codex_transport_flow.c — harness for the agent/transports/codex.py
 * UNCLAIMED-remainder FLOW port (src/agent/transports/codex_transport_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...args..., "io": {seam:
 * script}}}. Emits JSON LINES: {"func","in","out":{"ret","trace","state"}}.
 * "ret" is the canonical return (tuples/sets -> arrays, None -> null, uncaught
 * exception -> {"__raise__": cls}); "trace" is the ordered seam-call sequence
 * [{"ev","args","ret"}]; "state" is the post-call mutation window
 * ({"container":...} / {"kwargs":...} / {"self":...} / {"warned":[...]} for the
 * mutating flows, {} otherwise) — echoed on RAISE too (partial history). The
 * oracle drives LIVE agent/transports/codex.py under the SAME scripts and diffs
 * out. Dispatch runs on a deep COPY of "in" so the emitted "in" stays pristine.
 *
 * Self-contained: #includes the area-qualified port .c (libjson-only).
 */
#include "hermes_json.h"
#include "agent/transports/codex_transport_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* sorted array of a string-set object's keys (sets project as sorted arrays) */
static int cxt_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
static json_t *cxt_set_sorted(const json_t *set) {
    json_t *arr = json_array();
    if (!cxt_is_obj(set) || set->c.count == 0) return arr;
    const char **keys = (const char **)malloc(set->c.count * sizeof(char *));
    if (!keys) return arr;
    for (size_t i = 0; i < set->c.count; i++) keys[i] = set->c.keys[i];
    qsort(keys, set->c.count, sizeof(char *), cxt_cmp_str);
    for (size_t i = 0; i < set->c.count; i++) json_append(arr, json_string(keys[i]));
    free(keys);
    return arr;
}
/* in-place sort of a string array (frozenset projection) */
static int cxt_cmp_jstr(const void *a, const void *b) {
    const json_t *ja = *(const json_t *const *)a;
    const json_t *jb = *(const json_t *const *)b;
    return strcmp(cxt_jstr(ja), cxt_jstr(jb));
}
static void cxt_sort_str_array(json_t *arr) {
    if (cxt_is_arr(arr) && arr->c.count > 1)
        qsort(arr->c.items, arr->c.count, sizeof(json_t *), cxt_cmp_jstr);
}
static json_t *cxt_build_set(const json_t *arr) {
    json_t *set = json_object();
    if (cxt_is_arr(arr))
        for (size_t i = 0; i < arr->c.count; i++)
            if (cxt_is_str(arr->c.items[i]))
                json_set(set, arr->c.items[i]->str_val, json_bool(true));
    return set;
}
static json_t *cxt_state_win(const char *key, json_t *node) {
    json_t *o = json_object();
    json_set(o, key, node ? node : json_null());
    return o;
}

static json_t *cxt_harness_run(cxt_ctx *ctx, const char *func, json_t *in, json_t **state_out) {
    *state_out = NULL;
    if (strcmp(func, "_coerce_timeout") == 0)
        return cxt_coerce_timeout(json_obj_get(in, "timeout"));
    if (strcmp(func, "_is_openai_api_origin") == 0)
        return cxt_is_openai_api_origin(ctx, json_obj_get(in, "base_url"));
    if (strcmp(func, "_is_perplexity_responses_backend") == 0)
        return cxt_is_perplexity_responses_backend(ctx, json_obj_get(in, "params"));
    if (strcmp(func, "_is_azure_responses") == 0)
        return cxt_is_azure_responses(ctx, json_obj_get(in, "params"));
    if (strcmp(func, "_reserves_tool_search") == 0)
        return cxt_reserves_tool_search(ctx, json_obj_get(in, "params"), json_obj_get(in, "is_xai_responses"));
    if (strcmp(func, "_openai_prefers_native_web_search") == 0)
        return cxt_openai_prefers_native_web_search(ctx);
    if (strcmp(func, "_is_official_openai_responses_route") == 0)
        return cxt_is_official_openai_responses_route(ctx, json_obj_get(in, "model"), json_obj_get(in, "base_url"));
    if (strcmp(func, "_codex_efforts_for_route") == 0)
        return cxt_codex_efforts_for_route(ctx, json_obj_get(in, "model"), json_obj_get(in, "base_url"),
                                           cxt_truthy(json_obj_get(in, "is_codex_backend")));
    if (strcmp(func, "_sends_all_turns") == 0)
        return cxt_sends_all_turns(ctx, json_obj_get(in, "model"), json_obj_get(in, "params"));
    if (strcmp(func, "_reasoning_fields") == 0)
        return cxt_reasoning_fields(ctx, json_obj_get(in, "model"), json_obj_get(in, "params"),
                                    json_obj_get(in, "effort"),
                                    cxt_truthy(json_obj_get(in, "enabled")),
                                    cxt_truthy(json_obj_get(in, "replay_encrypted_reasoning")),
                                    cxt_truthy(json_obj_get(in, "is_xai_responses")),
                                    cxt_truthy(json_obj_get(in, "profile_reasoning")),
                                    cxt_truthy(json_obj_get(in, "all_turns")));
    if (strcmp(func, "_bound_prompt_cache_key_field") == 0) {
        json_t *c = json_obj_get(in, "container");
        cxt_bound_prompt_cache_key_field(ctx, c);
        *state_out = cxt_state_win("container", json_copy(c));
        return NULL;
    }
    if (strcmp(func, "_merge_extra_headers") == 0) {
        json_t *kw = json_obj_get(in, "kwargs");
        cxt_merge_extra_headers(kw, json_obj_get(in, "headers"));
        *state_out = cxt_state_win("kwargs", json_copy(kw));
        return NULL;
    }
    if (strcmp(func, "_newest_reasoning_only") == 0)
        return cxt_newest_reasoning_only(json_obj_get(in, "messages"));
    if (strcmp(func, "_sanitize_astra_request_kwargs") == 0) {
        json_t *kw = json_obj_get(in, "kwargs");
        cxt_sanitize_astra_request_kwargs(ctx, kw, json_obj_get(in, "model"), json_obj_get(in, "base_url"));
        *state_out = cxt_state_win("kwargs", json_copy(kw));
        return NULL;
    }
    if (strcmp(func, "_resolve_reasoning") == 0) {
        json_t *warned = cxt_build_set(json_obj_get(in, "warned"));
        json_t *r = cxt_resolve_reasoning(ctx, json_obj_get(in, "model"), json_obj_get(in, "params"), warned);
        *state_out = cxt_state_win("warned", cxt_set_sorted(warned));
        json_free(warned);
        return r;
    }
    if (strcmp(func, "_alias_wire_tools") == 0)
        return cxt_alias_wire_tools(ctx, json_obj_get(in, "response_tools"), json_obj_get(in, "params"),
                                    json_obj_get(in, "is_xai_responses"), json_obj_get(in, "is_codex_backend"));
    if (strcmp(func, "reject_all_turns") == 0) {
        json_t *slf = json_obj_get(in, "self");
        json_t *r = cxt_reject_all_turns(ctx, slf, json_obj_get(in, "error"));
        cxt_sort_str_array(json_obj_get(slf, "_all_turns_rejected"));
        *state_out = cxt_state_win("self", json_copy(slf));
        return r;
    }
    if (strcmp(func, "drop_unverified_replay") == 0)
        return cxt_drop_unverified_replay(ctx, json_obj_get(in, "self"), json_obj_get(in, "message_lists"));
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
        cxt_ctx ctx;
        ctx.io = json_obj_get(work, "io");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *state = NULL;
        json_t *ret = cxt_harness_run(&ctx, func, work, &state);
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