/*
 * t_port_relay_descriptor_flow4.c — oracle harness for the standalone FLOW
 * port of gateway/relay/descriptor.py (src/gateway/relay/relay_descriptor_flow4.c).
 *
 * Reads JSON-line cases from stdin (tests/fixtures/relay_descriptor_flow4.jsonl),
 * wires the universal seam to each case's scripted "seams" object (one trace
 * event per call, in order), and emits
 *   {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 * Fail-closed: a missing/empty seam script yields a RuntimeError raise spec.
 * An uncaught raise parks in the static rd4_pending channel and is emitted as
 * ret. Unlike other harnesses, args keep JSON nulls (the oracle records None).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gateway/relay/relay_descriptor_flow4.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *counters;
} script_ctx_t;

static json_t *rd4h_make_raise(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
static json_t *rd4h_script_call(void *vctx, const char *name, const json_t *args) {
    script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (sc && sc->type == JSON_ARRAY) {
        size_t n = json_len(sc);
        if (n == 0) {
            char *m = rd4_xsf("no script for %s", name);
            result = rd4h_make_raise("RuntimeError", m);
            free(m);
        } else {
            double idx = json_get_num(ctx->counters, name, 0);
            size_t i = (size_t)idx;
            result = json_copy(json_get(sc, i < n ? i : n - 1));
            json_set(ctx->counters, name, json_number(idx + 1));
        }
    } else if (sc) {
        result = json_copy(sc);
    } else {
        char *m = rd4_xsf("no script for %s", name);
        result = rd4h_make_raise("RuntimeError", m);
        free(m);
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", json_copy(args));          /* keep JSON nulls */
    json_set(e, "ret", result ? json_copy(result) : json_null());
    if (!ctx->trace) ctx->trace = json_array();
    json_append(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)
#define SJ(k, d) json_get_str(in, k, d)

static void ret_json(json_t *out, json_t *r) { json_set(out, "ret", r ? r : json_null()); }
static void ret_bool(json_t *out, bool b) { json_set(out, "ret", json_bool(b)); }
static void ret_str(json_t *out, char *s) { json_set(out, "ret", json_string(s ? s : "")); free(s); }

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, NULL, json_object()};
    rd4_seams_t seams = {&ctx, rd4h_script_call};
    const rd4_seams_t *sp = &seams;
    rd4_take_pending();   /* clear any stale parked raise */

    if (!strcmp(func, "init")) {
        ret_json(out, rd4_init(J("fields")));
    } else if (!strcmp(func, "supports_op")) {
        json_t *d = rd4_init(J("fields"));
        ret_bool(out, rd4_supports_op(d, S("op")));
        json_free(d);
    } else if (!strcmp(func, "to_json")) {
        json_t *d = rd4_init(J("fields"));
        ret_str(out, rd4_to_json(d));
        json_free(d);
    } else if (!strcmp(func, "from_json")) {
        ret_json(out, rd4_from_json(SJ("data", ""), sp));
    } else if (!strcmp(func, "repr")) {
        json_t *d = rd4_init(J("fields"));
        const json_t *f = J("fields");
        bool ops_tuple = !(f && rd4_is_obj(f) && json_obj_get(f, "supported_ops"));
        ret_str(out, rd4_repr(d, ops_tuple));
        json_free(d);
    } else if (!strcmp(func, "eq")) {
        json_t *a = rd4_init(J("a"));
        json_t *b = rd4_init(J("b"));
        ret_bool(out, rd4_eq(a, b));
        json_free(a); json_free(b);
    } else if (!strcmp(func, "hash")) {
        json_t *d = rd4_init(J("fields"));
        ret_json(out, rd4_hash(d, sp));
        json_free(d);
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
        ret_json(out, NULL);
    }

    json_t *pending = rd4_take_pending();
    if (pending) json_set(out, "ret", pending);
    json_set(out, "trace", ctx.trace ? ctx.trace : json_array());
    json_free(ctx.counters);
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