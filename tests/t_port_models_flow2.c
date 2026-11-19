/* t_port_models_flow2.c — harness for the hermes_cli/models.py UNCLAIMED-
 * remainder FLOW port (src/cli/models_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...args..., "io": {seam:
 * script}, "env": {...}}}. Emits JSON LINES: {"func","in","out":{"ret","trace",
 * "state"}}. "ret" is the canonical return (tuples/sets -> arrays, list
 * subclasses -> {"$curated"/"$native": [...]}, None -> null, uncaught exception
 * -> {"__raise__": cls}); "trace" is the ordered seam-call sequence
 * [{"ev","args","ret"}]; "state" is the post-call out-state (the seen_cursors
 * set for _anthropic_next_cursor; {} otherwise, and {} on any raise). The oracle
 * drives LIVE hermes_cli/models.py under the SAME scripts and diffs out.
 *
 * Self-contained: #includes the area-qualified port .c (libjson-only).
 */
#include "hermes_json.h"
#include "cli/models_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* sorted array of a string-set object's keys (sets project as sorted arrays) */
static int md2_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
static json_t *md2_set_sorted(const json_t *set) {
    json_t *arr = json_array();
    if (!md2_is_obj(set) || set->c.count == 0) return arr;
    const char **keys = (const char **)malloc(set->c.count * sizeof(char *));
    if (!keys) return arr;
    for (size_t i = 0; i < set->c.count; i++) keys[i] = set->c.keys[i];
    qsort(keys, set->c.count, sizeof(char *), md2_cmp_str);
    for (size_t i = 0; i < set->c.count; i++) json_append(arr, json_string(keys[i]));
    free(keys);
    return arr;
}

static json_t *md2_harness_run(md2_ctx *ctx, const char *func, json_t *in, json_t **state_out) {
    *state_out = NULL;
    if (strcmp(func, "_zero_priced") == 0) return md2_zero_priced(ctx, in);
    if (strcmp(func, "_is_subscription_billed") == 0) return md2_is_subscription_billed(ctx, in);
    if (strcmp(func, "_union_with_portal_recommendations") == 0)
        return md2_union_with_portal_recommendations(ctx, in);
    if (strcmp(func, "union_with_nous_on_sale_models") == 0)
        return md2_union_with_nous_on_sale_models(ctx, in);
    if (strcmp(func, "_pricing_profile_key") == 0) return md2_pricing_profile_key(ctx, in);
    if (strcmp(func, "get_cached_nous_free_tier") == 0) return md2_get_cached_nous_free_tier(ctx, in);
    if (strcmp(func, "_is_anthropic_frontier_tier") == 0) return md2_is_anthropic_frontier_tier(ctx, in);
    if (strcmp(func, "_anthropic_models_url") == 0) return md2_anthropic_models_url(ctx, in);
    if (strcmp(func, "_anthropic_next_cursor") == 0) {
        json_t *seen = json_object();
        json_t *init = md2_get(in, "seen");
        if (md2_is_arr(init))
            for (size_t i = 0; i < init->c.count; i++)
                if (md2_is_str(init->c.items[i])) json_set(seen, init->c.items[i]->str_val, json_bool(true));
        json_t *r = md2_anthropic_next_cursor(ctx, in, seen);
        if (!md2_exc_pending(ctx)) {
            json_t *st = json_object();
            json_set(st, "seen", md2_set_sorted(seen));
            *state_out = st;
        }
        json_free(seen);
        return r;
    }
    if (strcmp(func, "_fast_mode_route_supported") == 0) return md2_fast_mode_route_supported(ctx, in);
    if (strcmp(func, "model_supports_ultrafast") == 0) return md2_model_supports_ultrafast(ctx, in);
    if (strcmp(func, "_cache_entry") == 0)
        return md2_cache_entry(ctx, json_get_str(in, "fp", ""), md2_get(in, "models"), md2_get(in, "at"));
    if (strcmp(func, "_live_result_entry") == 0) return md2_live_result_entry(ctx, in);
    if (strcmp(func, "_normalized_cache_slug") == 0) return md2_normalized_cache_slug(ctx, in);
    if (strcmp(func, "_model_requires_account_discovery") == 0)
        return md2_model_requires_account_discovery(ctx, in);
    if (strcmp(func, "_ollama_native_probe_reachable") == 0) return md2_ollama_native_probe_reachable(ctx, in);
    if (strcmp(func, "_resolve_anthropic_pool_catalog_credentials") == 0)
        return md2_resolve_anthropic_pool_catalog_credentials(ctx, in);
    if (strcmp(func, "_probe_neg_key") == 0) return md2_probe_neg_key(ctx, in);
    if (strcmp(func, "_probe_result") == 0) return md2_probe_result(ctx, in);
    if (strcmp(func, "_deepinfra_env") == 0) return md2_deepinfra_env(ctx, in);
    if (strcmp(func, "_chat_catalog_rows") == 0) return md2_chat_catalog_rows(ctx, md2_get(in, "models"));
    if (strcmp(func, "_codex_catalog") == 0) return md2_codex_catalog(ctx, in);
    if (strcmp(func, "_nous_catalog") == 0) return md2_nous_catalog(ctx, in);
    if (strcmp(func, "_anthropic_catalog") == 0) return md2_anthropic_catalog(ctx, in);
    if (strcmp(func, "_static_catalog") == 0) return md2_static_catalog(ctx, in);
    if (strcmp(func, "_store_cache_entry") == 0)
        return md2_store_cache_entry(ctx, json_get_str(in, "cache_key", ""), md2_get(in, "entry"));
    if (strcmp(func, "cached_fetch_api_models") == 0) return md2_cached_fetch_api_models(ctx, in);
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
        md2_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.env = json_obj_get(in, "env");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *state = NULL;
        json_t *ret = md2_harness_run(&ctx, func, in, &state);
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_string(md2_exc_class(&ctx)));
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