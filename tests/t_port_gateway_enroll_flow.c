/*
 * t_port_gateway_enroll_flow.c — oracle harness for the
 * hermes_cli/gateway_enroll.py FLOW + gateway_multiplex_mode.py UNCLAIMED
 * cluster port in src/cli/gateway_enroll_flow.c.
 *
 * Self-contained: includes the port .c area-qualified. Reads JSON-line cases
 * from stdin (tests/fixtures/gateway_enroll_flow.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...], "state":...}}.
 * A scripted JSON null returns a NULL pointer (None), never a json_null node;
 * {"__raise__": cls, "msg": ...} scripts an exception.
 *
 * State echo:
 * - standalone_launcher_decision / resolve_multiplex_mode mutate a working
 *   copy of in.config (dict or null) and echo it as out.state (even on a
 *   raise — partial mutation history);
 * - record_multiplex_decision / publish_pending_multiplex_decision /
 *   log_multiplex_decision own the module-global _pending_decision list,
 *   modelled as state {"pending":[...]} seeded from in.pending;
 * - every other func echoes state {}.
 * tests/oracle_gateway_enroll_flow.py drives the LIVE Python with the same
 * scripts and compares ordered trace + ret + state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/gateway_enroll_flow.c"

typedef struct {
    const json_t *in;
    ge2_trace_t *trace;
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
    if (sc && sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx;
        size_t n = json_len(sc);
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else if (sc) {
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
    json_set(e, "args", args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    ge2_trace_add(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)

static void ret_json(json_t *out, json_t *r) {
    json_set(out, "ret", r ? r : json_null());
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    ge2_trace_t trace = {NULL};
    json_t *counters = json_object();
    script_ctx_t ctx = {in, &trace, counters};
    ge2_seams_t seams = {&ctx, script_call};
    const ge2_seams_t *sp = &seams;
    json_t *state = json_object();
    json_t *cfg = NULL;    /* working config copy for the config-mutating funcs */
    bool pending_func = !strcmp(func, "record_multiplex_decision")
        || !strcmp(func, "publish_pending_multiplex_decision")
        || !strcmp(func, "log_multiplex_decision");
    if (pending_func) {
        /* seed _pending_decision from in.pending (default []) BEFORE the call */
        const json_t *seed = json_obj_get(in, "pending");
        json_t *pend = ge2_is_arr(seed) ? json_copy(seed) : json_array();
        json_set(state, "pending", pend);
    }

    if (!strcmp(func, "fail")) {
        ret_json(out, ge2_flow_fail(J("lines"), sp));
    } else if (!strcmp(func, "explicit_multiplex_flag")) {
        ret_json(out, ge2_flow_explicit_multiplex_flag(S("default_home"), sp));
    } else if (!strcmp(func, "settled_explicit_flag")) {
        ret_json(out, ge2_flow_settled_explicit_flag(J("value")));
    } else if (!strcmp(func, "host_serves_named_profile")) {
        ret_json(out, ge2_flow_host_serves_named_profile(S("default_home"), S("home"), sp));
    } else if (!strcmp(func, "default_gateway_multiplexes")) {
        ret_json(out, ge2_flow_default_gateway_multiplexes(S("default_home"), sp));
    } else if (!strcmp(func, "standalone_launcher")) {
        ret_json(out, ge2_flow_standalone_launcher(sp));
    } else if (!strcmp(func, "standalone_launcher_decision")) {
        cfg = json_copy(J("config"));   /* NULL stays NULL (None carrier) */
        ret_json(out, ge2_flow_standalone_launcher_decision(cfg, sp));
    } else if (!strcmp(func, "implicit_multiplex_blocker")) {
        ret_json(out, ge2_flow_implicit_multiplex_blocker(sp));
    } else if (!strcmp(func, "default_profile_home")) {
        ret_json(out, ge2_flow_default_profile_home(sp));
    } else if (!strcmp(func, "persist_resolved_default")) {
        ret_json(out, ge2_flow_persist_resolved_default(J("decision"), S("default_home"), sp));
    } else if (!strcmp(func, "retired_opt_out_notice_lines")) {
        ret_json(out, ge2_flow_retired_opt_out_notice_lines());
    } else if (!strcmp(func, "consume_rewritten_notice")) {
        ret_json(out, ge2_flow_consume_rewritten_notice(S("default_home"), sp));
    } else if (!strcmp(func, "box")) {
        ret_json(out, ge2_flow_box(J("body")));
    } else if (!strcmp(func, "resolve_multiplex_mode")) {
        cfg = json_copy(J("config"));
        ret_json(out, ge2_flow_resolve_multiplex_mode(cfg, sp));
    } else if (!strcmp(func, "record_multiplex_decision")) {
        pending_func = true;
        ret_json(out, ge2_flow_record_multiplex_decision(J("decision"), state, sp));
    } else if (!strcmp(func, "publish_pending_multiplex_decision")) {
        pending_func = true;
        ret_json(out, ge2_flow_publish_pending_multiplex_decision(state, sp));
    } else if (!strcmp(func, "log_multiplex_decision")) {
        pending_func = true;
        ret_json(out, ge2_flow_log_multiplex_decision(J("decision"), state, sp));
    } else if (!strcmp(func, "unserved_profiles")) {
        ret_json(out, ge2_flow_unserved_profiles(sp));
    } else if (!strcmp(func, "standalone_warning_lines")) {
        ret_json(out, ge2_flow_standalone_warning_lines(J("decision"), J("unserved"), sp));
    } else if (!strcmp(func, "recorded_standalone_reason")) {
        ret_json(out, ge2_flow_recorded_standalone_reason(S("state_path"), sp));
    } else if (!strcmp(func, "recorded_standalone_warning_lines")) {
        ret_json(out, ge2_flow_recorded_standalone_warning_lines(sp));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
        ret_json(out, NULL);
    }

    if (cfg) { json_free(state); state = cfg; }
    /* json_set ignores a NULL val — a null config state must emit JSON null. */
    json_set(out, "state", state ? state : json_null());
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
        /* Reserve envelope key positions: the oracle's want is built
         * {ret, trace, state} and canon() treats insertion order as semantic. */
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