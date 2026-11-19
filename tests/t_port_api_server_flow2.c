/*
 * t_port_api_server_flow2.c — oracle harness for the UNCLAIMED remainder of
 * gateway/platforms/api_server.py ported in src/gateway/platforms/api_server_flow2.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * "in" carries the injected adapter-state mirror ("state"), the scripted seam
 * results ("io") and the call arguments. The universal seam stub records one
 * {ev, args, ret} trace event per call in call order and returns the script
 * (NULL == None; a {"__raise__":...} spec is returned for the port to raise).
 * A missing/exhausted script is FAIL-CLOSED: the stub records and returns a
 * {"__raise__":"RuntimeError","message":"no script for X"} spec, matching the
 * oracle's next_script. The harness emits {"func","in","out":{ret,trace,state}}
 * per case; tests/oracle_api_server_flow2.py drives LIVE Python identically.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/platforms/api_server_flow2.c"

typedef struct {
    const json_t *in;
    json_t *io;       /* borrowed in["io"] */
    json_t *trace;    /* owned array */
    json_t *pos;      /* owned per-name queue positions */
} as2_script_ctx;

/* Script lookup: bare array = per-call queue; {"__script__":true,"ret":v} =
 * constant list/object; anything else = constant. Absent/exhausted sets
 * *failclosed and fills fcmsg (the stub then returns a RuntimeError spec). */
static const json_t *as2_next_script(as2_script_ctx *c, const char *name,
                                     bool *failclosed, char *fcmsg, size_t fcsz) {
    *failclosed = false;
    const json_t *v = c->io ? json_obj_get(c->io, name) : NULL;
    if (!v) {
        *failclosed = true;
        snprintf(fcmsg, fcsz, "no script for %s", name);
        return NULL;
    }
    if (v->type == JSON_ARRAY) {
        double idx = json_get_num(c->pos, name, 0);
        size_t i = (size_t)idx;
        if (i >= json_len(v)) {
            *failclosed = true;
            snprintf(fcmsg, fcsz, "script exhausted for %s", name);
            return NULL;
        }
        json_set(c->pos, name, json_number((double)(i + 1)));
        return json_get(v, i);
    }
    if (v->type == JSON_OBJECT && json_get_bool(v, "__script__", false))
        return json_obj_get(v, "ret");
    return v;
}

static json_t *as2_stub_call(void *ud, const char *name, const json_t *args) {
    as2_script_ctx *c = ud;
    bool fc = false;
    char fcmsg[192];
    const json_t *script = as2_next_script(c, name, &fc, fcmsg, sizeof(fcmsg));
    json_t *ret_node;
    if (fc) ret_node = as2_raise_spec("RuntimeError", fcmsg);   /* owned */
    else ret_node = script ? json_copy(script) : json_null();
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args ? json_copy(args) : json_null());
    json_set(ev, "ret", json_copy(ret_node));
    json_append(c->trace, ev);
    if (ret_node->type == JSON_NULL) { json_free(ret_node); return NULL; }
    return ret_node;   /* owned; the port frees it (as2_call result) */
}

/* ── state defaulting (mirrors the oracle's setup_case defaults) ─────────── */
static void as2_default_obj(json_t *state, const char *key) {
    if (!as2_is_obj(json_obj_get(state, key))) json_set(state, key, json_object());
}
static void as2_default_arr(json_t *state, const char *key) {
    if (!as2_is_arr(json_obj_get(state, key))) json_set(state, key, json_array());
}
static void as2_state_defaults(json_t *state) {
    if (!json_has(state, "host")) json_set(state, "host", json_string("127.0.0.1"));
    if (!json_has(state, "port")) json_set(state, "port", json_number(AS2_DEFAULT_PORT));
    if (!json_has(state, "inflight_agent_runs")) json_set(state, "inflight_agent_runs", json_number(0));
    as2_default_obj(state, "run_statuses");
    if (!json_has(state, "metrics_day")) json_set(state, "metrics_day", json_string(""));
    if (!json_has(state, "metrics_requests_today")) json_set(state, "metrics_requests_today", json_number(0));
    if (!json_has(state, "metrics_messages_today")) json_set(state, "metrics_messages_today", json_number(0));
    if (!json_has(state, "metrics_tokens_today")) json_set(state, "metrics_tokens_today", json_number(0));
    as2_default_arr(state, "metrics_latency_ms");
    if (!json_has(state, "metrics_last_request_at")) json_set(state, "metrics_last_request_at", json_null());
    if (!json_has(state, "metrics_last_heartbeat_at")) json_set(state, "metrics_last_heartbeat_at", json_null());
    if (!json_has(state, "model_name")) json_set(state, "model_name", json_string("hermes-agent"));
    as2_default_obj(state, "last_resolved_model");
    as2_default_obj(state, "model_routes");
    as2_default_arr(state, "cors_origins");
    if (!json_has(state, "gateway_runner")) json_set(state, "gateway_runner", json_null());
    if (!json_has(state, "api_key")) json_set(state, "api_key", json_string(""));
    as2_default_obj(state, "runtime_kwargs");
    as2_default_obj(state, "result");
}

static void as2_run_case(const char *func, const json_t *in, json_t *out) {
    json_t *state = json_copy(json_obj_get(in, "state"));
    if (!as2_is_obj(state)) { json_free(state); state = json_object(); }
    as2_state_defaults(state);
    as2_script_ctx ctx = {
        .in = in, .io = json_obj_get(in, "io"),
        .trace = json_array(), .pos = json_object(),
    };
    as2_env env = { .state = state, .call = as2_stub_call, .ud = &ctx };
    json_t *ret = NULL;
    bool found = false;
    for (size_t i = 0; i < sizeof(as2_table) / sizeof(as2_table[0]); i++) {
        if (strcmp(func, as2_table[i].name) == 0) {
            ret = as2_table[i].fn(&env, in);
            found = true;
            break;
        }
    }
    if (!found) json_set(out, "error", json_string("unknown func"));
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", ctx.trace);
    json_set(out, "state", state);
    json_free(ctx.pos);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    FILE *f = (argc > 1) ? fopen(argv[1], "rb") : stdin;
    if (!f) { fprintf(stderr, "cannot open fixture\n"); return 1; }
    char line[1 << 20];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '#') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case line: %s\n", err ? err : "not an object");
            free(err);
            json_free(c);
            continue;
        }
        const char *func = json_get_str(c, "func", "");
        const json_t *in = json_obj_get(c, "in");
        json_t *empty = json_object();
        json_t *out = json_object();
        as2_run_case(func, in ? in : empty, out);
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
    if (f != stdin) fclose(f);
    return 0;
}