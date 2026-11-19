/*
 * t_port_openai_routes_flow.c — oracle harness for the
 * gateway/platforms/api_server_openai_routes.py route FLOW cores + leaves
 * ported in src/gateway/platforms/openai_routes_flow.c.
 *
 * Self-contained: includes the port .c by bare name. Reads JSON-line cases
 * from stdin (tests/fixtures/openai_routes_flow.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 * A scripted JSON null returns a NULL pointer (None), never a json_null node;
 * {"__raise__": cls, "msg": ...} scripts an exception.
 * tests/oracle_openai_routes_flow.py drives the LIVE Python with the same
 * scripts and compares ordered trace + ret.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "openai_routes_flow.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *counters;
} script_ctx_t;

static json_t *args_drop_null(const json_t *args) {
    json_t *out = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (!v || v->type == JSON_NULL) continue;
            json_set(out, args->c.keys[i], json_copy(v));
        }
    }
    return out;
}

static json_t *script_call(void *vctx, const char *name, const json_t *args) {
    script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (sc && sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx;
        size_t n = json_len(sc);
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else if (sc) {
        result = json_copy(sc);
    }
    /* Contract: NULL == None. A scripted null must NOT surface as a non-NULL
     * json_null node (truthiness / is-not-None guards would read it wrong). */
    if (result && result->type == JSON_NULL) {
        json_free(result);
        result = NULL;
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    json_append(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)
#define N(k, d) json_get_num(in, k, d)
#define B(k, d) json_get_bool(in, k, d)

static void ret_json(json_t *out, json_t *r) {
    json_set(out, "ret", r ? r : json_null());
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    json_t *trace = json_array();
    json_t *counters = json_object();
    script_ctx_t ctx = {in, trace, counters};
    orf_seams_t seams = {&ctx, script_call};
    const orf_seams_t *sp = &seams;

    if (!strcmp(func, "result_flags")) {
        ret_json(out, orf_result_flags(J("result")));
    } else if (!strcmp(func, "budget_exhausted")) {
        ret_json(out, orf_budget_exhausted(J("result")));
    } else if (!strcmp(func, "finish_reason")) {
        ret_json(out, orf_finish_reason(J("completed"), J("is_partial"), J("is_failed"),
                                        J("err_msg"), J("agent_error"), B("budget_exhausted", false)));
    } else if (!strcmp(func, "settled")) {
        ret_json(out, orf_settled(J("record")));
    } else if (!strcmp(func, "responses_fingerprint_keys")) {
        ret_json(out, orf_responses_fingerprint_keys(J("body")));
    } else if (!strcmp(func, "response_status")) {
        ret_json(out, orf_response_status(J("result")));
    } else if (!strcmp(func, "hermes_extras")) {
        ret_json(out, orf_hermes_extras(J("completed"), J("is_partial"), J("is_failed"),
                                        J("err_msg"), J("finish_reason"), B("budget_exhausted", false)));
    } else if (!strcmp(func, "post_stream_transform")) {
        ret_json(out, orf_post_stream_transform(J("result")));
    } else if (!strcmp(func, "cap_text")) {
        ret_json(out, orf_cap_text(J("text"), N("keep", 0)));
    } else if (!strcmp(func, "is_reasoning_input_item")) {
        ret_json(out, orf_is_reasoning_input_item(J("item")));
    } else if (!strcmp(func, "matching_replay")) {
        ret_json(out, orf_matching_replay(J("key"), J("binding"), sp));
    } else if (!strcmp(func, "store_nonstream_response")) {
        ret_json(out, orf_store_nonstream_response(J("snapshot"), J("replay_record"),
                                                   J("conversation"), J("terminal_replay"), sp));
    } else if (!strcmp(func, "parse_chat_messages")) {
        ret_json(out, orf_parse_chat_messages(J("messages"), sp));
    } else if (!strcmp(func, "parse_responses_input")) {
        ret_json(out, orf_parse_responses_input(J("input"), sp));
    } else if (!strcmp(func, "parse_conversation_history")) {
        ret_json(out, orf_parse_conversation_history(J("history"), sp));
    } else if (!strcmp(func, "select_request_route")) {
        ret_json(out, orf_select_request_route(J("body"), J("session_id"), J("gateway_session_key"),
                                               J("model_alias"), J("model_name"),
                                               B("direct_model_requests", true), sp));
    } else if (!strcmp(func, "cap_history_tool_outputs")) {
        ret_json(out, orf_cap_history_tool_outputs(J("history"), N("max_chars", 0)));
    } else if (!strcmp(func, "build_response_conversation_history")) {
        ret_json(out, orf_build_response_conversation_history(
            J("conversation_history"), J("user_message"), J("result"), J("final_response"),
            N("tool_output_max_chars", 0), sp));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
        ret_json(out, NULL);
    }

    json_set(out, "trace", trace);
    json_free(counters);
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    while ((len = getline(&line, &cap, stdin)) > 0) {
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        if (line[0] != '{') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case: %s\n", err ? err : "not object");
            free(err);
            json_free(c);
            continue;
        }
        const char *func = json_get_str(c, "func", "");
        const json_t *in = json_obj_get(c, "in");
        json_t *empty = json_object();
        json_t *out = json_object();
        /* Reserve envelope key positions: the oracle's want is built
         * {ret, trace} and canon() treats insertion order as semantic.
         * Dispatch replaces ret in place; trace is replaced last. */
        json_set(out, "ret", json_null());
        json_set(out, "trace", json_array());
        run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
        /* Serialize a string node for func (bounds-safe); never call
         * json_escape_string (off-by-one heap overflow in libjson). */
        json_t *func_node = json_string(func);
        char *func_s = json_serialize(func_node);
        json_free(func_node);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out);
        json_free(c);
    }
    free(line);
    return 0;
}
