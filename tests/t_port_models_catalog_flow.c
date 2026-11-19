/* t_port_models_catalog_flow.c — harness for the hermes_cli/models.py
 * catalog/selection FLOW port (src/cli/port_models_catalog_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}} where "in"
 * carries the call arguments plus "env" (scripted tables/constants) and "io"
 * (seam-name -> script: single value, {"$seq": [...]} per-call queue, or
 * {"__raise__": cls, "msg": m}). Emits JSON LINES:
 *   {"func", "in", "out", "trace"}
 * "out" is the canonical return (tuples/sets -> arrays, CuratedFallbackModels
 * -> {"$curated": [...]}, uncaught exception -> {"__raise__": cls}); "trace"
 * is the ordered seam-call sequence [{"ev","args","ret"}]. The oracle drives
 * LIVE models.py under the SAME scripts and diffs out + trace.
 */
#include "hermes_json.h"
#include "port_models_catalog_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *mcat_harness_run(mcat_ctx *ctx, const char *func, json_t *in) {
    if (strcmp(func, "_merge_unique") == 0) {
        return mcat_merge_unique(ctx, json_obj_get(in, "primary"), json_obj_get(in, "secondary"),
                                 json_get_bool(in, "dedup_key", false));
    }
    if (strcmp(func, "_configured_provider_ids") == 0) {
        return mcat_configured_provider_ids(ctx);
    }
    if (strcmp(func, "_resolve_provider_prefix") == 0) {
        return mcat_resolve_provider_prefix(ctx, json_get_str(in, "model_name", ""));
    }
    if (strcmp(func, "static_catalog") == 0) {
        return mcat_static_catalog(ctx, json_get_str(in, "provider", ""));
    }
    if (strcmp(func, "static_catalog_providers") == 0) {
        return mcat_static_catalog_providers(ctx);
    }
    if (strcmp(func, "_static_catalog_matches") == 0) {
        return mcat_static_catalog_matches(ctx, json_get_str(in, "name", ""),
                                           json_get_str(in, "current_provider", ""));
    }
    if (strcmp(func, "_detection_candidates") == 0) {
        return mcat_detection_candidates(ctx, json_get_str(in, "name", ""),
                                         json_get_str(in, "current_provider", ""));
    }
    if (strcmp(func, "_catalog_fetcher") == 0) {
        return mcat_catalog_fetcher(ctx, json_get_str(in, "normalized", ""));
    }
    if (strcmp(func, "_api_key_credentials") == 0) {
        return mcat_api_key_credentials(ctx, json_get_str(in, "normalized", ""));
    }
    if (strcmp(func, "_api_key_provider_live") == 0) {
        return mcat_api_key_provider_live(ctx, json_get_str(in, "normalized", ""),
                                          json_get_bool(in, "force_refresh", false));
    }
    if (strcmp(func, "_stepfun_catalog") == 0) {
        return mcat_stepfun_catalog(ctx, json_get_str(in, "normalized", ""),
                                    json_get_bool(in, "force_refresh", false));
    }
    if (strcmp(func, "_profile_live_catalog") == 0) {
        return mcat_profile_live_catalog(ctx, json_get_str(in, "normalized", ""));
    }
    if (strcmp(func, "probe_profile_catalog") == 0) {
        return mcat_probe_profile_catalog(ctx, json_get_str(in, "normalized", ""),
                                          json_obj_get(in, "profile"),
                                          json_obj_get(in, "api_key"),
                                          json_obj_get(in, "base_url"));
    }
    if (strcmp(func, "merge_profile_catalog") == 0) {
        return mcat_merge_profile_catalog(ctx, json_get_str(in, "normalized", ""),
                                          json_obj_get(in, "profile"), json_obj_get(in, "live"));
    }
    if (strcmp(func, "_drop_delisted_opencode_models") == 0) {
        return mcat_drop_delisted_opencode_models(ctx, json_get_str(in, "normalized", ""),
                                                  json_copy(json_obj_get(in, "rows")));
    }
    if (strcmp(func, "_configured_relay_base_url") == 0) {
        return mcat_configured_relay_base_url(ctx, json_get_str(in, "provider", ""));
    }
    if (strcmp(func, "_relay_model_catalog") == 0) {
        return mcat_relay_model_catalog(ctx, json_get_str(in, "normalized", ""),
                                        json_get_str(in, "relay", ""));
    }
    if (strcmp(func, "_disk_serve_tier") == 0) {
        double ttl = json_get_num(in, "ttl_seconds", mcat_env_num(ctx, "cache_ttl", 3600));
        return mcat_disk_serve_tier(ctx, json_obj_get(in, "entry"), json_get_str(in, "fp", ""),
                                    json_get_num(in, "now", 0), json_get_bool(in, "is_ollama", false), ttl);
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
        mcat_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.env = json_obj_get(in, "env");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *ret = mcat_harness_run(&ctx, func, in);
        json_t *out;
        if (ctx.exc) {
            out = json_object();
            json_set(out, "__raise__", json_copy(json_obj_get(ctx.exc, "__raise__")));
            json_free(ctx.exc);
            json_free(ret);
        } else {
            out = ret ? ret : json_null();
        }
        json_t *emit = json_object();
        json_set(emit, "func", json_string(func));
        json_set(emit, "in", json_copy(in));
        json_set(emit, "out", out);
        json_set(emit, "trace", ctx.trace);
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
