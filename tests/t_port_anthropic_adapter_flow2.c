/* t_port_anthropic_adapter_flow2.c — harness for the anthropic_adapter FLOW port
 * (src/agent/anthropic_adapter_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...args..., "io": {seam:
 * script}}}.  Emits JSON LINES: {"func","in","out":{"ret","trace","state"}}.
 * "ret" is the canonical return (tuple -> array, None -> null, uncaught
 * exception -> the full {"__raise__": cls, "msg": ...} marker); "trace" is the
 * ordered seam-call sequence; "state" is {"api_kwargs": <mutated>} for the
 * in-place mutator buffer_anthropic_tool_input and {} otherwise.  The oracle
 * drives the LIVE quarry defs under the SAME scripts and diffs out.
 * Self-contained: #includes the area-qualified port .c (libjson-only; the gate
 * links lib/libjson/json.o).
 */
#include "hermes_json.h"
#include "agent/anthropic_adapter_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *aa2_harness_run(aa2_ctx *ctx, const char *func, json_t *in, json_t **state_out) {
    *state_out = NULL;
    if (strcmp(func, "_model_matches") == 0)
        return aa2_model_matches(ctx, json_obj_get(in, "model"), json_obj_get(in, "substrings"));
    if (strcmp(func, "_beta_header") == 0)
        return aa2_beta_header(ctx, json_obj_get(in, "betas"));
    if (strcmp(func, "_normalize_to_mcp_wire") == 0)
        return aa2_normalize_to_mcp_wire(ctx, json_obj_get(in, "name"));
    if (strcmp(func, "_apply_oauth_prose_aliases") == 0) {
        const char *t = json_get_str(in, "text", "");
        json_t *r = aa2_apply_oauth_prose_aliases(ctx, json_string(t));
        return r;
    }
    if (strcmp(func, "_attribution_headers") == 0)
        return aa2_attribution_headers(ctx);
    if (strcmp(func, "_client_timeout") == 0)
        return aa2_client_timeout(ctx, json_obj_get(in, "timeout"));
    if (strcmp(func, "_auth_style") == 0)
        return aa2_auth_style(ctx, json_obj_get(in, "api_key"),
                              json_obj_get(in, "base_url"), json_obj_get(in, "normalized_base_url"));
    if (strcmp(func, "_thinking_kwargs") == 0)
        return aa2_thinking_kwargs(ctx, json_obj_get(in, "reasoning_config"),
                                   json_obj_get(in, "model"), json_obj_get(in, "effective_max_tokens"));
    if (strcmp(func, "_is_pre_thinking_haiku") == 0)
        return aa2_is_pre_thinking_haiku(ctx, json_obj_get(in, "model"));
    if (strcmp(func, "_base_client_kwargs") == 0)
        return aa2_base_client_kwargs(ctx, json_obj_get(in, "base_url"), json_obj_get(in, "timeout"));
    if (strcmp(func, "_custom_provider_extra_headers") == 0)
        return aa2_custom_provider_extra_headers(ctx, json_obj_get(in, "base_url"));
    if (strcmp(func, "buffer_anthropic_tool_input") == 0) {
        json_t *kw = json_copy(json_obj_get(in, "api_kwargs"));
        if (!kw) kw = json_object();
        *state_out = kw;
        aa2_buffer_anthropic_tool_input(ctx, kw, json_obj_get(in, "base_url"));
        return NULL;
    }
    return NULL;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    static char line[1 << 20];
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
        aa2_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.trace = json_array();
        ctx.idx = json_object();
        ctx.exc = NULL;
        json_t *state = NULL;
        json_t *ret = aa2_harness_run(&ctx, func, in, &state);
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = ctx.exc;          /* full marker, ownership transferred */
            ctx.exc = NULL;
            if (ret) json_free(ret);
        } else {
            ret_out = ret ? ret : json_null();
        }
        json_t *out = json_object();
        json_set(out, "ret", ret_out);
        json_set(out, "trace", ctx.trace);
        json_t *st = json_object();
        if (state) json_set(st, "api_kwargs", state);
        json_set(out, "state", st);
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
        json_free(ctx.idx);
        json_free(empty_in);
        json_free(rec);
    }
    return 0;
}
