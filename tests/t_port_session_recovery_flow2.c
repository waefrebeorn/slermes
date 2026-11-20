/*
 * t_port_session_recovery_flow2.c — oracle harness for
 * src/cli/session_recovery_flow2.c (session_recovery.py remainder FLOW port).
 *
 * Self-contained: includes the port .c by area-qualified path. Reads JSON-line
 * cases from stdin (tests/fixtures/session_recovery_flow2.jsonl), wires the
 * universal seam to the SCRIPTED results in each case's "seams" object (one
 * trace event per call, in call order), and emits
 *   {"func":..., "in":..., "out":{"ret":..., "trace":[...], "state":...}}
 *
 * "state" is the post-hoc mutated carrier (result / verification) for the four
 * mutation flows (compatible_columns, verify_fts_indexes, verify_structure,
 * recovery_report) — captured even on raise; JSON null otherwise.
 *
 * FAIL-CLOSED script semantics (identical to the oracle): a MISSING script is a
 * RuntimeError raise spec; a scripted JSON null is NULL (None), never a
 * json_null node; an ARRAY script is a per-call queue (last repeats, empty ->
 * None); {"__raise__": cls, "msg": ...} scripts an exception. A fetchall seam
 * (table_columns / integrity_check / foreign_key_check / fts_meta) that scripts
 * null yields an empty row list; a fetchone seam (count_rows / journal_mode /
 * reconcile_count / schema_version) that scripts null yields a None row.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/session_recovery_flow2.c"

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
        char *m = sr2_xsf("no script for seam: %s", name);
        result = sr2_raise("RuntimeError", m);
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
    if (result && result->type == JSON_NULL) { json_free(result); result = NULL; }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    if (!ctx->trace) ctx->trace = json_array();
    json_append(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)
#define B(k) json_get_bool(in, k, false)

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, NULL, json_object()};
    sr2_seams_t seams = {&ctx, script_call};
    const sr2_seams_t *sp = &seams;
    json_t *r = NULL;
    json_t *state = NULL;

    if (!strcmp(func, "count_rows")) {
        r = sr2_count_rows(sp, S("table"));
    } else if (!strcmp(func, "quoted_columns")) {
        r = sr2_quoted_columns(J("columns"));
    } else if (!strcmp(func, "journal_mode")) {
        r = sr2_journal_mode(sp);
    } else if (!strcmp(func, "compatible_columns")) {
        json_t *carrier = json_copy(J("result"));
        r = sr2_compatible_columns(sp, S("table"), carrier);
        state = carrier;
    } else if (!strcmp(func, "state_meta_result")) {
        r = sr2_state_meta_result(J("source_rows"), J("extra"));
    } else if (!strcmp(func, "state_meta_precheck")) {
        r = sr2_state_meta_precheck(sp, J("source_rows"), B("salvage"));
    } else if (!strcmp(func, "reconcile")) {
        r = sr2_reconcile(sp, S("table"), S("where"), S("mutation"));
    } else if (!strcmp(func, "immediate_transaction")) {
        r = sr2_immediate_transaction(sp, J("outcome"));
    } else if (!strcmp(func, "verify_fts_indexes")) {
        json_t *v = json_copy(J("verification"));
        r = sr2_verify_fts_indexes(sp, v);
        state = v;
    } else if (!strcmp(func, "verify_structure")) {
        json_t *v = json_copy(J("verification"));
        r = sr2_verify_structure(sp, v);
        state = v;
    } else if (!strcmp(func, "sanitize_session_model_config")) {
        r = sr2_sanitize_session_model_config(sp);
    } else if (!strcmp(func, "recovery_report")) {
        json_t *v = json_copy(J("verification"));
        r = sr2_recovery_report(sp, S("source"), S("output"), J("inspection"),
                                J("disk_space"), v, S("on_source_change"), J("fields"));
        state = v;
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    /* json_set takes ownership: do NOT free r / state / trace after setting. */
    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", ctx.trace ? ctx.trace : json_array());
    json_set(out, "state", state ? state : json_null());
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
        /* Reserve envelope key positions: canon() treats insertion order as semantic. */
        json_set(out, "ret", json_null());
        json_set(out, "trace", json_array());
        json_set(out, "state", json_null());
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
