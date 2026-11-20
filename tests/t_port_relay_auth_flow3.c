/*
 * t_port_relay_auth_flow3.c — oracle harness for the standalone FLOW port in
 * src/gateway/relay/relay_auth_flow3.c (live gateway/relay/auth.py).
 *
 * Reads JSON-line cases from stdin (tests/fixtures/relay_auth_flow3.jsonl),
 * wires the universal seam to the SCRIPTED results in each case's "seams"
 * object (one ordered trace event per seam call), and emits
 *   {"func":..., "in":<echo>, "out":{"ret":..., "trace":[...]}}
 * so tests/oracle_relay_auth_flow3.py can recompute the LIVE reference from
 * the echoed input. Args/ret are json_copy snapshots: no later mutation leaks
 * backwards into a recorded event.
 *
 * FAIL-CLOSED: a seam key MISSING from the script delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present JSON
 * null is a valid None; a JSON array script is one result PER CALL (last
 * repeats, empty -> None). An object with "__arr__" is an explicit array VALUE
 * (not a per-call queue). A raise spec is the ret AND propagates as the ret.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/relay/relay_auth_flow3.c"

typedef struct {
    const json_t *in;
    ra3_trace_t *trace;
    json_t *counters;
} ra3_script_ctx_t;

static json_t *ra3_args_drop_null(const json_t *args) {
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

static json_t *ra3_script_call(void *vctx, const char *name, const json_t *args) {
    ra3_script_ctx_t *ctx = (ra3_script_ctx_t *)vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (!sc) {
        char *msg = ra3_xsf("unscripted seam: %s", name);
        result = ra3_raise_spec("RuntimeError", msg);
        free(msg);
    } else if (sc->type == JSON_OBJECT && json_obj_get(sc, "__arr__")) {
        result = json_copy(json_obj_get(sc, "__arr__"));
    } else if (sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx;
        size_t n = json_len(sc);
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else {
        result = json_copy(sc);
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", ra3_args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    ra3_trace_add(ctx->trace, e);
    return result;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    ra3_trace_t trace = {NULL};
    json_t *counters = json_object();
    ra3_script_ctx_t ctx = {in, &trace, counters};
    ra3_seams_t seams = {&ctx, ra3_script_call};
    const ra3_seams_t *sp = &seams;
    json_t *ret = NULL;

    const char *payload = json_get_str(in, "payload", "");
    const char *secret  = json_get_str(in, "secret", "");

    if (!strcmp(func, "sign")) {
        ret = ra3_sign(sp, payload, secret);
    } else if (!strcmp(func, "make_token")) {
        double ttl = json_has(in, "ttl_seconds")
                   ? json_get_num(in, "ttl_seconds", 0) : 0.0;
        ret = ra3_make_token(sp, payload, secret, ttl);
    } else if (!strcmp(func, "make_upgrade_token")) {
        double ttl = json_has(in, "ttl_seconds")
                   ? json_get_num(in, "ttl_seconds", 300) : 300.0;
        ret = ra3_make_upgrade_token(sp, payload, secret, ttl);
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", trace.events ? json_copy(trace.events) : json_array());
    ra3_trace_free(&trace);
    json_free(counters);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
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
