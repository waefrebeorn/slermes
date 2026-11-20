/*
 * t_port_moa_loop_flow3.c — oracle harness for the agent/moa_loop.py
 * REMAINING-unclaimed lifecycle FLOW ported in src/agent/moa_loop_flow3.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * One universal seam stub consumes the case's "io" scripts (absent/null ->
 * None; {"seq": [...]} -> per-call sequence, None when exhausted; any other
 * value -> reused), records one {ev, args, ret} event per call in call order
 * (ret = the RAW scripted value, raise-specs included), and emits
 * {"func": ..., "in": ..., "out": {"ret","raise","trace","state"}} per case.
 * tests/oracle_moa_loop_flow3.py drives the LIVE Python with the same scripts
 * and diffs the ordered trace + ret + raise + state window.
 *
 * State windows: MoAChatCompletions.* funcs carry the facade attribute window
 * in state (the self SimpleNamespace double); MoAClient.* funcs carry
 * {"chat": {"completions": <facade window>}}. Both __init__ funcs build a
 * FRESH window (in.state ignored — live constructs a new object). The
 * unobservable _accounting_lock and the bound-method keys are stripped, a
 * CanonicalUsage-tagged _pending_reference_usage projects to its asdict field
 * dict, tagged _RefAccounting row elements fill their field set, and the
 * _merge_same_role_destinations set array sorts — mirroring the oracle's
 * materialize -> norm round trip (partial mutations included on a raise).
 *
 * Gate: bash tests/run_one_oracle.sh moa_loop_flow3 \
 *           oracle_moa_loop_flow3.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "agent/moa_loop_flow3.c"

typedef struct {
    const json_t *in;
    json_t *trace;    /* owned JSON array of events */
    json_t *counts;   /* owned object: seam name -> call count */
} ml3_script_ctx_t;

static json_t *ml3_script_next(ml3_script_ctx_t *c, const char *name) {
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

static json_t *ml3_stub_call(void *ctx, const char *name, const json_t *args) {
    ml3_script_ctx_t *c = (ml3_script_ctx_t *)ctx;
    json_t *r = ml3_script_next(c, name);
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args ? json_copy(args) : json_object());
    json_set(e, "ret", r ? json_copy(r) : json_null());
    json_append(c->trace, e);
    return r;   /* ownership transfers to ml3_call */
}

typedef json_t *(*ml3_fn_t)(const json_t *, const ml3_io_t *, json_t *, json_t **);
typedef struct { const char *func; ml3_fn_t fn; } ml3_dispatch_t;

static const ml3_dispatch_t ML3_DISPATCH[] = {
    { "MoAChatCompletions.__init__", ml3_mcc_init },
    { "MoAChatCompletions.consume_reference_usage", ml3_mcc_consume_reference_usage },
    { "MoAChatCompletions.last_reference_metrics", ml3_mcc_last_reference_metrics },
    { "MoAChatCompletions._record_late_reference_accounting", ml3_mcc_record_late },
    { "MoAChatCompletions.consume_and_save_trace", ml3_mcc_consume_and_save_trace },
    { "MoAChatCompletions._emit", ml3_mcc_emit },
    { "MoAChatCompletions.prepare", ml3_mcc_prepare },
    { "MoAChatCompletions.rebase_prepared_request", ml3_mcc_rebase },
    { "MoAChatCompletions.create", ml3_mcc_create },
    { "MoAChatCompletions._call_prepared_aggregator", ml3_mcc_call_prepared },
    { "MoAClient.__init__", ml3_client_init },
    { "MoAClient.consume_reference_usage", ml3_client_consume_reference_usage },
    { "MoAClient.last_aggregator_slot", ml3_client_last_aggregator_slot },
    { "MoAClient.consume_and_save_trace", ml3_client_consume_and_save_trace },
    { "MoAClient.last_reference_metrics", ml3_client_last_reference_metrics },
};
#define ML3_DISPATCH_N (sizeof(ML3_DISPATCH) / sizeof(ML3_DISPATCH[0]))

static bool ml3_is_init_func(const char *func) {
    return strcmp(func, "MoAChatCompletions.__init__") == 0
        || strcmp(func, "MoAClient.__init__") == 0;
}
static bool ml3_is_client_func(const char *func) {
    return strncmp(func, "MoAClient.", 10) == 0;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    ml3_script_ctx_t ctx = { in, json_array(), json_object() };
    ml3_io_t io = { &ctx, ml3_stub_call };
    json_t *state;
    if (ml3_is_init_func(func)) {
        state = json_object();   /* live constructs a FRESH object */
    } else {
        const json_t *sj = json_obj_get(in, "state");
        state = (sj && sj->type == JSON_OBJECT) ? json_copy(sj) : json_object();
    }

    json_t *exc = NULL;
    json_t *ret = NULL;
    ml3_fn_t fn = NULL;
    for (size_t i = 0; i < ML3_DISPATCH_N; i++) {
        if (strcmp(ML3_DISPATCH[i].func, func) == 0) { fn = ML3_DISPATCH[i].fn; break; }
    }
    if (fn) {
        ret = fn(in, &io, state, &exc);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    /* State-window normalization (the oracle's materialize->norm round trip). */
    if (ml3_is_client_func(func)) {
        json_t *chat = json_obj_get(state, "chat");
        if (ml3_is_obj(chat)) {
            json_t *comp = json_obj_get(chat, "completions");
            if (ml3_is_obj(comp) && !json_obj_get(comp, "__attr__"))
                ml3_norm_facade(comp);
        }
    } else {
        ml3_norm_facade(state);
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
