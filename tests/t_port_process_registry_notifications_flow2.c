/*
 * t_port_process_registry_notifications_flow2.c — oracle harness for the
 * tools/process_registry_notifications.py UNCLAIMED remainder ported in
 * src/tools/process_registry_notifications_flow2.c.
 *
 * Self-contained: includes the port .c by area-qualified path. Reads JSON-line
 * cases from stdin (tests/fixtures/process_registry_notifications_flow2.jsonl),
 * wires the universal seam to the SCRIPTED results in each case's "seams" object
 * (a recording stub appending one trace event per seam call, in call order), and
 * emits {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 *
 * FAIL-CLOSED script semantics (identical to the oracle): a MISSING script returns a
 * RuntimeError raise spec {"__raise__":"RuntimeError","msg":"no script for seam: <name>"};
 * a scripted JSON null returns a NULL pointer (None), never a json_null node; an ARRAY
 * script is a per-call queue (last value repeats, empty array -> None);
 * {"__raise__": cls, "msg": ...} scripts an exception. The observable effects are the
 * ordered seam trace + the return value.
 * tests/oracle_process_registry_notifications_flow2.py drives the LIVE Python with
 * the same scripts and compares ordered trace + ret.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tools/process_registry_notifications_flow2.c"

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
        /* FAIL-CLOSED: missing script -> RuntimeError raise spec. */
        char *m = prn2_xsf("no script for seam: %s", name);
        result = prn2_raise("RuntimeError", m);
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
    /* Contract: NULL == None. */
    if (result && result->type == JSON_NULL) { json_free(result); result = NULL; }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args_drop_null(args));   /* borrowed args: copy, do not free */
    json_set(e, "ret", result ? json_copy(result) : json_null());
    if (!ctx->trace) ctx->trace = json_array();
    json_append(ctx->trace, e);
    return result;
}

#define J(k) json_obj_get(in, k)

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, NULL, json_object()};
    prn2_seams_t seams = {&ctx, script_call};
    const prn2_seams_t *sp = &seams;
    json_t *r = NULL;

    if (!strcmp(func, "is_child_process_notification")) {
        r = prn2_is_child_process_notification(J("evt"));
    } else if (!strcmp(func, "should_surface_notification")) {
        r = prn2_should_surface_notification(sp, J("evt"), J("surface_child"));
    } else if (!strcmp(func, "is_truncated")) {
        r = prn2_is_truncated(J("entry"));
    } else if (!strcmp(func, "delegation_wakes_parent")) {
        r = prn2_delegation_wakes_parent(J("evt"));
    } else if (!strcmp(func, "completion_resumes_parent")) {
        r = prn2_completion_resumes_parent(J("evt"));
    } else if (!strcmp(func, "group_process_notifications")) {
        r = prn2_group_process_notifications(J("notifications"));
    } else if (!strcmp(func, "short_title")) {
        r = prn2_short_title(J("title"));
    } else if (!strcmp(func, "short_command")) {
        r = prn2_short_command(J("command"));
    } else if (!strcmp(func, "process_completion_display_text")) {
        r = prn2_process_completion_display_text(J("events"));
    } else if (!strcmp(func, "completion_status")) {
        r = prn2_completion_status(J("evt"));
    } else if (!strcmp(func, "heartbeat_output_tail")) {
        r = prn2_heartbeat_output_tail(J("output"), J("omitted"));
    } else if (!strcmp(func, "heartbeat_output_text")) {
        r = prn2_heartbeat_output_text(J("evt"));
    } else if (!strcmp(func, "join_heartbeat_outputs")) {
        r = prn2_join_heartbeat_outputs(J("earlier"), J("later"));
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
        /* Reserve envelope key positions: canon() treats insertion order as semantic. */
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
