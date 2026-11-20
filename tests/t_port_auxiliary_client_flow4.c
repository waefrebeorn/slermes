/* t_port_auxiliary_client_flow4.c — harness for the UNCLAIMED deep
 * async/client-builder remainder FLOW port (src/agent/auxiliary_client_flow4.c).
 *
 * stdin JSONL: {"func": <name>, "in": {...args..., "gate": {...}, "io": {seam: script}}}
 * stdout JSONL: {"func","in","out":{"ret","trace","state"}}.
 *   "ret"    canonical return (None -> null, raise -> {"__raise__":cls});
 *   "trace"  ordered seam events [{"ev","args","ret"}];
 *   "state"  post-call mutation window ({} for the stateless builders).
 * Input is echoed pristine; the working copy is mutated by the port.
 */
#include "hermes_json.h"
#include "agent/auxiliary_client_flow4.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *ac4_harness_run(ac4_ctx *ctx, const char *func, json_t *in,
        json_t **state_out) {
    if (strcmp(func, "_named_custom_openai_wire_client") == 0)
        return ac4_named_custom_openai_wire_client(ctx,
            json_get_str(in, "custom_base", ""), json_obj_get(in, "custom_key"),
            json_obj_get(in, "extra_headers"));
    if (strcmp(func, "_api_key_profile_supplied_client") == 0)
        return ac4_api_key_profile_supplied_client(ctx,
            json_get_str(in, "provider", ""), json_obj_get(in, "client_kwargs"));
    if (strcmp(func, "_build_vertex_client") == 0)
        return ac4_build_vertex_client(ctx, json_get_str(in, "provider", ""),
                                        json_obj_get(in, "model"));
    if (strcmp(func, "_build_bedrock_client") == 0)
        return ac4_build_bedrock_client(ctx, json_get_str(in, "provider", ""),
                                         json_obj_get(in, "model"),
                                         json_get_bool(in, "raw_codex", 0) != 0);
    if (strcmp(func, "_wrap_transport") == 0)
        return ac4_wrap_transport(ctx, json_obj_get(in, "req"),
            json_obj_get(in, "client"), json_get_str(in, "final_model", ""),
            json_get_str(in, "base_url", ""), json_get_str(in, "api_key", ""),
            state_out);
    if (strcmp(func, "_project_provider_profile") == 0)
        return ac4_project_provider_profile(ctx,
            json_get_str(in, "provider", ""), json_get_str(in, "provider_norm", ""),
            json_get_str(in, "model", ""), json_get_str(in, "base_url", ""),
            json_obj_get(in, "reasoning_config"), json_obj_get(in, "data_use"));
    if (strcmp(func, "_resolve_call_client") == 0) {
        json_t *tm = json_obj_get(in, "main_runtime");
        return ac4_resolve_call_client(ctx,
            json_obj_get(in, "task") && json_obj_get(in, "task")->type == JSON_NULL
                ? NULL : json_get_str(in, "task", NULL),
            json_obj_get(in, "provider"), json_obj_get(in, "model"),
            json_obj_get(in, "base_url"), json_obj_get(in, "api_key"),
            json_get_str(in, "resolved_provider", ""), json_obj_get(in, "resolved_model"),
            json_obj_get(in, "resolved_base_url"), json_obj_get(in, "resolved_api_key"),
            json_obj_get(in, "resolved_api_mode"), (tm && tm->type != JSON_NULL) ? tm : NULL,
            json_get_bool(in, "async_mode", 0) != 0);
    }
    if (strcmp(func, "_acreate_with_progress") == 0) {
        const json_t *cl = json_obj_get(in, "client");
        const char *marker = (cl && cl->type == JSON_OBJECT)
                             ? json_get_str((json_t *)cl, "id", "client") : "client";
        json_t *task = json_obj_get(in, "task");
        return ac4_acreate_with_progress(ctx, marker, json_obj_get(in, "kwargs"),
            (task && task->type != JSON_NULL) ? json_get_str(in, "task", "") : NULL,
            json_get_bool(in, "force_stream", 0) != 0,
            json_get_str(in, "provider", ""));
    }
    if (strcmp(func, "_try_discovery_chain") == 0)
        return ac4_try_discovery_chain(ctx);
    if (strcmp(func, "_translate_anthropic_response_format") == 0) {
        json_t *ak = json_obj_get(in, "anthropic_kwargs");
        ac4_translate_anthropic_response_format(ak, json_obj_get(in, "response_format"));
        if (state_out) {
            json_t *st = json_object();
            json_set(st, "anthropic_kwargs", ak ? json_copy(ak) : json_object());
            *state_out = st;
        }
        return NULL;
    }
    if (strcmp(func, "_pool_credential_digest") == 0)
        return ac4_pool_credential_digest(ctx, json_obj_get(in, "pool"),
                                           json_obj_get(in, "entry"));
    return NULL;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    static char line[1 << 17];
    while (fgets(line, sizeof(line), stdin)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '\r') continue;
        json_t *rec = json_parse(line, NULL);
        if (!rec) { fprintf(stderr, "harness: unparseable case line\n"); return 1; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in_orig = json_obj_get(rec, "in");
        if (!in_orig) in_orig = json_object();
        json_t *in = json_copy(in_orig);          /* working copy (mutated) */

        ac4_ctx ctx;
        ctx.gate = json_obj_get(in, "gate");
        ctx.io = json_obj_get(in, "io");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;

        json_t *state = NULL;
        json_t *ret = ac4_harness_run(&ctx, func, in, &state);
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_string(ac4_exc_class(&ctx)));
            json_free(ctx.exc);
            json_free(ret);
            state = NULL;               /* no state echo on a raise */
        } else {
            ret_out = ret ? ret : json_null();
        }

        json_t *out = json_object();
        json_set(out, "ret", ret_out);
        json_set(out, "trace", ctx.trace);
        json_set(out, "state", state ? state : json_object());
        json_t *emit = json_object();
        json_set(emit, "func", json_string(func));
        json_set(emit, "in", json_copy(in_orig));
        json_set(emit, "out", out);
        char *s = json_serialize(emit);
        if (s) { fputs(s, stdout); fputc('\n', stdout); free(s); }
        json_free(emit);
        json_free(ctx.counters);
        json_free(in);
        json_free(rec);
    }
    return 0;
}
