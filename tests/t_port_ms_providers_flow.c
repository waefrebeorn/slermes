/* t_port_ms_providers_flow.c — harness for the hermes_cli/model_switch_providers.py
 * FLOW port (src/cli/port_ms_providers_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}} where "in" carries
 * the call arguments plus "env" (scripted tables/constants) and "io" (seam-name
 * -> script: single value, {"$seq": [...]} per-call queue, or {"__raise__": cls,
 * "msg": m, ...}). Emits JSON LINES:
 *   {"func", "in", "out": {"ret", "trace", "state"}}
 * "ret" is the canonical return (tuples/sets -> arrays, uncaught exception ->
 * {"__raise__": cls}); "trace" is the ordered seam-call sequence
 * [{"ev","args","ret"}]; "state" is the post-call results argument for
 * _finalize_picker_rows ({} otherwise). The oracle drives LIVE
 * hermes_cli/model_switch_providers.py under the SAME scripts and diffs out.
 */
#include "hermes_json.h"
#include "port_ms_providers_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *msp_harness_run(msp_ctx *ctx, const char *func, json_t *in, json_t **state_out) {
    *state_out = NULL;
    if (strcmp(func, "_discovered_catalog_stale") == 0)
        return msp_discovered_catalog_stale(ctx, json_obj_get(in, "entry"), json_obj_get(in, "model_ids"));
    if (strcmp(func, "_any_env") == 0)
        return msp_any_env(ctx, json_obj_get(in, "env_vars"));
    if (strcmp(func, "_skip") == 0)
        return msp_skip(ctx, json_obj_get(in, "seen"), json_obj_get(in, "excluded"), json_obj_get(in, "keys"));
    if (strcmp(func, "_auth_store_has_provider") == 0)
        return msp_auth_store_has_provider(ctx, json_obj_get(in, "keys"));
    if (strcmp(func, "_raw_pool_usable") == 0)
        return msp_raw_pool_usable(ctx, json_get_str(in, "hermes_id", ""), json_get_bool(in, "for_picker", false));
    if (strcmp(func, "_pool_usable") == 0)
        return msp_pool_usable(ctx, json_get_str(in, "slug", ""), json_get_bool(in, "for_picker", false));
    if (strcmp(func, "_overlay_has_env_creds") == 0)
        return msp_overlay_has_env_creds(ctx, json_get_str(in, "pid", ""), json_get_str(in, "hermes_slug", ""),
                                         json_obj_get(in, "overlay"));
    if (strcmp(func, "_azure_entra_configured") == 0)
        return msp_azure_entra_configured(ctx);
    if (strcmp(func, "_has_fast_aws_sdk_signal") == 0)
        return msp_has_fast_aws_sdk_signal(ctx);
    if (strcmp(func, "_has_aws_sdk_creds_for_listing") == 0)
        return msp_has_aws_sdk_creds_for_listing(ctx, json_get_str(in, "slug", NULL),
                                                 json_get_str(in, "current_provider", NULL));
    if (strcmp(func, "_live_or_curated_ids") == 0)
        return msp_live_or_curated_ids(ctx, json_get_str(in, "slug", ""), json_obj_get(in, "curated"),
                                       json_obj_get(in, "fallback_keys"),
                                       json_get_bool(in, "merge_models_dev", true),
                                       json_get_bool(in, "non_blocking", false));
    if (strcmp(func, "_aws_live_or_curated_ids") == 0)
        return msp_aws_live_or_curated_ids(ctx, json_get_str(in, "slug", ""), json_obj_get(in, "curated"),
                                           json_obj_get(in, "fallback_keys"),
                                           json_get_bool(in, "non_blocking", false));
    if (strcmp(func, "_nous_picker_model_ids") == 0)
        return msp_nous_picker_model_ids(ctx, json_obj_get(in, "curated"),
                                         json_get_bool(in, "force_fresh_nous_tier", false));
    if (strcmp(func, "_free_tier_nous_row") == 0)
        return msp_free_tier_nous_row(ctx, json_obj_get(in, "row"));
    if (strcmp(func, "_cap_models") == 0)
        return msp_cap_models(ctx, json_obj_get(in, "model_ids"), json_obj_get(in, "max_models"),
                              json_get_str(in, "slug", ""));
    if (strcmp(func, "_discover_endpoint_models") == 0)
        return msp_discover_endpoint_models(ctx, json_obj_get(in, "api_key"), json_get_str(in, "api_url", ""),
                                            json_get_str(in, "native_catalog_provider", ""),
                                            json_get_bool(in, "has_explicit_models", false),
                                            json_obj_get(in, "headers"), json_obj_get(in, "api_mode"),
                                            json_get_bool(in, "probe_live", false),
                                            json_get_bool(in, "discovery_allowed", false),
                                            json_get_bool(in, "fast_custom_probe", false));
    if (strcmp(func, "_finalize_picker_rows") == 0) {
        json_t *results = json_copy(json_obj_get(in, "results"));
        if (!results) results = json_array();
        json_t *ret = msp_finalize_picker_rows(ctx, results, json_obj_get(in, "user_providers"),
                                               json_get_str(in, "current_model", ""));
        *state_out = results; /* emitted even when an exception propagated */
        return ret;
    }
    if (strcmp(func, "_saved_entry_on_other_origin") == 0)
        return msp_saved_entry_on_other_origin(ctx, json_get_str(in, "name", ""), json_get_str(in, "base_url", ""));
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
        msp_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.env = json_obj_get(in, "env");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *state = NULL;
        json_t *ret = msp_harness_run(&ctx, func, in, &state);
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