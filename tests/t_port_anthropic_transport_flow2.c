/* t_port_anthropic_transport_flow2.c — harness for the anthropic Messages-API
 * transport FLOW port (src/agent/transports/anthropic_transport_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...args..., "io": {seam:
 * script}}}. Emits JSON LINES: {"func","in","out":{"ret","trace","state"}}.
 * "ret" is the canonical return (None -> null; uncaught exception -> the full
 * {"__raise__": cls, "msg": ...} marker); "trace" is the ordered seam-call
 * sequence [{"ev","args","ret"}]; "state" is {} (no caller-held state is
 * mutated). The oracle drives LIVE quarry defs under the SAME scripts and
 * diffs out. Self-contained: #includes the area-qualified port .c (libjson-only).
 */
#include "hermes_json.h"
#include "agent/transports/anthropic_transport_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *at2_harness_run(at2_ctx *ctx, const char *func, json_t *in) {
    if (strcmp(func, "_unprefix_oauth_tool_name") == 0)
        return at2_unprefix_oauth_tool_name(ctx, at2_get(in, "name"));
    if (strcmp(func, "api_mode") == 0)
        return at2_api_mode(ctx);
    if (strcmp(func, "convert_messages") == 0)
        return at2_convert_messages(ctx, at2_get(in, "messages"), at2_get(in, "kwargs"));
    if (strcmp(func, "convert_tools") == 0)
        return at2_convert_tools(ctx, at2_get(in, "tools"));
    if (strcmp(func, "build_kwargs") == 0)
        return at2_build_kwargs(ctx, at2_get(in, "model"), at2_get(in, "messages"),
                                at2_get(in, "tools"), at2_get(in, "params"));
    if (strcmp(func, "normalize_response") == 0)
        return at2_normalize_response(ctx, at2_get(in, "response"), at2_get(in, "kwargs"));
    if (strcmp(func, "response_finish_reason") == 0)
        return at2_response_finish_reason(ctx, at2_get(in, "response"));
    if (strcmp(func, "validate_response") == 0)
        return at2_validate_response(ctx, at2_get(in, "response"));
    if (strcmp(func, "extract_cache_stats") == 0)
        return at2_extract_cache_stats(ctx, at2_get(in, "response"));
    if (strcmp(func, "map_finish_reason") == 0)
        return at2_map_finish_reason(ctx, at2_get(in, "raw_reason"));
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
        at2_ctx ctx;
        ctx.io = at2_get(in, "io");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *ret = at2_harness_run(&ctx, func, in);
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
