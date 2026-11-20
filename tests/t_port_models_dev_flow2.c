/* t_port_models_dev_flow2.c — harness for the agent/models_dev.py UNCLAIMED-
 * remainder FLOW port (src/agent/models_dev_flow2.c).
 *
 * Reads JSON LINES on stdin: {"func": <name>, "in": {...args..., "io":
 * {seam: script}, "env": {...}}}. Emits JSON LINES: {"func","in","out":
 * {"ret","trace","state"}}. "ret" is the canonical return (None -> null,
 * tuple -> array, uncaught exception -> {"__raise__": cls}); "trace" is the
 * ordered seam-call sequence [{"ev","args","ret"}]; "state" is always {}
 * (mutable globals are seeded fresh per case from in.env). The oracle drives
 * LIVE agent/models_dev.py under the SAME scripts and diffs out.
 *
 * Self-contained: #includes the area-qualified port .c (libjson-only).
 */
#include "hermes_json.h"
#include "agent/models_dev_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *md2_harness_run(md2_ctx *ctx, const char *func, json_t *in) {
    if (strcmp(func, "_profile_models_dev_ids") == 0) return md2_profile_models_dev_ids(ctx, in);
    if (strcmp(func, "models_dev_provider_map") == 0) return md2_models_dev_provider_map(ctx, in);
    if (strcmp(func, "models_dev_id_for") == 0) return md2_models_dev_id_for(ctx, in);
    if (strcmp(func, "_dict_or_empty") == 0) return md2_dict_or_empty(ctx, in);
    if (strcmp(func, "_configured_catalog_provider") == 0) return md2_configured_catalog_provider(ctx, in);
    if (strcmp(func, "_models_dev_id") == 0) return md2_models_dev_id(ctx, in);
    if (strcmp(func, "_cfg_get") == 0) return md2_cfg_get(ctx, in);
    if (strcmp(func, "_hermes_path") == 0) return md2_hermes_path(ctx, in);
    if (strcmp(func, "_quietly") == 0) return md2_quietly(ctx, in);
    if (strcmp(func, "_serve_stale") == 0) return md2_serve_stale(ctx, in);
    if (strcmp(func, "_refresh_locked") == 0) return md2_refresh_locked(ctx, in);
    if (strcmp(func, "_registry_provider") == 0) return md2_registry_provider(ctx, in);
    if (strcmp(func, "_registry_models") == 0) return md2_registry_models(ctx, in);
    if (strcmp(func, "_openrouter_catalog_lookup_base") == 0) return md2_openrouter_catalog_lookup_base(ctx, in);
    if (strcmp(func, "_iter_model_entries") == 0) return md2_iter_model_entries(ctx, in);
    if (strcmp(func, "_extract_limit") == 0) return md2_extract_limit(ctx, in);
    if (strcmp(func, "_builtin_model_metadata") == 0) return md2_builtin_model_metadata(ctx, in);
    if (strcmp(func, "_relay_vision_marker_metadata") == 0) return md2_relay_vision_marker_metadata(ctx, in);
    if (strcmp(func, "_provider_model_capabilities") == 0) return md2_provider_model_capabilities(ctx, in);
    if (strcmp(func, "_apply_overrides") == 0) return md2_apply_overrides(ctx, in);
    if (strcmp(func, "_entry_supports_vision") == 0) return md2_entry_supports_vision(ctx, in);
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
        json_t *ret = md2_harness_run(&ctx, func, in);
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_string(md2_exc_class(&ctx)));
            json_free(ctx.exc);
            json_free(ret);
        } else {
            ret_out = ret ? ret : json_null();
        }
        json_t *out = json_object();
        json_set(out, "ret", ret_out);
        json_set(out, "trace", ctx.trace);
        json_set(out, "state", json_object());
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
