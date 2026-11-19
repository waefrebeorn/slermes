/*
 * t_port_tool_result_processing_flow.c — oracle harness for the
 * agent/tool_executor.py tool-result-processing FLOW cores + leaves ported in
 * src/agent/tool_result_processing_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * One universal seam stub consumes the case's "io" scripts (absent/null ->
 * None; {"seq": [...]} -> per-call sequence, None when exhausted; any other
 * value -> reused), records one {ev, args, ret} event per call in call order
 * (ret = the RAW scripted value, raise-specs included), and emits
 * {"func": ..., "in": ..., "out": {"ret","raise","trace","state"}} per case.
 * tests/oracle_tool_result_processing_flow.py drives the LIVE Python with the
 * same scripts and diffs the ordered trace + ret + raise + state window.
 *
 * The "body" seam models the with-body of the _registered_tool_worker
 * contextmanager (the port calls it at the yield point); the "fn" seam models
 * the callable passed to _run_with_activity_heartbeat. Both are plain script
 * consumers here — the port itself fires them in live order.
 *
 * Gate: bash tests/run_one_oracle.sh tool_result_processing_flow \
 *           oracle_tool_result_processing_flow.py "" "lib/libjson/json.o"
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "agent/tool_result_processing_flow.c"

typedef struct {
    const json_t *in;
    json_t *state;    /* owned mutable facade state (borrowed by the stubs) */
    json_t *trace;    /* owned JSON array of events */
    json_t *counts;   /* owned object: seam name -> call count */
} trp_script_ctx_t;

static json_t *trp_script_next(trp_script_ctx_t *c, const char *name) {
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

static json_t *trp_stub_call(void *ctx, const char *name, const json_t *args) {
    trp_script_ctx_t *c = (trp_script_ctx_t *)ctx;
    json_t *r = trp_script_next(c, name);
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args ? json_copy(args) : json_object());
    json_set(e, "ret", r ? json_copy(r) : json_null());
    json_append(c->trace, e);
    return r;   /* ownership transfers to trp_call */
}

typedef json_t *(*trp_fn_t)(const json_t *, const trp_io_t *, json_t *, json_t **);
typedef struct { const char *func; trp_fn_t fn; } trp_dispatch_t;

static const trp_dispatch_t TRP_DISPATCH[] = {
    { "_record_persisted_path_for_stub", trp_record_persisted_path_for_stub },
    { "_canonical_tool_name", trp_canonical_tool_name },
    { "_append_skipped_tool_results", trp_append_skipped_tool_results },
    { "_commit_tool_result", trp_commit_tool_result },
    { "_finalize_tool_batch", trp_finalize_tool_batch },
    { "_print_tool_completed", trp_print_tool_completed },
    { "_append_batch_results", trp_append_batch_results },
    { "_start_quiet_tool_spinner", trp_start_quiet_tool_spinner },
    { "_finish_quiet_tool_spinner", trp_finish_quiet_tool_spinner },
    { "_resolve_sequential_dispatch", trp_resolve_sequential_dispatch },
    { "_skip_remaining_sequential", trp_skip_remaining_sequential },
    { "_append_invalid_arguments_result", trp_append_invalid_arguments_result },
    { "_run_sequential_call", trp_run_sequential_call },
    { "_publish_sequential_result", trp_publish_sequential_result },
    { "_execute_tool_calls_sequential", trp_execute_tool_calls_sequential },
    { "middleware_kwargs", trp_middleware_kwargs },
    { "emit_post", trp_emit_post },
    { "emit_cancelled", trp_emit_cancelled },
    { "emit_invalid_arguments", trp_emit_invalid_arguments },
    { "ref", trp_parsed_call_ref },
    { "_set_worker_activity_callback", trp_set_worker_activity_callback },
    { "_run_tool_activity_heartbeat", trp_run_tool_activity_heartbeat },
    { "_registered_tool_worker", trp_registered_tool_worker },
    { "_run_with_activity_heartbeat", trp_run_with_activity_heartbeat },
};
#define TRP_DISPATCH_N (sizeof(TRP_DISPATCH) / sizeof(TRP_DISPATCH[0]))

static void run_case(const char *func, const json_t *in, json_t *out) {
    trp_script_ctx_t ctx = { in, NULL, json_array(), json_object() };
    const json_t *sj = json_obj_get(in, "state");
    ctx.state = (sj && sj->type == JSON_OBJECT) ? json_copy(sj) : json_object();
    trp_io_t io = { &ctx, trp_stub_call };
    json_t *exc = NULL;
    json_t *ret = NULL;

    trp_fn_t fn = NULL;
    for (size_t i = 0; i < TRP_DISPATCH_N; i++) {
        if (strcmp(TRP_DISPATCH[i].func, func) == 0) { fn = TRP_DISPATCH[i].fn; break; }
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
    char line[1 << 23];   /* large-result commit case carries a 1M-char string */
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