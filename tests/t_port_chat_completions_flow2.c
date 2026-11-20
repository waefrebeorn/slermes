/* t_port_chat_completions_flow2.c — harness for the Chat Completions transport
 * UNCLAIMED-remainder FLOW port (wave 2;
 * src/agent/transports/chat_completions_flow2.c; live module
 * agent/transports/chat_completions.py).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...args..., "io": {seam:
 * script}}}. Emits JSON LINES: {"func","in","out":{"ret","trace","state"}}.
 * "ret" is the canonical return (tuples/sets -> arrays, None -> null, uncaught
 * exception -> {"__raise__": cls}); "trace" is the ordered seam-call sequence
 * [{"ev","args","ret"}]; "state" is the post-call mutation window
 * ({"api_kwargs":...} for _add_prompt_cache_key, {} otherwise) — echoed on RAISE
 * too (partial history). The oracle drives LIVE agent/transports/chat_completions.py
 * under the SAME scripts and diffs out. Dispatch runs on a deep COPY of "in" so
 * the emitted "in" stays pristine.
 *
 * Self-contained: #includes the area-qualified port .c (libjson-only).
 */
#include "hermes_json.h"
#include "agent/transports/chat_completions_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static json_t *cc3_state_win(const char *key, json_t *node) {
    json_t *o = json_object();
    json_set(o, key, node ? node : json_null());
    return o;
}

static json_t *cc3_harness_run(cc3_ctx *ctx, const char *func, json_t *in, json_t **state_out) {
    *state_out = NULL;
    if (strcmp(func, "api_mode") == 0)
        return cc3_api_mode();
    if (strcmp(func, "convert_tools") == 0)
        return cc3_convert_tools(json_obj_get(in, "tools"));
    if (strcmp(func, "convert_messages") == 0)
        return cc3_convert_messages(ctx, json_obj_get(in, "messages"), json_obj_get(in, "kwargs"));
    if (strcmp(func, "build_kwargs") == 0)
        return cc3_build_kwargs(ctx, json_obj_get(in, "model"), json_obj_get(in, "messages"),
                                json_obj_get(in, "tools"), json_obj_get(in, "params"));
    if (strcmp(func, "_build_kwargs_from_profile") == 0)
        return cc3_build_kwargs_from_profile(ctx, json_obj_get(in, "profile"), json_obj_get(in, "model"),
                                             json_obj_get(in, "sanitized"), json_obj_get(in, "tools"),
                                             json_obj_get(in, "params"));
    if (strcmp(func, "normalize_response") == 0)
        return cc3_normalize_response(ctx, json_obj_get(in, "response"));
    if (strcmp(func, "validate_response") == 0)
        return cc3_validate_response(ctx, json_obj_get(in, "response"));
    if (strcmp(func, "extract_cache_stats") == 0)
        return cc3_extract_cache_stats(json_obj_get(in, "response"));
    if (strcmp(func, "_static_prompt_instructions") == 0)
        return cc3_static_prompt_instructions(ctx, json_obj_get(in, "messages"));
    if (strcmp(func, "_add_prompt_cache_key") == 0) {
        json_t *kw = json_obj_get(in, "api_kwargs");
        cc3_add_prompt_cache_key(ctx, kw, json_obj_get(in, "messages"), json_obj_get(in, "tools"),
                                 json_obj_get(in, "supports_prompt_cache_key"),
                                 json_obj_get(in, "session_id"), json_obj_get(in, "cache_scope_id"));
        *state_out = cc3_state_win("api_kwargs", json_copy(kw));
        return NULL;
    }
    if (strcmp(func, "_build_gemini_thinking_config") == 0)
        return cc3_build_gemini_thinking_config(ctx, json_obj_get(in, "model"),
                                                json_obj_get(in, "reasoning_config"));
    if (strcmp(func, "_snake_case_gemini_thinking_config") == 0)
        return cc3_snake_case_gemini_thinking_config(json_obj_get(in, "config"));
    if (strcmp(func, "_raise_gemini_thinking_max_tokens") == 0)
        return cc3_raise_gemini_thinking_max_tokens(ctx, json_obj_get(in, "model"),
                                                    json_obj_get(in, "reasoning_config"),
                                                    json_obj_get(in, "requested"));
    if (strcmp(func, "_is_gemini_openai_compat_base_url") == 0)
        return cc3_is_gemini_openai_compat_base_url(json_obj_get(in, "base_url"));
    if (strcmp(func, "_is_openai_api_base_url") == 0)
        return cc3_is_openai_api_base_url(ctx, json_obj_get(in, "base_url"));
    if (strcmp(func, "_model_consumes_thought_signature") == 0)
        return cc3_model_consumes_thought_signature(ctx, json_obj_get(in, "model"),
                                                    json_obj_get(in, "base_url"),
                                                    json_obj_get(in, "provider"));
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
        cc3_ctx ctx;
        ctx.io = json_obj_get(work, "io");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        json_t *state = NULL;
        json_t *ret = cc3_harness_run(&ctx, func, work, &state);
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_copy(json_obj_get(ctx.exc, "__raise__")));
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