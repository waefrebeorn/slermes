/*
 * t_port_gateway_multiplex_flow2.c — oracle harness for the
 * gateway multiplex FLOW2 port (src/cli/gateway_multiplex_flow2.c: the
 * UNCLAIMED remainder of gateway_multiplex_served.py (7 defs) +
 * gateway_runtime_multiplex.py (3 defs)).
 *
 * Self-contained: includes the port .c area-qualified and links ONLY
 * lib/libjson/json.o (per the oracle gate). Reads JSON-line cases from
 * stdin (tests/fixtures/gateway_multiplex_flow2.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording
 * stub appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...], "state":...}}.
 * A scripted JSON null returns a NULL pointer (Python None), never a
 * json_null node; {"__raise__": cls, "msg": ...} scripts an exception.
 *
 * Every ported def is pure w.r.t. caller/module state, so out.state is {}.
 * tests/oracle_gateway_multiplex_flow2.py drives the LIVE Python with the
 * same scripts and compares ordered trace + ret + state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/gateway_multiplex_flow2.c"

typedef struct {
    const json_t *in;
    gm2_trace_t *trace;
    json_t *counters;
} script_ctx_t;

/* Drop null-valued keys from a seam's args object (both sides do this). */
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
    if (sc && sc->type == JSON_ARRAY) {
        /* bare-array script == per-call QUEUE (last value repeats) */
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx;
        size_t n = json_len(sc);
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else if (sc) {
        result = json_copy(sc);          /* present null -> handled below */
    }
    /* Contract: NULL == None. A scripted null must NOT surface as a non-NULL
     * json_null node (is-None / truthiness guards would read it wrong). */
    if (result && result->type == JSON_NULL) {
        json_free(result);
        result = NULL;
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    gm2_trace_add(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)

static void ret_json(json_t *out, json_t *r) {
    json_set(out, "ret", r ? r : json_null());
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    gm2_trace_t trace = {NULL};
    json_t *counters = json_object();
    script_ctx_t ctx = {in, &trace, counters};
    gm2_seams_t seams = {&ctx, script_call};
    const gm2_seams_t *sp = &seams;

    if (!strcmp(func, "live_default_gateway_pid")) {
        ret_json(out, gm2_flow_live_default_gateway_pid(sp));
    } else if (!strcmp(func, "recorded_served_profiles")) {
        ret_json(out, gm2_flow_recorded_served_profiles(S("default_root"), sp));
    } else if (!strcmp(func, "multiplexer_served_secondaries")) {
        ret_json(out, gm2_flow_multiplexer_served_secondaries(sp));
    } else if (!strcmp(func, "served_profile_unserved_platforms")) {
        ret_json(out, gm2_flow_served_profile_unserved_platforms(S("profile"), sp));
    } else if (!strcmp(func, "served_profile_ingress_urls")) {
        ret_json(out, gm2_flow_served_profile_ingress_urls(S("profile"), sp));
    } else if (!strcmp(func, "format_ingress_url_lines")) {
        /* indent default: live signature is indent="  " — absent -> "  ". */
        const json_t *ij = J("indent");
        json_t *indent = ij ? json_copy(ij) : json_string("  ");
        ret_json(out, gm2_flow_format_ingress_url_lines(J("urls"), indent));
        json_free(indent);
    } else if (!strcmp(func, "notify_multiplexer_profiles_changed")) {
        ret_json(out, gm2_flow_notify_multiplexer_profiles_changed(
            S("profile_name"), J("timeout"), sp));
    } else if (!strcmp(func, "multiplexer_root_for")) {
        ret_json(out, gm2_flow_multiplexer_root_for(S("home"), sp));
    } else if (!strcmp(func, "implied_host_root")) {
        ret_json(out, gm2_flow_implied_host_root(S("home"), sp));
    } else if (!strcmp(func, "multiplexer_serves_home")) {
        ret_json(out, gm2_flow_multiplexer_serves_home(S("home"), sp));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
        ret_json(out, NULL);
    }

    /* All 10 defs are state-free: echo {} (matching the oracle's state {}). */
    json_set(out, "state", json_object());
    json_set(out, "trace", trace.events ? trace.events : json_array());
    json_free(counters);
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
        /* Reserve envelope key positions: ret, trace, state (canon treats
         * insertion order as semantic). */
        json_set(out, "ret", json_null());
        json_set(out, "trace", json_array());
        json_set(out, "state", json_object());
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
