/*
 * t_port_gateway_agent_flow.c — oracle harness for the gateway agent-bridge
 * FLOW cores ported in src/cli/port_gateway_agent_flow.c.
 *
 * Self-contained: includes the port .c by bare name. Reads JSON-line cases
 * from stdin (tests/fixtures/gateway_agent_flow.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...], "st":...}}.
 * The injected self-model "st" is mutated in place and echoed back.
 * tests/oracle_gateway_agent_flow.py drives the LIVE Python with the same
 * scripts and compares the ordered decision sequence + return + mutated state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "port_gateway_agent_flow.c"

typedef struct {
    const json_t *in;
    gwa_trace_t *trace;
    json_t *counters;
} script_ctx_t;

static json_t *args_drop_null(const json_t *args) {
    json_t *out = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (v && v->type == JSON_NULL) continue;
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
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    gwa_trace_add(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define SD(k, d) json_get_str(in, k, d)
#define J(k) json_obj_get(in, k)
#define B(k, d) json_get_bool(in, k, d)

static void ret_json(json_t *out, json_t *r) { json_set(out, "ret", r ? r : json_null()); }
static void ret_bool(json_t *out, bool b) { json_set(out, "ret", json_bool(b)); }
static void ret_str(json_t *out, char *s) {
    json_set(out, "ret", s ? json_string(s) : json_null());
    free(s);
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    gwa_trace_t trace = {NULL};
    json_t *counters = json_object();
    script_ctx_t ctx = {in, &trace, counters};
    gwa_seams_t seams = {&ctx, script_call};
    const gwa_seams_t *sp = &seams;
    json_t *st = json_has(in, "st") ? json_copy(J("st")) : NULL;

    if (!strcmp(func, "effort_word")) {
        ret_str(out, gwa_effort_word(J("config")));
    } else if (!strcmp(func, "route_off_session")) {
        ret_json(out, gwa_route_off_session(st, SD("model", ""), SD("provider", ""), SD("alias", ""), sp));
    } else if (!strcmp(func, "set_reasoning")) {
        ret_bool(out, gwa_set_reasoning(st, J("config"), B("explicit", false), sp));
    } else if (!strcmp(func, "sync_show_reasoning")) {
        gwa_sync_show_reasoning(st, sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "service_tier")) {
        gwa_service_tier_set(st, S("tier"), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "correct")) {
        ret_json(out, gwa_correct(st, SD("verb", "steer"), S("text"), sp));
    } else if (!strcmp(func, "take_turn_route")) {
        ret_json(out, gwa_take_turn_route(st));
    } else if (!strcmp(func, "give_back_turn_route")) {
        gwa_give_back_turn_route(st);
        ret_json(out, NULL);
    } else if (!strcmp(func, "prepare_submit")) {
        ret_str(out, gwa_prepare_submit(st, SD("turn_session_id", ""), sp));
    } else if (!strcmp(func, "submit_params")) {
        ret_json(out, gwa_submit_params(st, J("user_message"), J("persist_user_message"),
                                        S("turn_author"), sp));
    } else if (!strcmp(func, "clarify_reply")) {
        ret_json(out, gwa_clarify_reply(B("callback_present", false), J("params"), sp));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
        ret_json(out, NULL);
    }

    json_set(out, "trace", trace.events ? json_copy(trace.events) : json_array());
    if (st) json_set(out, "st", st);
    gwa_trace_free(&trace);
    json_free(counters);
}

int main(void) {
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
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
        run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
        char *func_s = json_escape_string(func);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out);
        json_free(c);
    }
    free(line);
    return 0;
}