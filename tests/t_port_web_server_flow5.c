/*
 * t_port_web_server_flow5.c — oracle harness for the FINAL UNCLAIMED
 * web-server FLOW surface of hermes_cli/web_server.py ported in
 * src/gateway/web_server_flow5.c (_dashboard_selftest_loop, the async
 * dashboard self-test driver).
 *
 * Self-contained: includes the port .c by area-qualified name. Reads JSON-line
 * cases from stdin (tests/fixtures/web_server_flow5.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 *
 * FAIL-CLOSED: a seam key MISSING from the script delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present JSON
 * null is a valid None result; a JSON array script is one result PER CALL
 * (last repeats, empty array -> None). tests/oracle_web_server_flow5.py drives
 * the LIVE Python with the same scripts and compares the ordered decision
 * sequence + return value (raise envelopes included).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/web_server_flow5.c"

typedef struct {
    const json_t *in;
    ws5_trace_t *trace;
    json_t *counters;
} ws5_script_ctx_t;

static json_t *ws5_args_drop_null(const json_t *args) {
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

static json_t *ws5_script_call(void *vctx, const char *name, const json_t *args) {
    ws5_script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (!sc) {
        char *msg = ws5_xsf("unscripted seam: %s", name);
        result = ws5_raise_spec("RuntimeError", msg);
        free(msg);
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
    json_set(e, "args", ws5_args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    ws5_trace_add(ctx->trace, e);
    return result;
}

#define J(k) json_obj_get(in, k)

/* Normalize a raise envelope to {__raise__, msg[, code]} so extra scripted
 * keys never leak into the ret comparison. Non-raise rets pass through. */
static json_t *ws5_norm_raise(json_t *r) {
    if (!ws5_is_raise(r)) return r;
    json_t *o = json_object();
    json_set(o, "__raise__", json_string(ws5_raise_class(r)));
    json_set(o, "msg", json_string(json_get_str(r, "msg", "")));
    const json_t *code = json_obj_get(r, "code");
    if (code) json_set(o, "code", json_copy(code));
    json_free(r);
    return o;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    ws5_trace_t trace = {NULL};
    json_t *counters = json_object();
    ws5_script_ctx_t ctx = {in, &trace, counters};
    ws5_seams_t seams = {&ctx, ws5_script_call};
    const ws5_seams_t *sp = &seams;
    json_t *ret = NULL;

    if (!strcmp(func, "dashboard_selftest_loop")) {
        const json_t *st = J("app_state");
        json_t *empty = json_object();
        ret = ws5_dashboard_selftest_loop(sp, st ? st : empty);
        json_free(empty);
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    json_set(out, "ret", ws5_norm_raise(ret ? ret : json_null()));
    json_set(out, "trace", trace.events ? json_copy(trace.events) : json_array());
    ws5_trace_free(&trace);
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
