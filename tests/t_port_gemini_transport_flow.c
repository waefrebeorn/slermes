/* t_port_gemini_transport_flow.c — harness for the Gemini native transport
 * UNCLAIMED-remainder FLOW port
 * (src/agent/transports/gemini_transport_flow.c; live module
 * agent/gemini_native_adapter.py — the task's agent/transports/gemini.py path
 * does not exist in this quarry snapshot).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...args..., "io": {seam:
 * script}}}. Emits JSON LINES: {"func","in","out":{"ret","trace","state"}}.
 * "ret" is the canonical return (tuples -> arrays, None -> null, uncaught
 * exception -> {"__raise__": cls, "msg": str(e)} plus code/status_code/
 * retry_after/details for the real GeminiAPIError raise); "trace" is the
 * ordered seam-call sequence [{"ev","args","ret"}]; "state" is the post-call
 * mutation window ({"contents":...} for _merge_alternating, {} otherwise) —
 * echoed on RAISE too (partial history). The oracle drives LIVE
 * agent/gemini_native_adapter.py under the SAME scripts and diffs out.
 * Dispatch runs on a deep COPY of "in" so the emitted "in" stays pristine.
 *
 * Self-contained: #includes the area-qualified port .c (libjson-only).
 */
#include "hermes_json.h"
#include "agent/transports/gemini_transport_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *gem_state_win(const char *key, json_t *node) {
    json_t *o = json_object();
    json_set(o, key, node ? node : json_null());
    return o;
}

/* str-or-NULL reader (null input stays NULL, not ""). */
static const char *gem_in_str(const json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    return gem_is_str(v) ? gem_jstr(v) : NULL;
}

static json_t *gem_harness_run(gem_ctx *ctx, const char *func, json_t *in, json_t **state_out) {
    *state_out = NULL;
    if (strcmp(func, "_api_client") == 0)
        return gem_api_client(ctx);
    if (strcmp(func, "_response_text") == 0)
        return gem_response_text(ctx, json_obj_get(in, "response"));
    if (strcmp(func, "wrong_gemini_surface_guidance") == 0)
        return gem_wrong_gemini_surface_guidance(ctx, json_obj_get(in, "base_url"),
                                                 json_obj_get(in, "api_key"),
                                                 json_obj_get(in, "err_status"));
    if (strcmp(func, "_text_of") == 0)
        return gem_text_of(json_obj_get(in, "item"));
    if (strcmp(func, "_inline_data_part") == 0)
        return gem_inline_data_part(ctx, json_obj_get(in, "item"));
    if (strcmp(func, "_multimodal_part") == 0)
        return gem_multimodal_part(ctx, json_obj_get(in, "item"));
    if (strcmp(func, "_tool_call_id") == 0)
        return gem_tool_call_id(ctx, json_obj_get(in, "tool_call"));
    if (strcmp(func, "_looks_like_json_schema") == 0)
        return json_bool(gem_looks_like_json_schema(json_obj_get(in, "node")));
    if (strcmp(func, "_has_function_response") == 0)
        return gem_has_function_response(ctx, json_obj_get(in, "content"));
    if (strcmp(func, "_merge_alternating") == 0) {
        json_t *r = gem_merge_alternating(ctx, json_obj_get(in, "contents"));
        *state_out = gem_state_win("contents", json_copy(json_obj_get(in, "contents")));
        return r;
    }
    if (strcmp(func, "_function_declaration") == 0)
        return gem_function_declaration(ctx, json_obj_get(in, "tool"),
                                        gem_truthy(json_obj_get(in, "json_schema")));
    if (strcmp(func, "_long_decode_cap") == 0)
        return gem_long_decode_cap(ctx, json_obj_get(in, "max_tokens"),
                                   json_obj_get(in, "extra_body"));
    if (strcmp(func, "_translate_response_format") == 0)
        return gem_translate_response_format(ctx, json_obj_get(in, "response_format"),
                                             gem_truthy(json_obj_get(in, "json_schema")));
    if (strcmp(func, "_provider_call_id") == 0)
        return gem_provider_call_id(ctx, json_obj_get(in, "fc"));
    if (strcmp(func, "_new_call_id") == 0)
        return gem_new_call_id(ctx, json_obj_get(in, "fc"));
    if (strcmp(func, "_dump_call_args") == 0)
        return gem_dump_call_args(ctx, json_obj_get(in, "fc"), json_obj_get(in, "kwargs"));
    if (strcmp(func, "_usage_from_metadata") == 0)
        return gem_usage_from_metadata(ctx, json_obj_get(in, "usage_meta"));
    if (strcmp(func, "_envelope") == 0)
        return gem_envelope(ctx, json_get_str(in, "model", ""), json_get_str(in, "object_", ""),
                            json_obj_get(in, "choice"), json_obj_get(in, "usage"));
    if (strcmp(func, "_tool_call_ns") == 0)
        return gem_tool_call_ns(json_get_str(in, "name", ""), json_get_str(in, "arguments", ""),
                                (int)json_get_num(in, "index", 0), json_get_str(in, "call_id", ""),
                                json_obj_get(in, "extra_content"));
    if (strcmp(func, "_part_text") == 0)
        return gem_part_text(ctx, json_obj_get(in, "part"));
    if (strcmp(func, "_text_part_signature") == 0)
        return gem_text_part_signature(ctx, json_obj_get(in, "part"));
    if (strcmp(func, "_part_function_call") == 0)
        return gem_part_function_call(ctx, json_obj_get(in, "part"));
    if (strcmp(func, "raise_on_prompt_block") == 0)
        return gem_raise_on_prompt_block(ctx, json_obj_get(in, "payload"));
    if (strcmp(func, "_parse_sse_line") == 0)
        return gem_parse_sse_line(ctx, gem_in_str(in, "line"));
    if (strcmp(func, "_tool_call_slot") == 0)
        return gem_tool_call_slot(ctx, json_obj_get(in, "fc"), json_obj_get(in, "part"),
                                  (int)json_get_num(in, "part_index", 0),
                                  json_get_str(in, "args_str", ""),
                                  json_obj_get(in, "tool_call_indices"));
    if (strcmp(func, "_error_info") == 0)
        return gem_error_info(ctx, json_obj_get(in, "err_obj"));
    if (strcmp(func, "_error_object") == 0)
        return gem_error_object(gem_in_str(in, "body_text"));
    if (strcmp(func, "GeminiNativeClient._auth_headers") == 0)
        return gem_auth_headers(ctx, json_obj_get(in, "self"));
    if (strcmp(func, "GeminiNativeClient._http_error") == 0)
        return gem_http_error_method(ctx, json_obj_get(in, "self"), json_obj_get(in, "response"),
                                     json_obj_get(in, "body_text"));
    if (strcmp(func, "GeminiNativeClient.effective_output_cap") == 0)
        return gem_effective_output_cap(ctx, json_obj_get(in, "max_tokens"),
                                        json_obj_get(in, "extra_body"));
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
        json_t *work = json_copy(in);   /* dispatch mutates the copy, not "in" */
        gem_ctx ctx;
        ctx.io = json_obj_get(work, "io");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *state = NULL;
        json_t *ret = gem_harness_run(&ctx, func, work, &state);
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_copy(json_obj_get(ctx.exc, "__raise__")));
            json_set(ret_out, "msg", json_string(json_get_str(ctx.exc, "msg", "")));
            if (json_has(ctx.exc, "code")) {   /* the real GeminiAPIError raise */
                json_t *sc = json_obj_get(ctx.exc, "status_code");
                json_t *ra = json_obj_get(ctx.exc, "retry_after");
                json_t *de = json_obj_get(ctx.exc, "details");
                json_set(ret_out, "code", json_copy(json_obj_get(ctx.exc, "code")));
                json_set(ret_out, "status_code", sc ? json_copy(sc) : json_null());
                json_set(ret_out, "retry_after", ra ? json_copy(ra) : json_null());
                json_set(ret_out, "details", de ? json_copy(de) : json_null());
            }
            json_free(ctx.exc);
            if (ret) json_free(ret);
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
        json_free(work);
        json_free(empty_in);
        json_free(rec);
    }
    return 0;
}