/*
 * t_port_cron_flow4.c — oracle harness for the hermes_cli/cron.py remainder
 * FLOW port in src/cli/cron_flow4.c (fourth wave).
 *
 * Includes the port by AREA-QUALIFIED name. Reads JSON-line cases from stdin
 * (auto-fed by run_one_oracle.sh from tests/fixtures/cron_flow4.jsonl), wires
 * ONE universal seam to the per-name script in each case's in.io object, and
 * emits {"func":..., "in":..., "out":{"ret":..., "trace":[...], "state":null}}.
 *
 * Script shapes (both sides): a bare JSON array is a SEQUENCE (one result per
 * call, LAST element repeats after exhaustion); any other value is reused;
 * a scripted null is delivered as a NULL pointer (Python None). Fail-closed:
 * a MISSING or EMPTY script yields {"__raise__":"RuntimeError",
 * "msg":"no script for <name>"}, recorded as the event ret then propagated
 * or swallowed by the port exactly as live. Every seam call records one
 * {ev, args, ret} event in call order; object keys with null values are
 * dropped from args on both sides.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/cron_flow4.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *counters;
} script_ctx_t;

static json_t *make_raise(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}

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
    script_ctx_t *ctx = (script_ctx_t *)vctx;
    const json_t *io = json_obj_get(ctx->in, "io");
    const json_t *sc = io ? json_obj_get(io, name) : NULL;
    json_t *result = NULL;
    bool fail_closed = false;
    /* A list-valued seam RESULT is wrapped in {"__script__":true,"ret":[...]}:
     * a bare array is a per-call SEQUENCE. Both sides read the wrapper. */
    if (sc && sc->type == JSON_OBJECT && json_obj_get(sc, "__script__")) {
        result = json_copy(json_obj_get(sc, "ret"));
        if (!result) result = json_null();
    } else if (sc && sc->type == JSON_ARRAY) {
        size_t n = json_len(sc);
        if (n == 0) {
            fail_closed = true;
        } else {
            double idx = json_get_num(ctx->counters, name, 0);
            size_t i = (size_t)idx;
            result = json_copy(json_get(sc, i < n ? i : n - 1));
            json_set(ctx->counters, name, json_number(idx + 1));
        }
    } else if (sc) {
        result = json_copy(sc);
    } else {
        fail_closed = true;
    }
    if (fail_closed) {
        char m[160];
        snprintf(m, sizeof(m), "no script for %s", name);
        result = make_raise("RuntimeError", m);
    }
    /* A scripted JSON null is delivered as a NULL pointer, not a json_null. */
    if (result && result->type == JSON_NULL) { json_free(result); result = NULL; }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    json_append(ctx->trace, e);
    return result;
}

#define J(k)  json_obj_get(in, k)
#define S(k)  json_get_str(in, k, NULL)
#define N(k,d) json_get_num(in, k, (d))
#define B(k,d) json_get_bool(in, k, (d))

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx;
    ctx.in = in;
    ctx.trace = json_array();
    ctx.counters = json_object();
    cr4_seams_t seams;
    seams.ctx = &ctx;
    seams.call = script_call;
    const cr4_seams_t *sp = &seams;
    json_t *r = NULL;

    if (strcmp(func, "builtin_gateway_liveness") == 0) {
        r = cr4_builtin_gateway_liveness(sp, N("ticker_interval", 60.0));
    } else if (strcmp(func, "ticker_age_is_fresh") == 0) {
        r = cr4_ticker_age_is_fresh(J("age"), N("ticker_interval", 60.0));
    } else if (strcmp(func, "store_unwritable_report") == 0) {
        r = cr4_store_unwritable_report(sp);
    } else if (strcmp(func, "print_store_unwritable") == 0) {
        r = cr4_print_store_unwritable(J("report"), B("brief", false), sp);
    } else if (strcmp(func, "probe_then_list_jobs") == 0) {
        r = cr4_probe_then_list_jobs(B("include_disabled", true), B("brief", false), sp);
    } else if (strcmp(func, "incidents") == 0) {
        r = cr4_incidents(J("args"), sp);
    } else if (strcmp(func, "doctor") == 0) {
        r = cr4_doctor(sp);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", ctx.trace);
    json_set(out, "state", json_null());
    json_free(ctx.counters);
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
    if (f != stdin) fclose(f);
    return 0;
}
