/*
 * t_port_moa_loop_flow.c — oracle harness for the agent/moa_loop.py
 * fan-out / aggregation FLOW cores + leaves ported in
 * src/agent/moa_loop_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * One universal seam stub consumes the case's "io" scripts (absent/null ->
 * None; {"seq": [...]} -> per-call sequence, None when exhausted; any other
 * value -> reused), records one {ev, args, ret} event per call in call order
 * (ret = the RAW scripted value, raise-specs included), and emits
 * {"func": ..., "in": ..., "out": {"ret","raise","trace","state"}} per case.
 * tests/oracle_moa_loop_flow.py drives the LIVE Python with the same scripts
 * and diffs the ordered trace + ret + raise + facade state window.
 *
 * The _run_references_parallel seam additionally fires one progress_callback
 * per scripted "progress" entry (each a recursive _emit seam call, raises
 * suppressed exactly as the live collect() try/except) BEFORE returning the
 * materialized "outputs" — modelling the fan-out's mid-call progress emission.
 *
 * Gate: bash tests/run_one_oracle.sh moa_loop_flow \
 *           oracle_moa_loop_flow.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "moa_loop_flow.c"

typedef struct {
    const json_t *in;
    json_t *trace;    /* owned JSON array of events */
    json_t *counts;   /* owned object: seam name -> call count */
} mlf_script_ctx_t;

static json_t *mlf_script_next(mlf_script_ctx_t *c, const char *name) {
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

static json_t *mlf_stub_call(void *ctx, const char *name, const json_t *args) {
    mlf_script_ctx_t *c = (mlf_script_ctx_t *)ctx;
    json_t *r = mlf_script_next(c, name);
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args ? json_copy(args) : json_object());
    json_set(e, "ret", r ? json_copy(r) : json_null());
    json_append(c->trace, e);

    if (strcmp(name, "_run_references_parallel") == 0) {
        /* Fire the scripted progress callbacks (each invokes the _emit seam);
         * a raising progress callback is swallowed by the live collect(). */
        const json_t *prog = r ? json_obj_get(r, "progress") : NULL;
        if (prog && prog->type == JSON_ARRAY) {
            for (size_t i = 0; i < prog->c.count; i++) {
                const json_t *p = prog->c.items[i];
                json_t *ea = json_object();
                json_set(ea, "event", json_string("moa.progress"));
                const json_t *rd = json_obj_get(p, "refs_done");
                const json_t *rt = json_obj_get(p, "refs_total");
                const json_t *lb = json_obj_get(p, "label");
                if (rd) json_set(ea, "refs_done", json_copy(rd));
                if (rt) json_set(ea, "refs_total", json_copy(rt));
                if (lb) json_set(ea, "label", json_copy(lb));
                json_t *er = mlf_stub_call(ctx, "_emit", ea);
                json_free(ea);
                json_free(er);   /* suppress any scripted raise */
            }
        }
        const json_t *outs = r ? json_obj_get(r, "outputs") : NULL;
        json_t *ret = (outs && outs->type == JSON_ARRAY) ? json_copy(outs) : json_array();
        json_free(r);
        return ret;   /* materialized outputs; ownership transfers to mlf_call */
    }
    return r;   /* ownership transfers to mlf_call */
}

typedef json_t *(*mlf_fn_t)(const json_t *, const mlf_io_t *, json_t *, json_t **);
typedef struct { const char *func; mlf_fn_t fn; } mlf_dispatch_t;

static const mlf_dispatch_t MLF_DISPATCH[] = {
    { "_field", mlf_field },
    { "_slot_labels", mlf_slot_labels },
    { "_agent_cache_opts", mlf_agent_cache_opts },
    { "_with_cache_disabled", mlf_with_cache_disabled },
    { "_hash_messages", mlf_hash_messages },
    { "_tool_activity_since_last_user", mlf_tool_activity_since_last_user },
    { "_placeholder_output", mlf_placeholder_output },
    { "_join_reference_outputs", mlf_join_reference_outputs },
    { "_guidance_inputs", mlf_guidance_inputs },
    { "_sum_reference_accounting", mlf_sum_reference_accounting },
    { "_fold_pending_accounting", mlf_fold_pending_accounting },
    { "_reference_context_length", mlf_reference_context_length },
    { "_price_reference_response", mlf_price_reference_response },
    { "_record_late", mlf_record_late },
    { "_settle_interrupted", mlf_settle_interrupted },
    { "_fanout_cache_key", mlf_fanout_cache_key },
    { "_build_guidance", mlf_build_guidance },
    { "_run_fanout", mlf_run_fanout },
};
#define MLF_DISPATCH_N (sizeof(MLF_DISPATCH) / sizeof(MLF_DISPATCH[0]))

static void run_case(const char *func, const json_t *in, json_t *out) {
    mlf_script_ctx_t ctx = { in, json_array(), json_object() };
    mlf_io_t io = { &ctx, mlf_stub_call };
    const json_t *sj = json_obj_get(in, "state");
    json_t *state = (sj && sj->type == JSON_OBJECT) ? json_copy(sj) : json_object();
    json_t *exc = NULL;
    json_t *ret = NULL;

    mlf_fn_t fn = NULL;
    for (size_t i = 0; i < MLF_DISPATCH_N; i++) {
        if (strcmp(MLF_DISPATCH[i].func, func) == 0) { fn = MLF_DISPATCH[i].fn; break; }
    }
    if (fn) {
        ret = fn(in, &io, state, &exc);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "raise", exc ? exc : json_null());
    json_set(out, "trace", ctx.trace);
    json_set(out, "state", state);
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