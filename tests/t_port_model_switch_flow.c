/* t_port_model_switch_flow.c — harness for the hermes_cli/model_switch.py
 * switching FLOW port (src/cli/port_model_switch_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}} where "in"
 * carries the call arguments plus "env" (scripted tables/constants) and "io"
 * (seam-name -> script: single value, {"$seq": [...]} per-call queue, or
 * {"__raise__": cls, "msg": m, ...}). Emits JSON LINES:
 *   {"func", "in", "out": {"ret", "trace", "state"}}
 * "ret" is the canonical return (tuples/sets -> arrays, uncaught exception ->
 * {"__raise__": cls}); "trace" is the ordered seam-call sequence
 * [{"ev","args","ret"}]; "state" is the post-call _Switch field window for
 * the st-mutating functions ({} otherwise). The oracle drives LIVE
 * hermes_cli/model_switch.py under the SAME scripts and diffs out.
 */
#include "hermes_json.h"
#include "port_model_switch_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool msf_harness_is_st_func(const char *func) {
    return strcmp(func, "_apply_direct_alias_endpoint") == 0
        || strcmp(func, "_route_explicit_provider") == 0
        || strcmp(func, "_route_from_model_input") == 0
        || strcmp(func, "_route_configured_provider") == 0
        || strcmp(func, "_validate_switch") == 0;
}

static json_t *msf_harness_run(msf_ctx *ctx, const char *func, json_t *in, json_t **state_out) {
    *state_out = NULL;
    if (msf_harness_is_st_func(func)) {
        json_t *st = msf_switch_new(json_obj_get(in, "st"));
        json_t *ret = NULL;
        if (strcmp(func, "_apply_direct_alias_endpoint") == 0)
            ret = msf_apply_direct_alias_endpoint(ctx, st, json_obj_get(in, "da"));
        else if (strcmp(func, "_route_explicit_provider") == 0)
            ret = msf_route_explicit_provider(ctx, st);
        else if (strcmp(func, "_route_from_model_input") == 0)
            ret = msf_route_from_model_input(ctx, st);
        else if (strcmp(func, "_route_configured_provider") == 0)
            ret = msf_route_configured_provider(ctx, st);
        else if (strcmp(func, "_validate_switch") == 0)
            ret = msf_validate_switch(ctx, st);
        *state_out = st; /* emitted even when an exception propagated */
        return ret;
    }
    if (strcmp(func, "_provider_identity") == 0)
        return msf_provider_identity(ctx, json_get_str(in, "name", ""),
                                     json_obj_get(in, "user_providers"),
                                     json_obj_get(in, "custom_providers"));
    if (strcmp(func, "resolve_alias") == 0)
        return msf_resolve_alias(ctx, json_get_str(in, "raw_input", ""),
                                 json_get_str(in, "current_provider", ""),
                                 json_obj_get(in, "user_providers"),
                                 json_obj_get(in, "custom_providers"));
    if (strcmp(func, "_external_process_catalog") == 0)
        return msf_external_process_catalog(ctx, json_get_str(in, "provider", ""));
    if (strcmp(func, "_external_process_match") == 0)
        return msf_external_process_match(ctx, json_obj_get(in, "catalog"),
                                          json_obj_get(in, "aliases"),
                                          json_get_str(in, "typed", ""),
                                          json_get_str(in, "provider", ""));
    if (strcmp(func, "_aggregator_catalog_match") == 0)
        return msf_aggregator_catalog_match(ctx, json_get_str(in, "new_model", ""),
                                            json_obj_get(in, "catalog"));
    if (strcmp(func, "_config_declares_model") == 0)
        return msf_config_declares_model(ctx, json_get_str(in, "new_model", ""),
                                         json_get_str(in, "target_provider", ""),
                                         json_get_str(in, "base_url", ""),
                                         json_obj_get(in, "user_providers"),
                                         json_obj_get(in, "custom_providers"));
    if (strcmp(func, "_unknown_provider_message") == 0)
        return msf_unknown_provider_message(ctx, json_get_str(in, "explicit_provider", ""));
    if (strcmp(func, "_aggregator_alias_error") == 0)
        return msf_aggregator_alias_error(ctx, json_get_str(in, "explicit_provider", ""),
                                          json_get_str(in, "target_provider", ""),
                                          json_get_str(in, "current_provider", ""),
                                          json_obj_get(in, "user_providers"),
                                          json_obj_get(in, "custom_providers"));
    if (strcmp(func, "model_selection_config_updates") == 0)
        return msf_model_selection_config_updates(ctx, json_obj_get(in, "result"),
                                                  json_obj_get(in, "current_model_cfg"));
    if (strcmp(func, "selection_route_changed") == 0)
        return msf_selection_route_changed(ctx, json_obj_get(in, "model_cfg"),
                                           json_obj_get(in, "result"));
    if (strcmp(func, "_merge_selection_updates") == 0)
        return msf_merge_selection_updates(ctx, json_obj_get(in, "model_cfg"),
                                           json_obj_get(in, "updates"),
                                           json_get_bool(in, "mask_cleared", false));
    if (strcmp(func, "apply_model_selection") == 0)
        return msf_apply_model_selection(ctx, json_obj_get(in, "model_cfg"),
                                         json_obj_get(in, "result"),
                                         json_get_bool(in, "mask_cleared", false));
    if (strcmp(func, "persist_model_selection") == 0)
        return msf_persist_model_selection(ctx, json_obj_get(in, "result"),
                                           json_get_str(in, "config_path", NULL));
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
        msf_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.env = json_obj_get(in, "env");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *state = NULL;
        json_t *ret = msf_harness_run(&ctx, func, in, &state);
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
