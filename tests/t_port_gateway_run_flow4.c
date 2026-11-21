/*
 * t_port_gateway_run_flow4.c — oracle harness for the gateway/run.py FLOW
 * cores ported in src/gateway/gateway_run_flow4.c (fourth wave).
 *
 * Self-contained: includes the port .c by AREA-QUALIFIED name. Reads
 * JSON-line cases from stdin (tests/fixtures/gateway_run_flow4.jsonl), wires
 * the universal seam to the SCRIPTED results in each case's in.io object (a
 * recording stub appending one trace event per seam call, in call order),
 * and emits {"func":..., "in":..., "out":{"ret":..., "trace":[...],
 * "state":null}}.
 *
 * Script shapes: a bare JSON array is a SEQUENCE (one result per call, the
 * LAST element repeats after exhaustion); any other value is reused on every
 * call; a scripted JSON null is delivered as a NULL pointer (Python None).
 * Fail-closed: a MISSING or EMPTY script yields a RuntimeError raise spec
 * ("no script for <name>"), recorded as the event ret then propagated or
 * swallowed by the port exactly as live. BaseException kinds pass through.
 *
 * tests/oracle_gateway_run_flow4.py drives the LIVE Python with the same
 * scripts and compares the ordered seam-call trace plus the return value.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gateway/gateway_run_flow4.c"

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
    script_ctx_t *ctx = vctx;
    const json_t *io = json_obj_get(ctx->in, "io");
    const json_t *sc = io ? json_obj_get(io, name) : NULL;
    json_t *result = NULL;
    bool fail_closed = false;
    if (sc && sc->type == JSON_ARRAY) {
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
    /* A scripted JSON null is delivered as a NULL pointer, not a node. */
    if (result && result->type == JSON_NULL) { json_free(result); result = NULL; }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    json_append(ctx->trace, e);
    return result;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, json_array(), json_object() };
    gr4_seams_t seams = { &ctx, script_call };
    const gr4_seams_t *sp = &seams;
    json_t *r = NULL;

    if (strcmp(func, "status_action_label") == 0) {
        r = gr4_status_action_label(in, sp);
    } else if (strcmp(func, "status_action_gerund") == 0) {
        r = gr4_status_action_gerund(in, sp);
    } else if (strcmp(func, "sessions_map") == 0) {
        r = gr4_sessions_map(in, sp);
    } else if (strcmp(func, "peek_session_state") == 0) {
        r = gr4_peek_session_state(in, sp);
    } else if (strcmp(func, "session_state") == 0) {
        r = gr4_session_state(in, sp);
    } else if (strcmp(func, "is_session_running") == 0) {
        r = gr4_is_session_running(in, sp);
    } else if (strcmp(func, "running_agent_items") == 0) {
        r = gr4_running_agent_items(in, sp);
    } else if (strcmp(func, "running_agent_count") == 0) {
        r = gr4_running_agent_count(in, sp);
    } else if (strcmp(func, "resolve_session_key_or_none") == 0) {
        r = gr4_resolve_session_key_or_none(in, sp);
    } else if (strcmp(func, "get_cached_session_source") == 0) {
        r = gr4_get_cached_session_source(in, sp);
    } else if (strcmp(func, "stop_pool") == 0) {
        r = gr4_stop_pool(in, sp);
    } else if (strcmp(func, "get_or_create_pool") == 0) {
        r = gr4_get_or_create_pool(in, sp);
    } else if (strcmp(func, "update_runtime_status") == 0) {
        r = gr4_update_runtime_status(in, sp);
    } else if (strcmp(func, "session_db_get") == 0) {
        r = gr4_session_db_get(in, sp);
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
