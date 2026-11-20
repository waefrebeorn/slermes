/*
 * t_port_stream_dispatch_flow2.c — oracle harness for the
 * gateway/stream_dispatch.py FLOW remainder ported in
 * src/gateway/stream_dispatch_flow2.c.
 *
 * Self-contained: includes the port .c by AREA-QUALIFIED name. Reads JSON-line
 * cases from stdin (tests/fixtures/stream_dispatch_flow2.jsonl), wires the
 * universal seam to the SCRIPTED results in each case's in.io object (a
 * recording stub appending one trace event per seam call, in call order), and
 * emits {"func":..., "in":..., "out":{"ret":..., "trace":[...], "state":...}}.
 * The dispatcher state (in.state) is normalized like __init__ (tool_mode or
 * "all"), mutated in place (_last_tool) and echoed as out.state.
 *
 * Script shapes: a bare JSON array is a SEQUENCE (one result per call, the LAST
 * element repeats after exhaustion); any other value is reused on every call;
 * a scripted JSON null is delivered as a NULL pointer (Python None).
 * Fail-closed: a MISSING or EMPTY script yields a RuntimeError raise spec
 * ("no script for <name>"), recorded as the event ret then propagated or
 * swallowed by the port exactly as live. BaseException kinds pass through every
 * except Exception site.
 *
 * tests/oracle_stream_dispatch_flow2.py drives the LIVE Python with the same
 * scripts and compares the ordered seam-call trace + return + state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/stream_dispatch_flow2.c"

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

#define J(k) json_obj_get(in, k)

/* Build the working dispatcher state from in.state, normalized like __init__:
 * tool_mode or "all"; preview_max_len default 40; _last_tool default null.
 * Callback/sink/adapter markers are copied verbatim (presence = non-null). */
static json_t *build_state(const json_t *in) {
    const json_t *src = J("state");
    json_t *st = json_object();
    const char *tm = json_get_str(src, "tool_mode", NULL);
    /* Python `tool_mode or "all"`: falsy (null/""/0/false) -> "all". */
    const json_t *tmn = src ? json_obj_get(src, "tool_mode") : NULL;
    if (!sd2_truth(tmn)) tm = "all";
    json_set(st, "tool_mode", json_string(tm ? tm : "all"));
    json_set(st, "preview_max_len", json_number(json_get_num(src, "preview_max_len", 40)));
    const json_t *last = src ? json_obj_get(src, "_last_tool") : NULL;
    json_set(st, "_last_tool", last ? json_copy(last) : json_null());
    static const char *markers[] = {
        "sink", "_enqueue_tool_line", "_on_long_tool", "_on_notice", "adapter"};
    for (size_t i = 0; i < sizeof(markers) / sizeof(markers[0]); i++) {
        const json_t *m = src ? json_obj_get(src, markers[i]) : NULL;
        json_set(st, markers[i], m ? json_copy(m) : json_null());
    }
    return st;
}

static json_t *echo_state(const json_t *st) {
    json_t *out = json_object();
    json_set(out, "tool_mode", json_copy(json_obj_get(st, "tool_mode")));
    json_set(out, "preview_max_len", json_copy(json_obj_get(st, "preview_max_len")));
    json_set(out, "_last_tool", json_copy(json_obj_get(st, "_last_tool")));
    return out;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, json_array(), json_object() };
    sd2_seams_t seams = { &ctx, script_call };
    const sd2_seams_t *sp = &seams;
    json_t *state = build_state(in);
    const json_t *event = J("event");
    json_t *empty_event = json_object();
    const json_t *ev = event ? event : empty_event;
    json_t *r = NULL;

    if (strcmp(func, "dispatch") == 0) {
        r = sd2_dispatch(sp, state, ev);
    } else if (strcmp(func, "dispatch_inner") == 0) {
        r = sd2_dispatch_inner(sp, state, ev);
    } else if (strcmp(func, "dispatch_tool_call") == 0) {
        r = sd2_dispatch_tool_call(sp, state, ev);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", ctx.trace);
    json_set(out, "state", echo_state(state));
    json_free(state);
    json_free(empty_event);
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