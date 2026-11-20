/*
 * t_port_auth_flow2.c — oracle harness for the three still-UNCLAIMED auth.py
 * FLOW cores ported in src/cli/auth_flow2.c:
 *   primary_failure_wording, OAuthProviderFlow.is_terminal_refresh_error,
 *   _resolve_nous_access_token.
 *
 * Self-contained: includes the port .c by area-qualified name. Reads JSON-line
 * cases from stdin (tests/fixtures/auth_flow2.jsonl), wires the universal seam
 * to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 *
 * FAIL-CLOSED: a seam key MISSING from the script delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present JSON
 * null is a valid None result; a JSON array script is one result PER CALL
 * (last repeats, empty array -> None). tests/oracle_auth_flow2.py drives the
 * LIVE Python with the same scripts and compares the ordered trace + return.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/auth_flow2.c"

typedef struct {
    const json_t *in;
    au2_trace_t *trace;
    json_t *counters;
} au2_script_ctx_t;

/* Mirror of the oracle's seam(): null args are dropped so a scripted null and
 * an unscripted-but-nullable field compare identically on both sides. Args and
 * ret are SNAPSHOTS (json_copy): a later in-place state mutation cannot leak
 * backwards into an already recorded trace entry. */
static json_t *au2_args_drop_null(const json_t *args) {
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

static json_t *au2_script_call(void *vctx, const char *name, const json_t *args) {
    au2_script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (!sc) {
        char *msg = au2_xsf("unscripted seam: %s", name);
        result = au2_raise_spec("RuntimeError", msg);
        free(msg);
    } else if (sc->type == JSON_OBJECT && json_obj_get(sc, "__arr__")) {
        /* An explicit array VALUE (e.g. the (ts, token) cache entry or the
         * txn triple) — not a per-call queue. */
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
    json_set(e, "args", au2_args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    au2_trace_add(ctx->trace, e);
    return result;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    au2_trace_t trace = {NULL};
    json_t *counters = json_object();
    au2_script_ctx_t ctx = {in, &trace, counters};
    au2_seams_t seams = {&ctx, au2_script_call};
    const au2_seams_t *sp = &seams;
    json_t *ret = NULL;

    if (!strcmp(func, "primary_failure_wording")) {
        ret = au2_primary_failure_wording(json_obj_get(in, "exc"));
    } else if (!strcmp(func, "is_terminal_refresh_error")) {
        ret = au2_is_terminal_refresh_error(json_obj_get(in, "self"),
                                            json_obj_get(in, "exc"));
    } else if (!strcmp(func, "resolve_nous_access_token")) {
        ret = au2_resolve_nous_access_token(sp, in);
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", trace.events ? json_copy(trace.events) : json_array());
    au2_trace_free(&trace);
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
