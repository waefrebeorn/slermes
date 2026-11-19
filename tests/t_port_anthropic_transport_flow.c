/* t_port_anthropic_transport_flow.c — harness for the anthropic Messages-API
 * transport FLOW + leaf port (src/agent/transports/anthropic_transport_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...args..., "io": {seam:
 * script}}}. Emits JSON LINES: {"func","in","out":{"ret","trace","state"}}.
 * "ret" is the canonical return (tuple/set/generator -> array, None -> null,
 * uncaught exception -> the full {"__raise__": cls, "msg": ...} marker); "trace"
 * is the ordered seam-call sequence [{"ev","args","ret"}]; "state" is {} (this
 * function set mutates no caller-held state — all mutation is on local/returned
 * structures). The oracle drives LIVE quarry defs under the SAME scripts and
 * diffs out. Self-contained: #includes the area-qualified port .c (libjson-only).
 */
#include "hermes_json.h"
#include "agent/transports/anthropic_transport_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* owned copy of a borrowed leaf result (NULL -> null) */
static json_t *ant_own(const json_t *v) { return v ? json_copy(v) : json_null(); }

static json_t *ant_harness_run(ant_ctx *ctx, const char *func, json_t *in) {
    if (strcmp(func, "_block_type") == 0)
        return ant_own(ant_block_type(ant_get(in, "b")));
    if (strcmp(func, "_has_block_type") == 0) {
        bool r = ant_has_block_type(ctx, ant_get(in, "blocks"), ant_get(in, "types"));
        return ant_exc_pending(ctx) ? NULL : json_bool(r);
    }
    if (strcmp(func, "_is_blank_text_block") == 0)
        return json_bool(ant_is_blank_text_block(ant_get(in, "b")));
    if (strcmp(func, "_cache_control_of") == 0)
        return ant_own(ant_cache_control_of(ant_get(in, "b")));
    if (strcmp(func, "_text_block") == 0)
        return ant_text_block(ant_get(in, "text"));
    if (strcmp(func, "_text_block_with_citations") == 0)
        return ant_text_block_with_citations(ant_get(in, "text"), ant_get(in, "cits"));
    if (strcmp(func, "_parse_tool_args") == 0)
        return ant_parse_tool_args(ant_get(in, "raw"));
    if (strcmp(func, "_strip_thinking") == 0)
        return ant_strip_thinking(ctx, ant_get(in, "blocks"));
    if (strcmp(func, "_block_ids") == 0)
        return ant_block_ids(ctx, ant_get(in, "blocks"), ant_get(in, "btype"),
                             json_get_str(in, "key", "id"));
    if (strcmp(func, "_assistant_block_lists") == 0)
        return ant_assistant_block_lists(ctx, ant_get(in, "result"));
    if (strcmp(func, "_tool_use_block") == 0)
        return ant_tool_use_block(ctx, ant_get(in, "tool_id"), ant_get(in, "name"),
                                  ant_get(in, "tool_input"));
    if (strcmp(func, "_replay_ordered_blocks") == 0)
        return ant_replay_ordered_blocks(ctx, ant_get(in, "m"), ant_get(in, "ordered_blocks"));
    if (strcmp(func, "assistant_replay_carrier") == 0)
        return ant_assistant_replay_carrier(ctx, ant_get(in, "m"));
    if (strcmp(func, "_tool_result_content") == 0)
        return ant_tool_result_content(ctx, ant_get(in, "m"));
    if (strcmp(func, "_concat_content") == 0)
        return ant_concat_content(ctx, ant_get(in, "prev"), ant_get(in, "curr"));
    if (strcmp(func, "_keep_valid_thinking") == 0)
        return ant_keep_valid_thinking(ctx, ant_get(in, "content"),
                                       json_get_bool(in, "signature_dead", false));
    if (strcmp(func, "_current_tool_loop_start") == 0)
        return ant_current_tool_loop_start(ctx, ant_get(in, "result"));
    if (strcmp(func, "_image_source_data") == 0)
        return ant_own(ant_image_source_data(ant_get(in, "block")));
    if (strcmp(func, "_convert_system_content") == 0)
        return ant_convert_system_content(ctx, ant_get(in, "content"));
    if (strcmp(func, "_unprefix_oauth_tool_name") == 0)
        return ant_unprefix_oauth_tool_name(ctx, ant_get(in, "name"));
    if (strcmp(func, "response_finish_reason") == 0)
        return ant_response_finish_reason(ctx, ant_get(in, "response"));
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
        ant_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *ret = ant_harness_run(&ctx, func, in);
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