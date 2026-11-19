/* t_port_model_metadata_flow.c — harness for the agent/model_metadata.py
 * UNCLAIMED-remainder FLOW port (src/cli/model_metadata_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...args..., "io": {seam:
 * script}, "env": {...}, "state": {...}}}. Emits JSON LINES: {"func","in",
 * "out":{"ret","trace","state"}}. "ret" is the canonical return (tuples/sets ->
 * arrays, None -> null, Path -> POSIX str, uncaught exception -> {"__raise__":
 * cls}); "trace" is the ordered seam-call sequence [{"ev","args","ret"}];
 * "state" is the post-call out-state (the mutated memo/cache window for the
 * stateful flows; {} otherwise, and {} on any raise). The oracle drives LIVE
 * agent/model_metadata.py under the SAME scripts and diffs out.
 *
 * Self-contained: #includes the area-qualified port .c (libjson-only).
 */
#include "hermes_json.h"
#include "cli/model_metadata_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* seed ctx->state[key] from in.state[key] (or an empty object). */
static void mm_seed_state(mm_ctx *ctx, json_t *in, const char *key) {
    json_t *st = mm_get(in, "state");
    json_t *src = mm_get(st, key);
    json_set(ctx->state, key, mm_is_obj(src) ? json_copy(src) : json_object());
}

static json_t *mm_harness_run(mm_ctx *ctx, const char *func, json_t *in, json_t **state_out) {
    *state_out = NULL;
    if (strcmp(func, "_count_parts") == 0) return mm_count_parts(ctx, in);
    if (strcmp(func, "_lmstudio_loaded_context") == 0) return mm_lmstudio_loaded_context(ctx, in);
    if (strcmp(func, "_codex_catalog_urls") == 0) return mm_codex_catalog_urls(ctx, in);
    if (strcmp(func, "_ollama_show_context") == 0) return mm_ollama_show_context(ctx, in);
    if (strcmp(func, "_endpoint_model_entry") == 0) return mm_endpoint_model_entry(ctx, in);
    if (strcmp(func, "_note_if_connect_timeout") == 0) return mm_note_if_connect_timeout(ctx, in);
    if (strcmp(func, "_save_unless_skipped") == 0) return mm_save_unless_skipped(ctx, in);
    if (strcmp(func, "_probe_local_context_length") == 0) return mm_probe_local_context_length(ctx, in);
    if (strcmp(func, "save_provider_context_length") == 0) return mm_save_provider_context_length(ctx, in);
    if (strcmp(func, "_is_ollama_server") == 0) return mm_is_ollama_server(ctx, in);
    if (strcmp(func, "_cache_file") == 0) return mm_cache_file(ctx, in);
    if (strcmp(func, "_load_json_dict") == 0) return mm_load_json_dict(ctx, in);
    if (strcmp(func, "_ttl_memo_get") == 0) return mm_ttl_memo_get(ctx, in);
    if (strcmp(func, "_ttl_memo_put") == 0) return mm_ttl_memo_put(ctx, in);
    if (strcmp(func, "_memo_local_probe") == 0) {
        mm_seed_state(ctx, in, "_LOCAL_CTX_PROBE_CACHE");
        json_t *r = mm_memo_local_probe(ctx, in);
        if (!mm_exc_pending(ctx)) *state_out = json_copy(ctx->state);
        return r;
    }
    if (strcmp(func, "_bedrock_probe_failed_recently") == 0) {
        mm_seed_state(ctx, in, "_BEDROCK_PROBE_FAILURE_CACHE");
        json_t *r = mm_bedrock_probe_failed_recently(ctx, in);
        if (!mm_exc_pending(ctx)) *state_out = json_copy(ctx->state);
        return r;
    }
    if (strcmp(func, "_remember_no_codex_catalog") == 0) {
        mm_seed_state(ctx, in, "_codex_oauth_context_cache");
        json_t *r = mm_remember_no_codex_catalog(ctx, in);
        if (!mm_exc_pending(ctx)) *state_out = json_copy(ctx->state);
        return r;
    }
    if (strcmp(func, "_cached_codex_catalog_max") == 0) {
        mm_seed_state(ctx, in, "_codex_oauth_max_context_cache");
        return mm_cached_codex_catalog_max(ctx, in);   /* read-only: state stays {} */
    }
    if (strcmp(func, "_validate_cached_context_length") == 0) return mm_validate_cached_context_length(ctx, in);
    if (strcmp(func, "fetch_codex_catalog_entries") == 0) return mm_fetch_codex_catalog_entries(ctx, in);
    if (strcmp(func, "_codex_catalog_probe_allowed") == 0) return mm_codex_catalog_probe_allowed(ctx, in);
    if (strcmp(func, "_parse_models_payload") == 0) return mm_parse_models_payload(ctx, in);
    if (strcmp(func, "estimate_native_anthropic_messages_tokens_rough") == 0)
        return mm_estimate_native_anthropic_messages_tokens_rough(ctx, in);
    if (strcmp(func, "estimate_native_anthropic_request_tokens_rough") == 0)
        return mm_estimate_native_anthropic_request_tokens_rough(ctx, in);
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
        mm_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.env = json_obj_get(in, "env");
        ctx.state = json_object();
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *state = NULL;
        json_t *ret = mm_harness_run(&ctx, func, in, &state);
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_string(mm_exc_class(&ctx)));
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
        json_free(ctx.state);
        json_free(empty_in);
        json_free(rec);
    }
    return 0;
}