/*
 * t_port_tool_executor_flow.c — oracle harness for the agent/tool_executor.py
 * execution-decision FLOW cores + leaves ported in
 * src/agent/tool_executor_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * One universal seam stub consumes the case's "io" scripts (absent/null ->
 * None; {"seq": [...]} -> per-call sequence, None when exhausted; any other
 * value -> reused), records one {ev, args, ret} event per call in call order
 * (ret = the RAW scripted value, raise-specs included), and emits
 * {"func": ..., "in": ..., "out": {"ret","raise","trace","state"}} per case.
 * tests/oracle_tool_executor_flow.py drives the LIVE Python with the same
 * scripts and diffs the ordered trace + ret + raise + state window.
 *
 * Three seams fire their scripted callback AFTER recording, exactly as the
 * live closures run:
 *   gate.run                     -> _pre_tool_block(agent, ref)
 *   begin_execution (w/ callback)-> _begin_tool_execution(agent, ref, display_index)
 *   run_with_activity_heartbeat  -> execute(ref.args)
 * A scripted raise on the outer seam skips the callback (live semantics).
 *
 * Gate: bash tests/run_one_oracle.sh tool_executor_flow \
 *           oracle_tool_executor_flow.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "agent/tool_executor_flow.c"

typedef struct {
    const json_t *in;
    json_t *state;    /* owned mutable facade state (borrowed by the stubs) */
    json_t *trace;    /* owned JSON array of events */
    json_t *counts;   /* owned object: seam name -> call count */
} tef_script_ctx_t;

static json_t *tef_script_next(tef_script_ctx_t *c, const char *name) {
    const json_t *io = json_obj_get(c->in, "io");
    const json_t *s = io ? json_obj_get(io, name) : NULL;
    if (!s || s->type == JSON_NULL) return NULL;
    if (s->type == JSON_OBJECT) {
        const json_t *seq = json_obj_get(s, "seq");
        if (seq) {
            double n = json_get_num(c->counts, name, 0);
            json_set(c->counts, name, json_number(n + 1));
            if (seq->type == JSON_ARRAY && (size_t)n < seq->c.count)
                return json_copy(seq->c.items[(size_t)n]);
            return NULL;   /* exhausted -> None */
        }
    }
    return json_copy(s);
}

static bool tef_spec_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *tef_stub_call(void *ctx, const char *name, const json_t *args) {
    tef_script_ctx_t *c = (tef_script_ctx_t *)ctx;
    json_t *r = tef_script_next(c, name);
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args ? json_copy(args) : json_object());
    json_set(e, "ret", r ? json_copy(r) : json_null());
    json_append(c->trace, e);

    if (strcmp(name, "gate.run") == 0) {
        if (tef_spec_is_raise(r)) return r;
        json_free(r);
        /* resolve lambda: _pre_tool_block(agent, ref) */
        json_t *a = json_object();
        json_t *agent = json_obj_get(c->state, "agent");
        json_t *ref = json_obj_get(c->state, "ref");
        if (agent) json_set(a, "agent", json_copy(agent));
        if (ref) json_set(a, "ref", json_copy(ref));
        return tef_stub_call(ctx, "_pre_tool_block", a);
    }
    if (strcmp(name, "begin_execution") == 0) {
        if (tef_spec_is_raise(r)) return r;
        if (args && json_obj_get(args, "callback")) {
            json_free(r);
            /* callback lambda: _begin_tool_execution(agent, ref, display_index) */
            json_t *a = json_object();
            json_t *ref = json_obj_get(c->state, "ref");
            if (ref) json_set(a, "ref", json_copy(ref));
            const json_t *di = json_obj_get(c->in, "display_index");
            if (di && di->type != JSON_NULL) json_set(a, "display_index", json_copy(di));
            return tef_stub_call(ctx, "_begin_tool_execution", a);
        }
        return r;
    }
    if (strcmp(name, "run_with_activity_heartbeat") == 0) {
        if (tef_spec_is_raise(r)) return r;
        json_free(r);
        /* fn lambda: execute(ref.args) */
        json_t *a = json_object();
        json_t *ref = json_obj_get(c->state, "ref");
        json_t *ra = ref ? json_obj_get(ref, "args") : NULL;
        if (ra) json_set(a, "args", json_copy(ra));
        return tef_stub_call(ctx, "execute", a);
    }
    return r;   /* ownership transfers to tef_call */
}

typedef json_t *(*tef_fn_t)(const json_t *, const tef_io_t *, json_t *, json_t **);
typedef struct { const char *func; tef_fn_t fn; } tef_dispatch_t;

static const tef_dispatch_t TEF_DISPATCH[] = {
    { "_tc_name", tef_tc_name },
    { "_tool_progress_enabled", tef_tool_progress_enabled },
    { "_preview", tef_preview },
    { "_start_order_gate_timeout", tef_start_order_gate_timeout },
    { "_delegate_spinner_label", tef_delegate_spinner_label },
    { "_safe_callback", tef_safe_callback },
    { "_interrupt_worker_tids", tef_interrupt_worker_tids },
    { "_blocked_tool_result", tef_blocked_tool_result },
    { "_pre_tool_block", tef_pre_tool_block },
    { "_unwrap_tool_search_call", tef_unwrap_tool_search_call },
    { "_parse_tool_call", tef_parse_tool_call },
    { "_dispatch_authorized_once", tef_dispatch_authorized_once },
    { "_poll_sequential_future", tef_poll_sequential_future },
    { "_unfinished_tool_result", tef_unfinished_tool_result },
    { "_abandoned_sequential_result", tef_abandoned_sequential_result },
    { "_persist_multimodal_text_parts", tef_persist_multimodal_text_parts },
    { "_emit_tool_complete_and_risk", tef_emit_tool_complete_and_risk },
    { "advance", tef_worker_start_once_advance },
};
#define TEF_DISPATCH_N (sizeof(TEF_DISPATCH) / sizeof(TEF_DISPATCH[0]))

static void run_case(const char *func, const json_t *in, json_t *out) {
    tef_script_ctx_t ctx = { in, NULL, json_array(), json_object() };
    const json_t *sj = json_obj_get(in, "state");
    ctx.state = (sj && sj->type == JSON_OBJECT) ? json_copy(sj) : json_object();
    tef_io_t io = { &ctx, tef_stub_call };
    json_t *exc = NULL;
    json_t *ret = NULL;

    tef_fn_t fn = NULL;
    for (size_t i = 0; i < TEF_DISPATCH_N; i++) {
        if (strcmp(TEF_DISPATCH[i].func, func) == 0) { fn = TEF_DISPATCH[i].fn; break; }
    }
    if (fn) {
        ret = fn(in, &io, ctx.state, &exc);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "raise", exc ? exc : json_null());
    json_set(out, "trace", ctx.trace);
    json_set(out, "state", ctx.state);
    json_free(ctx.counts);
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