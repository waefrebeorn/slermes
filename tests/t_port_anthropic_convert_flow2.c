/* t_port_anthropic_convert_flow2.c — harness for the anthropic message-conversion
 * FLOW port (src/agent/anthropic_convert_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...args..., "io": {seam:
 * script}, "env": {generator-only sibling-world scripts}}}. Emits JSON LINES:
 * {"func","in","out":{"ret","trace","state"}}. "ret" is the canonical return
 * (tuple/set -> array, None -> null, uncaught exception -> the full
 * {"__raise__": cls, "msg": ...} marker); "trace" is the ordered seam-call
 * sequence [{"ev","args","ret"}]; "state" is {"result": <mutated list>} for the
 * seven in-place mutators (echoed EVEN ON A RAISE — partial mutation history)
 * and {} otherwise. The oracle drives LIVE quarry defs under the SAME scripts
 * and diffs out. Self-contained: #includes the area-qualified port .c
 * (libjson-only; the gate links lib/libjson/json.o).
 */
#include "hermes_json.h"
#include "agent/anthropic_convert_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* owned copy of a borrowed leaf result (NULL -> null) */
static json_t *acv2_own(const json_t *v) { return v ? json_copy(v) : json_null(); }

static json_t *acv2_harness_run(acv2_ctx *ctx, const char *func, json_t *in, json_t **state_out) {
    *state_out = NULL;
    if (strcmp(func, "_extract_preserved_thinking_blocks") == 0)
        return acv2_extract_preserved_thinking_blocks(ctx, acv2_get(in, "message"));
    if (strcmp(func, "convert_tools_to_anthropic") == 0)
        return acv2_convert_tools_to_anthropic(ctx, acv2_get(in, "tools"));
    if (strcmp(func, "_convert_content_to_anthropic") == 0)
        return acv2_convert_content_to_anthropic(ctx, acv2_get(in, "content"));
    if (strcmp(func, "_content_parts_to_anthropic_blocks") == 0)
        return acv2_content_parts_to_anthropic_blocks(ctx, acv2_get(in, "parts"));
    if (strcmp(func, "_convert_assistant_message") == 0)
        return acv2_convert_assistant_message(ctx, acv2_get(in, "m"));
    if (strcmp(func, "_convert_user_message") == 0)
        return acv2_convert_user_message(ctx, acv2_get(in, "content"));
    if (strcmp(func, "_fix_blank_text_blocks_in_list") == 0) {
        json_t *ph = json_string(json_get_str(in, "placeholder_text", "(empty)"));
        json_t *r = acv2_fix_blank_text_blocks_in_list(
            ctx, acv2_get(in, "blocks"), ph->str_val,
            json_get_num(in, "msg_index", 0), acv2_get(in, "role"),
            json_get_str(in, "location", "content"));
        json_free(ph);
        return r;
    }
    if (strcmp(func, "convert_messages_to_anthropic") == 0)
        return acv2_convert_messages_to_anthropic(ctx, acv2_get(in, "messages"),
                                                  acv2_get(in, "base_url"), acv2_get(in, "model"));
    /* stateful mutators: the state window is a deep copy of in.result */
    json_t *result = json_copy(acv2_get(in, "result"));
    if (!result) result = json_null();
    *state_out = result;
    if (strcmp(func, "_convert_tool_message_to_result") == 0) {
        acv2_convert_tool_message_to_result(ctx, result, acv2_get(in, "m"));
        return NULL;
    }
    if (strcmp(func, "_strip_orphaned_tool_blocks") == 0) {
        acv2_strip_orphaned_tool_blocks(ctx, result);
        return NULL;
    }
    if (strcmp(func, "_merge_consecutive_roles") == 0)
        return acv2_merge_consecutive_roles(ctx, result);
    if (strcmp(func, "_manage_thinking_signatures") == 0) {
        acv2_manage_thinking_signatures(ctx, result, acv2_get(in, "base_url"), acv2_get(in, "model"));
        return NULL;
    }
    if (strcmp(func, "_evict_old_screenshots") == 0) {
        acv2_evict_old_screenshots(ctx, result);
        return NULL;
    }
    if (strcmp(func, "_ensure_leading_user_turn") == 0) {
        acv2_ensure_leading_user_turn(ctx, result);
        return NULL;
    }
    if (strcmp(func, "_scrub_blank_text_blocks") == 0) {
        acv2_scrub_blank_text_blocks(ctx, result);
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
        acv2_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *state = NULL;
        json_t *ret = acv2_harness_run(&ctx, func, in, &state);
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
        if (state) json_set(st, "result", state);
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
        json_free(ctx.counters);
        json_free(empty_in);
        json_free(rec);
    }
    return 0;
}