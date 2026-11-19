/*
 * t_port_openai_routes_flow2.c — oracle harness for the
 * gateway/platforms/api_server_openai_routes.py UNCLAIMED-remainder FLOW
 * ported in src/gateway/platforms/openai_routes_flow2.c.
 *
 * Self-contained: includes the port .c by area-qualified path. Reads
 * JSON-line cases from stdin (tests/fixtures/openai_routes_flow2.jsonl),
 * wires the universal seam to the SCRIPTED results in each case's "seams"
 * object (a recording stub appending one trace event per seam call, in call
 * order), and emits {"func":..., "in":..., "out":{"ret":...,"trace":[...],
 * "st":{...}}}. FAIL-CLOSED: a MISSING script returns a recorded
 * {"__raise__":"RuntimeError","msg":"no script for <name>"} spec; a scripted
 * JSON null returns a NULL pointer (None), never a json_null node; a bare
 * array is a per-call queue (last value repeats). Stateful rs_* functions
 * mutate a PRIVATE copy of in.state, echoed as out.st even on a raise.
 * tests/oracle_openai_routes_flow2.py drives the LIVE Python with the same
 * scripts and compares ordered trace + ret + state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/platforms/openai_routes_flow2.c"

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
    if (!sc) {
        /* FAIL-CLOSED: missing script -> recorded RuntimeError spec. */
        char *m = xsf("no script for %s", name);
        result = raise_obj("RuntimeError", m);
        free(m);
    } else if (sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx;
        size_t n = json_len(sc);
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else {
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

#define J(k) json_obj_get(in, k)
#define B(k, d) json_get_bool(in, k, d)

static void ret_json(json_t *out, json_t *r) {
    json_set(out, "ret", r ? r : json_null());
}

static bool is_rs_func(const char *func) {
    return !strncmp(func, "rs_", 3);
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    json_t *trace = json_array();
    json_t *counters = json_object();
    script_ctx_t ctx = {in, trace, counters};
    or2_seams_t seams = {&ctx, script_call};
    const or2_seams_t *sp = &seams;
    json_t *state = NULL;
    if (is_rs_func(func)) {
        const json_t *st = J("state");
        state = (st && st->type == JSON_OBJECT) ? json_copy(st) : json_object();
    }

    if (!strcmp(func, "transformed_notice")) {
        ret_json(out, or2_transformed_notice(sp));
    } else if (!strcmp(func, "message_item")) {
        ret_json(out, or2_message_item(J("text")));
    } else if (!strcmp(func, "reasoning_item")) {
        ret_json(out, or2_reasoning_item(J("text"), sp));
    } else if (!strcmp(func, "trim_tool_items")) {
        ret_json(out, or2_trim_tool_items(J("items"), sp));
    } else if (!strcmp(func, "turn_start_index")) {
        ret_json(out, or2_turn_start_index(J("conversation_history"), J("user_message"),
                                           J("result"), sp));
    } else if (!strcmp(func, "turn_transcript_messages")) {
        ret_json(out, or2_turn_transcript_messages(J("conversation_history"), J("user_message"),
                                                   J("result"), sp));
    } else if (!strcmp(func, "turn_reasoning_text")) {
        ret_json(out, or2_turn_reasoning_text(J("conversation_history"), J("user_message"),
                                              J("result"), sp));
    } else if (!strcmp(func, "extract_output_items")) {
        json_t *si = J("start_index") ? json_copy(J("start_index")) : json_number(0);
        ret_json(out, or2_extract_output_items(J("result"), si, J("response_id"), sp));
        json_free(si);
    } else if (!strcmp(func, "chat_request_id")) {
        ret_json(out, or2_chat_request_id(J("request"), sp));
    } else if (!strcmp(func, "authority_chat_kwargs")) {
        ret_json(out, or2_authority_chat_kwargs(J("gateway_runner"), J("request"),
                                                J("provided_session_id"), sp));
    } else if (!strcmp(func, "response_identity")) {
        ret_json(out, or2_response_identity(J("durable_key"), sp));
    } else if (!strcmp(func, "implicit_chat_session")) {
        ret_json(out, or2_implicit_chat_session(J("gateway_runner"), J("request"),
                                                J("session_id"), sp));
    } else if (!strcmp(func, "run_idempotent")) {
        ret_json(out, or2_run_idempotent(J("gateway_runner"), J("request"), J("body"),
                                         J("log_label"), J("fingerprint_keys"), J("route"), sp));
    } else if (!strcmp(func, "prepare_sse_response")) {
        ret_json(out, or2_prepare_sse_response(J("request"), J("session_id"),
                                               J("gateway_session_key"), sp));
    } else if (!strcmp(func, "replay_sse_responses")) {
        ret_json(out, or2_replay_sse_responses(J("request"), J("replay"), sp));
    } else if (!strcmp(func, "responses_durable_replay")) {
        ret_json(out, or2_responses_durable_replay(J("gateway_runner"), J("request"),
                                                   J("body"), J("stream"), sp));
    } else if (!strcmp(func, "rs_item_id")) {
        ret_json(out, or2_rs_item_id(state, J("kind"), J("call_id"), B("commentary", false), sp));
    } else if (!strcmp(func, "rs_envelope")) {
        ret_json(out, or2_rs_envelope(state, J("status")));
    } else if (!strcmp(func, "rs_terminal_envelope")) {
        ret_json(out, or2_rs_terminal_envelope(state, J("status"), J("output"), J("error"), sp));
    } else if (!strcmp(func, "rs_history_with_user")) {
        ret_json(out, or2_rs_history_with_user(state));
    } else if (!strcmp(func, "rs_persist_snapshot")) {
        ret_json(out, or2_rs_persist_snapshot(state, J("response_env"), J("history"),
                                              J("session_id"), sp));
    } else if (!strcmp(func, "rs_commit_terminal")) {
        ret_json(out, or2_rs_commit_terminal(state, J("env"), J("full_history"),
                                             J("session_id"), J("record"), sp));
    } else if (!strcmp(func, "rs_write_event")) {
        ret_json(out, or2_rs_write_event(state, J("event_type"), J("data"), sp));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
        ret_json(out, NULL);
    }

    json_set(out, "trace", trace);
    json_set(out, "st", state ? state : json_object());
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
         * {ret, trace, st} and canon() treats insertion order as semantic. */
        json_set(out, "ret", json_null());
        json_set(out, "trace", json_array());
        json_set(out, "st", json_object());
        run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
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