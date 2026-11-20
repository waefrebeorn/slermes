/* t_port_bedrock_transport_flow2.c — harness for the REMAINING-UNCLAIMED
 * Bedrock adapter FLOW port (src/agent/transports/bedrock_transport_flow2.c;
 * live defining module agent/bedrock_adapter.py — agent/transports/bedrock.py
 * is a thin delegation shell whose every method is already claimed, and
 * bedrock_transport_flow.c claimed 27 adapter defs; this harness drives the 12
 * still-unclaimed defs and the PoP lines credit the real defining module).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...args..., "io": {seam:
 * script}, "state": {module-state seed}}}. Emits JSON LINES:
 * {"func","in","out":{"ret","trace","state"}}. "ret" is the canonical return
 * (tuples -> arrays, None -> null, bytes -> {"__bytes__":[...]}, ported-class
 * instances -> dicts, uncaught exception -> {"__raise__": cls, "msg": str(e)});
 * "trace" is the ordered seam-call sequence [{"ev","args","ret"}]; "state" is
 * the post-call mutation window ({"cache","_bedrock_clients_by_home"} /
 * {"agent"} / {"block","parts"} / {"request"} for the mutating flows, else the
 * module-state keys PRESENT in in.state) — echoed on RAISE too (partial
 * history). The oracle drives LIVE agent/bedrock_adapter.py under the SAME
 * scripts and diffs out. Dispatch runs on a deep COPY of "in" so the emitted
 * "in" stays pristine.
 *
 * Self-contained: #includes the area-qualified port .c (libjson-only).
 */
#include "hermes_json.h"
#include "agent/transports/bedrock_transport_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* request projection {method,url,content,headers} (pristine echo on raise). */
static json_t *bt3_request_proj(const json_t *request) {
    json_t *o = json_object();
    json_t *m = bt3_get(request, "method");
    json_set(o, "method", m ? json_copy(m) : json_null());
    json_t *u = bt3_get(request, "url");
    json_set(o, "url", u ? json_copy(u) : json_null());
    json_t *c = bt3_get(request, "content");
    json_set(o, "content", c ? json_copy(c) : json_null());
    json_t *h = bt3_get(request, "headers");
    json_set(o, "headers", h ? json_copy(h) : json_null());
    return o;
}

/* Seed a _ResponseParts carrier with all three accumulator keys (the live
 * instance always has them; mk_parts overrides from the input, default []). */
static json_t *bt3_parts_seed(const json_t *in_parts) {
    json_t *p = json_object();
    static const char *keys[3] = {"text_parts", "reasoning_details", "tool_calls"};
    for (int i = 0; i < 3; i++) {
        json_t *v = bt3_get(in_parts, keys[i]);
        json_set(p, keys[i], (bt3_is_arr(v) && v->c.count > 0) ? json_copy(v) : json_array());
    }
    return p;
}

static json_t *bt3_harness_run(bt3_ctx *ctx, const char *func, json_t *in, json_t **state_out) {
    *state_out = NULL;
    if (strcmp(func, "_new_client") == 0)
        return bt3_new_client(ctx, json_obj_get(in, "factory"), json_obj_get(in, "service"),
                              json_obj_get(in, "region"), json_obj_get(in, "read_timeout"));
    if (strcmp(func, "_cached_client") == 0) {
        json_t *cache = json_obj_get(in, "cache");
        json_t *empty = NULL;
        if (!bt3_is_obj(cache)) { empty = json_object(); cache = empty; }
        json_t *r = bt3_cached_client(ctx, cache, json_obj_get(in, "service"),
                                      json_obj_get(in, "region"), json_obj_get(in, "read_timeout"));
        json_t *win = json_object();
        json_set(win, "cache", json_copy(cache));
        json_t *home = bt3_get(ctx->state, "_bedrock_clients_by_home");
        json_set(win, "_bedrock_clients_by_home", home ? json_copy(home) : json_object());
        *state_out = win;
        if (empty) json_free(empty);
        return r;
    }
    if (strcmp(func, "BedrockOpenAISigV4Auth.__init__") == 0) {
        json_t *service = json_obj_get(in, "service");
        json_t *def = NULL;
        if (!service) { def = json_string("bedrock"); service = def; }
        json_t *r = bt3_sigv4_init(ctx, json_obj_get(in, "region"), service);
        if (def) json_free(def);
        return r;
    }
    if (strcmp(func, "BedrockOpenAISigV4Auth.auth_flow") == 0) {
        json_t *request = json_obj_get(in, "request");
        json_t *service = json_obj_get(in, "service");
        json_t *def = NULL;
        if (!service) { def = json_string("bedrock"); service = def; }
        json_t *r = bt3_auth_flow(ctx, json_obj_get(in, "region"), service, request);
        if (def) json_free(def);
        json_t *win = json_object();
        if (bt3_is_arr(r) && r->c.count > 0)
            json_set(win, "request", json_copy(r->c.items[0]));
        else
            json_set(win, "request", bt3_request_proj(request));
        *state_out = win;
        return r;
    }
    if (strcmp(func, "build_bedrock_openai_http_client") == 0)
        return bt3_build_http_client(ctx, json_obj_get(in, "region"), json_obj_get(in, "timeout"));
    if (strcmp(func, "_stale_error_types") == 0)
        return bt3_stale_error_types(ctx);
    if (strcmp(func, "_boto3_chain_has_credentials") == 0) {
        json_t *im = json_obj_get(in, "instance_metadata");
        json_t *def = NULL;
        if (!im) { def = json_bool(true); im = def; }
        json_t *r = bt3_boto3_chain_has_credentials(ctx, im);
        if (def) json_free(def);
        return r;
    }
    if (strcmp(func, "bind_bedrock_runtime") == 0) {
        json_t *agent = json_obj_get(in, "agent");
        json_t *r = bt3_bind_bedrock_runtime(ctx, agent, json_obj_get(in, "base_url"),
                                             json_obj_get(in, "api_mode"));
        json_t *win = json_object();
        json_set(win, "agent", agent ? json_copy(agent) : json_null());
        *state_out = win;
        return r;
    }
    if (strcmp(func, "_ResponseParts.__init__") == 0)
        return bt3_parts_init(ctx);
    if (strcmp(func, "_ResponseParts.absorb_reasoning") == 0) {
        json_t *parts = bt3_parts_seed(json_obj_get(in, "parts"));
        json_t *block = json_obj_get(in, "block");
        json_t *eb = NULL;
        if (!bt3_is_obj(block)) { eb = json_object(); block = eb; }
        json_t *r = bt3_absorb_reasoning(ctx, parts, json_obj_get(in, "reasoning"), block,
                                         json_obj_get(in, "on_text"));
        json_t *win = json_object();
        json_set(win, "block", json_copy(block));
        json_set(win, "parts", json_copy(parts));
        *state_out = win;
        json_free(parts);
        if (eb) json_free(eb);
        return r;
    }
    if (strcmp(func, "_ResponseParts.build") == 0) {
        json_t *parts = bt3_parts_seed(json_obj_get(in, "parts"));
        json_t *usage = json_obj_get(in, "usage_data");
        json_t *eu = NULL;
        if (!bt3_is_obj(usage)) { eu = json_object(); usage = eu; }
        json_t *r = bt3_parts_build(ctx, parts, json_obj_get(in, "ordered_blocks"), usage,
                                    json_obj_get(in, "stop_reason"), json_obj_get(in, "model"));
        json_free(parts);
        if (eu) json_free(eu);
        return r;
    }
    if (strcmp(func, "bedrock_static_context_length") == 0)
        return bt3_static_context_length(ctx, json_obj_get(in, "model_id"));
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
        json_t *in_state = json_obj_get(work, "state");
        bool had_prof = in_state && json_obj_get(in_state, "_inference_profile_model_cache");
        bool had_home = in_state && json_obj_get(in_state, "_bedrock_clients_by_home");
        bt3_ctx ctx;
        ctx.io = json_obj_get(work, "io");
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        ctx.state = in_state ? json_copy(in_state) : json_object();
        if (!bt3_get(ctx.state, "_inference_profile_model_cache"))
            json_set(ctx.state, "_inference_profile_model_cache", json_object());
        if (!bt3_get(ctx.state, "_bedrock_clients_by_home"))
            json_set(ctx.state, "_bedrock_clients_by_home", json_object());
        json_t *state = NULL;
        json_t *ret = bt3_harness_run(&ctx, func, work, &state);
        if (!state) {
            state = json_object();
            if (had_prof)
                json_set(state, "_inference_profile_model_cache",
                         json_copy(bt3_get(ctx.state, "_inference_profile_model_cache")));
            if (had_home)
                json_set(state, "_bedrock_clients_by_home",
                         json_copy(bt3_get(ctx.state, "_bedrock_clients_by_home")));
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