/* t_port_auxiliary_client_flow2.c — harness for the agent/auxiliary_client.py
 * UNCLAIMED-remainder FLOW port (src/agent/auxiliary_client_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...args..., "io": {seam:
 * script}, "env": {...}}}. Emits JSON LINES: {"func","in","out":{"ret","trace",
 * "state"}}. "ret" is the canonical return (tuples/sets -> arrays, None -> null,
 * uncaught exception -> {"__raise__": cls}); "trace" is the ordered seam-call
 * sequence [{"ev","args","ret"}]; "state" is the post-call out-state
 * (_log_once_debug seen set, _warn_stale_openai_base_url warned flag,
 * _cached_semaphore store; {} otherwise, and {} on any raise). The oracle drives
 * LIVE agent/auxiliary_client.py under the SAME scripts and diffs out.
 *
 * Self-contained: #includes the area-qualified port .c (libjson-only).
 */
#include "hermes_json.h"
#include "agent/auxiliary_client_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* sorted array of a string-set object's keys (sets project as sorted arrays) */
static int ac2_cmp_str(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}
static json_t *ac2_set_sorted(const json_t *set) {
    json_t *arr = json_array();
    if (!ac2_is_obj(set) || set->c.count == 0) return arr;
    const char **keys = (const char **)malloc(set->c.count * sizeof(char *));
    if (!keys) return arr;
    for (size_t i = 0; i < set->c.count; i++) keys[i] = set->c.keys[i];
    qsort(keys, set->c.count, sizeof(char *), ac2_cmp_str);
    for (size_t i = 0; i < set->c.count; i++) json_append(arr, json_string(keys[i]));
    free(keys);
    return arr;
}

static json_t *ac2_harness_run(ac2_ctx *ctx, const char *func, json_t *in, json_t **state_out) {
    *state_out = NULL;
    json_t *req = ac2_get(in, "req");

    if (strcmp(func, "_tick_hook") == 0)
        return ac2_tick_hook(ctx, ac2_get(in, "local"), json_get_str(in, "label", ""));
    if (strcmp(func, "_notify_aux_dispatch") == 0)
        return ac2_notify_aux_dispatch(ctx, in);
    if (strcmp(func, "_notify_aux_timing_response") == 0)
        return ac2_notify_aux_timing_response(ctx, in);
    if (strcmp(func, "_notify_aux_provider_response") == 0)
        return ac2_notify_aux_provider_response(ctx, in);
    if (strcmp(func, "_ai_gateway_headers") == 0)
        return ac2_ai_gateway_headers(ctx, in);
    if (strcmp(func, "_auth_json_path") == 0)
        return ac2_auth_json_path(ctx, in);
    if (strcmp(func, "_load_pool_with_credentials") == 0)
        return ac2_load_pool_with_credentials(ctx, in);
    if (strcmp(func, "_close_quietly") == 0)
        return ac2_close_quietly(ctx, in);
    if (strcmp(func, "_attempt_stream_socket") == 0)
        return ac2_attempt_stream_socket(ctx, in);
    if (strcmp(func, "_route_client_headers") == 0)
        return ac2_route_client_headers(ctx, json_get_str(in, "base_url", ""),
                                        json_get_str(in, "provider", ""),
                                        json_get_bool(in, "is_vision", false));
    if (strcmp(func, "_profile_default_headers") == 0)
        return ac2_profile_default_headers(ctx, json_get_str(in, "provider", ""));
    if (strcmp(func, "_endpoint_default_headers") == 0)
        return ac2_endpoint_default_headers(ctx, in);
    if (strcmp(func, "_codex_base_url_override") == 0)
        return ac2_codex_base_url_override(ctx, in);
    if (strcmp(func, "_profile_declared_messages_wire") == 0)
        return ac2_profile_declared_messages_wire(ctx, in);
    if (strcmp(func, "_route_client") == 0)
        return ac2_route_client(ctx, req, ac2_get(in, "client_obj"), ac2_get(in, "final_model_str"));
    if (strcmp(func, "_route_or_warn") == 0)
        return ac2_route_or_warn(ctx, req, ac2_get(in, "client"), ac2_get(in, "default"),
                                 json_get_str(in, "unavailable_msg", ""), ac2_get(in, "args"));
    if (strcmp(func, "_resolve_openrouter_branch") == 0)
        return ac2_resolve_openrouter_branch(ctx, in);
    if (strcmp(func, "_resolve_nous_branch") == 0)
        return ac2_resolve_nous_branch(ctx, in);
    if (strcmp(func, "_resolve_xai_oauth_branch") == 0)
        return ac2_resolve_xai_oauth_branch(ctx, in);
    if (strcmp(func, "_resolve_azure_foundry_branch") == 0)
        return ac2_resolve_azure_foundry_branch(ctx, in);
    if (strcmp(func, "_resolve_openai_codex_branch") == 0)
        return ac2_resolve_openai_codex_branch(ctx, in);
    if (strcmp(func, "_discovery_chain_allowed") == 0)
        return ac2_discovery_chain_allowed(ctx, in);
    if (strcmp(func, "_log_once_debug") == 0) {
        json_t *seen = json_object();
        json_t *init = ac2_get(in, "seen");
        if (ac2_is_arr(init))
            for (size_t i = 0; i < init->c.count; i++)
                if (ac2_is_str(init->c.items[i])) json_set(seen, init->c.items[i]->str_val, json_bool(true));
        json_t *r = ac2_log_once_debug(ctx, in, seen);
        if (!ac2_exc_pending(ctx)) {
            json_t *st = json_object();
            json_set(st, "seen", ac2_set_sorted(seen));
            *state_out = st;
        }
        json_free(seen);
        return r;
    }
    if (strcmp(func, "_warn_stale_openai_base_url") == 0) {
        bool warned = json_get_bool(ctx->env, "warned", false);
        json_t *r = ac2_warn_stale_openai_base_url(ctx, in, &warned);
        if (!ac2_exc_pending(ctx)) {
            json_t *st = json_object();
            json_set(st, "warned", json_bool(warned));
            *state_out = st;
        }
        return r;
    }
    if (strcmp(func, "_is_actual_auxiliary_route") == 0)
        return ac2_is_actual_auxiliary_route(ctx, in);
    if (strcmp(func, "_main_route_target") == 0)
        return ac2_main_route_target(ctx, in);
    if (strcmp(func, "_read_main_field") == 0)
        return ac2_read_main_field(ctx, in);
    if (strcmp(func, "_read_main_api_key_if_same_origin") == 0)
        return ac2_read_main_api_key_if_same_origin(ctx, json_get_str(in, "aux_base_url", ""));
    if (strcmp(func, "_borrowed_main_credential_key") == 0)
        return ac2_borrowed_main_credential_key(ctx, in);
    if (strcmp(func, "_cached_semaphore") == 0) {
        json_t *store = json_object();
        json_t *init = ac2_get(in, "store");
        if (ac2_is_obj(init))
            for (size_t i = 0; i < init->c.count; i++)
                json_set(store, init->c.keys[i], json_copy(init->c.items[i]));
        json_t *r = ac2_cached_semaphore(ctx, in, store);
        if (!ac2_exc_pending(ctx)) {
            json_t *st = json_object();
            json_set(st, "store", json_copy(store));
            *state_out = st;
        }
        json_free(store);
        return r;
    }
    if (strcmp(func, "_current_event_loop") == 0)
        return ac2_current_event_loop(ctx, in);
    if (strcmp(func, "_async_client_streams_internally") == 0)
        return ac2_async_client_streams_internally(ctx, in);
    if (strcmp(func, "_refresh_codex_credentials") == 0)
        return ac2_refresh_codex_credentials(ctx, in);
    if (strcmp(func, "_refresh_nous_credentials") == 0)
        return ac2_refresh_nous_credentials(ctx, in);
    if (strcmp(func, "_refresh_anthropic_credentials") == 0)
        return ac2_refresh_anthropic_credentials(ctx, in);
    if (strcmp(func, "_refresh_xai_oauth_credentials") == 0)
        return ac2_refresh_xai_oauth_credentials(ctx, in);
    if (strcmp(func, "_refresh_vertex_credentials") == 0)
        return ac2_refresh_vertex_credentials(ctx, in);
    if (strcmp(func, "_deepinfra_strict_vision_backend") == 0)
        return ac2_deepinfra_strict_vision_backend(ctx, in);
    if (strcmp(func, "_finalize_vision_client") == 0)
        return ac2_finalize_vision_client(ctx, json_get_str(in, "resolved_provider", ""),
                                          ac2_get(in, "sync_client"), ac2_get(in, "default_model"),
                                          ac2_get(in, "resolved_model"),
                                          json_get_bool(in, "async_mode", false));
    if (strcmp(func, "_vision_auto_route") == 0)
        return ac2_vision_auto_route(ctx, in);
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
        ac2_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.env = json_obj_get(in, "env");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *state = NULL;
        json_t *ret = ac2_harness_run(&ctx, func, in, &state);
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_string(ac2_exc_class(&ctx)));
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