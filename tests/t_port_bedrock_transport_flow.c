/* t_port_bedrock_transport_flow.c — harness for the Bedrock Converse transport
 * UNCLAIMED-remainder FLOW port
 * (src/agent/transports/bedrock_transport_flow.c; live defining module
 * agent/bedrock_adapter.py — agent/transports/bedrock.py exists but is a thin
 * delegation shell whose every method is already claimed by
 * transports_bedrock_ports.c, so the UNCLAIMED data-path functions are ported
 * from the module it delegates to and the PoP lines credit that module).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...args..., "io": {seam:
 * script}, "state": {module-state seed}}}. Emits JSON LINES:
 * {"func","in","out":{"ret","trace","state"}}. "ret" is the canonical return
 * (tuples -> arrays, None -> null, bytes -> {"__bytes__":[...]}, uncaught
 * exception -> {"__raise__": cls, "msg": str(e)}); "trace" is the ordered
 * seam-call sequence [{"ev","args","ret"}]; "state" is the post-call mutation
 * window (module-state keys PRESENT in in.state, rejection sets sorted; or
 * {"models":...} / {"kwargs":...,"inference_config":...} for the mutating
 * flows) — echoed on RAISE too (partial history). The oracle drives LIVE
 * agent/bedrock_adapter.py under the SAME scripts and diffs out. Dispatch runs
 * on a deep COPY of "in" so the emitted "in" stays pristine.
 *
 * Self-contained: #includes the area-qualified port .c (libjson-only).
 */
#include "hermes_json.h"
#include "agent/transports/bedrock_transport_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* insertion-sort a string array in place (rejection-set echo normalization) */
static void bt2_sort_str_arr(json_t *arr) {
    if (!bt2_is_arr(arr)) return;
    for (size_t i = 1; i < arr->c.count; i++) {
        json_t *cur = arr->c.items[i];
        size_t j = i;
        while (j > 0 && strcmp(bt2_jstr(arr->c.items[j - 1]), bt2_jstr(cur)) > 0) {
            arr->c.items[j] = arr->c.items[j - 1];
            j--;
        }
        arr->c.items[j] = cur;
    }
}

static json_t *bt2_harness_run(bt2_ctx *ctx, const char *func, json_t *in, json_t **state_out) {
    *state_out = NULL;
    if (strcmp(func, "bedrock_openai_uses_mantle") == 0)
        return bt2_bedrock_openai_uses_mantle(ctx, json_obj_get(in, "model_id"));
    if (strcmp(func, "_tool_use_block") == 0)
        return bt2_tool_use_block(json_obj_get(in, "tool_use_id"), json_obj_get(in, "name"),
                                  json_obj_get(in, "input_dict"));
    if (strcmp(func, "_reasoning_text_block") == 0)
        return bt2_reasoning_text_block(json_obj_get(in, "text"), json_obj_get(in, "signature"));
    if (strcmp(func, "_redacted_reasoning_block") == 0)
        return bt2_redacted_reasoning_block(ctx, json_obj_get(in, "encoded"));
    if (strcmp(func, "_anthropic_ordered_blocks") == 0)
        return bt2_anthropic_ordered_blocks(ctx, json_obj_get(in, "msg"));
    if (strcmp(func, "_issuer") == 0)
        return bt2_issuer(ctx, json_obj_get(in, "msg"));
    if (strcmp(func, "_minted_outside_claude") == 0)
        return bt2_minted_outside_claude(ctx, json_obj_get(in, "issuer"));
    if (strcmp(func, "reasoning_minted_outside_claude") == 0)
        return bt2_reasoning_minted_outside_claude(ctx, json_obj_get(in, "msg"));
    if (strcmp(func, "_unsigned_reasoning") == 0)
        return bt2_unsigned_reasoning(ctx, json_obj_get(in, "block"));
    if (strcmp(func, "_verbatim") == 0)
        return bt2_verbatim(json_obj_get(in, "block"));
    if (strcmp(func, "_without_reasoning") == 0)
        return bt2_without_reasoning(ctx, json_obj_get(in, "block"));
    if (strcmp(func, "_filter_reasoning") == 0)
        return bt2_filter_reasoning(ctx, json_obj_get(in, "blocks"), json_obj_get(in, "keep"));
    if (strcmp(func, "_without_cache_points") == 0)
        return bt2_without_cache_points(json_obj_get(in, "blocks"));
    if (strcmp(func, "_without_redacted_reasoning") == 0)
        return bt2_without_redacted_reasoning(json_obj_get(in, "blocks"));
    if (strcmp(func, "note_cache_point_rejection") == 0)
        return bt2_note_cache_point_rejection(ctx, json_obj_get(in, "model_id"),
                                              json_obj_get(in, "placement"));
    if (strcmp(func, "reset_cache_point_rejections") == 0)
        return bt2_reset_cache_point_rejections(ctx);
    if (strcmp(func, "recover_from_redacted_reasoning_rejection") == 0) {
        json_t *exc = json_obj_get(in, "exc");
        const char *msg = bt2_is_str(exc) ? bt2_jstr(exc) : "None";
        return bt2_recover_from_redacted_reasoning_rejection(ctx, msg, json_obj_get(in, "kwargs"));
    }
    if (strcmp(func, "_encode_redacted") == 0)
        return bt2_encode_redacted(json_obj_get(in, "redacted"));
    if (strcmp(func, "_tool_call_ns") == 0)
        return bt2_tool_call_ns(json_obj_get(in, "tool_use_id"), json_obj_get(in, "name"),
                                json_obj_get(in, "input_dict"));
    if (strcmp(func, "_persisted_reasoning_details") == 0)
        return bt2_persisted_reasoning_details(ctx, json_obj_get(in, "ordered_blocks"));
    if (strcmp(func, "_model_entry") == 0)
        return bt2_model_entry(ctx, json_obj_get(in, "model_id"), json_obj_get(in, "name"),
                               json_obj_get(in, "provider"), json_obj_get(in, "input_mods"),
                               json_obj_get(in, "output_mods"));
    if (strcmp(func, "_nova_converse_reasoning") == 0)
        return bt2_nova_converse_reasoning(ctx, json_obj_get(in, "model"),
                                           json_obj_get(in, "reasoning_config"),
                                           json_obj_get(in, "max_tokens"));
    if (strcmp(func, "_claude_converse_reasoning") == 0)
        return bt2_claude_converse_reasoning(ctx, json_obj_get(in, "model"),
                                             json_obj_get(in, "reasoning_config"),
                                             json_obj_get(in, "max_tokens"));
    if (strcmp(func, "_apply_converse_reasoning") == 0) {
        json_t *r = bt2_apply_converse_reasoning(ctx, json_obj_get(in, "kwargs"),
                                                 json_obj_get(in, "inference_config"),
                                                 json_obj_get(in, "model"),
                                                 json_obj_get(in, "reasoning_config"),
                                                 json_obj_get(in, "max_tokens"));
        json_t *win = json_object();
        json_t *kw = json_obj_get(in, "kwargs");
        json_t *ic = json_obj_get(in, "inference_config");
        json_set(win, "kwargs", kw ? json_copy(kw) : json_null());
        json_set(win, "inference_config", ic ? json_copy(ic) : json_null());
        *state_out = win;
        return r;
    }
    if (strcmp(func, "_resolve_inference_profile_model_id") == 0) {
        json_t *region = json_obj_get(in, "region");
        json_t *empty = NULL;
        if (!region) { empty = json_string(""); region = empty; }
        json_t *r = bt2_resolve_inference_profile_model_id(ctx, json_obj_get(in, "profile_arn"), region);
        if (empty) json_free(empty);
        return r;
    }
    if (strcmp(func, "_list_foundation_models") == 0) {
        json_t *client = json_obj_get(in, "client");
        json_t *models = json_obj_get(in, "models");
        json_t *r;
        if (bt2_is_none(client)) {
            /* None.list_foundation_models — AttributeError BEFORE the seam */
            bt2_raise_attr(ctx, NULL, "list_foundation_models");
            r = NULL;
        } else {
            r = bt2_list_foundation_models(ctx, client, json_obj_get(in, "filter_set"), models);
        }
        json_t *win = json_object();
        json_set(win, "models", models ? json_copy(models) : json_null());
        *state_out = win;
        return r;
    }
    if (strcmp(func, "_list_inference_profiles") == 0) {
        json_t *client = json_obj_get(in, "client");
        json_t *models = json_obj_get(in, "models");
        json_t *r;
        if (bt2_is_none(client)) {
            bt2_raise_attr(ctx, NULL, "list_inference_profiles");
            r = NULL;
        } else {
            r = bt2_list_inference_profiles(ctx, client, json_obj_get(in, "filter_set"), models);
        }
        json_t *win = json_object();
        json_set(win, "models", models ? json_copy(models) : json_null());
        *state_out = win;
        return r;
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
        json_t *work = json_copy(in);   /* dispatch mutates the copy, not "in" */
        /* module-state carrier: seed from in.state, ensure both keys exist */
        json_t *in_state = json_obj_get(work, "state");
        bool had_rej = in_state && json_obj_get(in_state, "_CACHE_POINT_REJECTIONS");
        bool had_prof = in_state && json_obj_get(in_state, "_inference_profile_model_cache");
        bt2_ctx ctx;
        ctx.io = json_obj_get(work, "io");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        ctx.state = in_state ? json_copy(in_state) : json_object();
        if (!bt2_get(ctx.state, "_CACHE_POINT_REJECTIONS"))
            json_set(ctx.state, "_CACHE_POINT_REJECTIONS", json_object());
        if (!bt2_get(ctx.state, "_inference_profile_model_cache"))
            json_set(ctx.state, "_inference_profile_model_cache", json_object());
        json_t *state = NULL;
        json_t *ret = bt2_harness_run(&ctx, func, work, &state);
        if (!state) {
            /* module-state echo: only the keys PRESENT in the fixture state */
            state = json_object();
            if (had_rej) {
                json_t *rej = json_copy(bt2_get(ctx.state, "_CACHE_POINT_REJECTIONS"));
                if (bt2_is_obj(rej))
                    for (size_t i = 0; i < rej->c.count; i++) bt2_sort_str_arr(rej->c.items[i]);
                json_set(state, "_CACHE_POINT_REJECTIONS", rej);
            }
            if (had_prof)
                json_set(state, "_inference_profile_model_cache",
                         json_copy(bt2_get(ctx.state, "_inference_profile_model_cache")));
        }
        json_t *ret_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_copy(json_obj_get(ctx.exc, "__raise__")));
            json_set(ret_out, "msg", json_string(json_get_str(ctx.exc, "msg", "")));
            json_free(ctx.exc);
            if (ret) json_free(ret);
        } else {
            ret_out = ret ? ret : json_null();
        }
        json_t *out = json_object();
        json_set(out, "ret", ret_out);
        json_set(out, "trace", ctx.trace);
        json_set(out, "state", state);
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
        json_free(ctx.state);
        json_free(work);
        json_free(empty_in);
        json_free(rec);
    }
    return 0;
}