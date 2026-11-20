/*
 * t_port_transport_types_flow2.c — oracle harness for the agent/transports/types.py
 * UNCLAIMED remainder ported in src/agent/transports/transport_types_flow2.c (the 12
 * dataclass-generated methods __init__ / __repr__ / __eq__ / __replace__ x ToolCall /
 * Usage / NormalizedResponse).
 *
 * Self-contained: includes the port .c by area-qualified path. Reads JSON-line cases
 * from stdin (tests/fixtures/transport_types_flow2.jsonl), wires the universal seam to
 * the SCRIPTED results in each case's "seams" object (a recording stub appending one
 * trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 *
 * FAIL-CLOSED script semantics (identical to the oracle): a MISSING script returns a
 * RuntimeError raise spec {"__raise__":"RuntimeError","msg":"no script for seam: <name>"};
 * a scripted JSON null returns a NULL pointer (None), never a json_null node; an ARRAY
 * script is a per-call queue (last value repeats, empty array -> None);
 * {"__raise__": cls, "msg": ...} scripts an exception. The observable effects are the
 * ordered seam trace + the return value. tests/oracle_transport_types_flow2.py drives
 * the LIVE Python with the same scripts and compares ordered trace + ret.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "agent/transports/transport_types_flow2.c"

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
        /* FAIL-CLOSED: a missing script is a RuntimeError raise spec (NOT None). */
        char *m = tt2_xsf("no script for seam: %s", name);
        result = tt2_raise("RuntimeError", m);
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
    json_set(e, "args", args_drop_null(args));   /* borrowed args: copy, do not free */
    json_set(e, "ret", result ? json_copy(result) : json_null());
    if (!ctx->trace) ctx->trace = json_array();
    json_append(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, NULL, json_object()};
    tt2_seams_t seams = {&ctx, script_call};
    const tt2_seams_t *sp = &seams;
    json_t *r = NULL;

    if (!strcmp(func, "tool_call_init")) {
        r = tt2_tool_call_init(J("fields"));
    } else if (!strcmp(func, "tool_call_repr")) {
        r = tt2_tool_call_repr(J("fields"));
    } else if (!strcmp(func, "tool_call_eq")) {
        r = tt2_tool_call_eq(J("self"), J("other"), S("other_class"));
    } else if (!strcmp(func, "tool_call_replace")) {
        r = tt2_tool_call_replace(sp, J("self"), J("changes"));
    } else if (!strcmp(func, "usage_init")) {
        r = tt2_usage_init(J("fields"));
    } else if (!strcmp(func, "usage_repr")) {
        r = tt2_usage_repr(J("fields"));
    } else if (!strcmp(func, "usage_eq")) {
        r = tt2_usage_eq(J("self"), J("other"), S("other_class"));
    } else if (!strcmp(func, "usage_replace")) {
        r = tt2_usage_replace(sp, J("self"), J("changes"));
    } else if (!strcmp(func, "normalized_response_init")) {
        r = tt2_normalized_response_init(J("fields"));
    } else if (!strcmp(func, "normalized_response_repr")) {
        r = tt2_normalized_response_repr(J("fields"));
    } else if (!strcmp(func, "normalized_response_eq")) {
        r = tt2_normalized_response_eq(J("self"), J("other"), S("other_class"));
    } else if (!strcmp(func, "normalized_response_replace")) {
        r = tt2_normalized_response_replace(sp, J("self"), J("changes"));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", ctx.trace ? ctx.trace : json_array());
    json_free(ctx.counters);
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
        /* Reserve envelope key positions: the oracle's want is built {ret, trace}
         * and canon() treats insertion order as semantic. */
        json_set(out, "ret", json_null());
        json_set(out, "trace", json_array());
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