/*
 * t_port_session_policy_flow.c — oracle harness for the gateway/session_policy.py
 * FLOW cores ported in src/gateway/session_policy_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * Wires each flow's seams to the SCRIPTED results in "in" (recording stubs that
 * append one trace event per seam call, in call order) and emits
 * {"func": ..., "in": ..., "out": {"ret": ..., "trace": [...], ...}} per case.
 * tests/oracle_session_policy_flow.py drives the LIVE Python with the same scripts.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "session_policy_flow.c"

typedef struct {
    const json_t *in;
    gflow_trace_t *trace;
} script_ctx_t;

static void rec(script_ctx_t *c, json_t *e) { gflow_trace_add(c->trace, e); }
static bool in_bool(const script_ctx_t *c, const char *k) {
    return json_get_bool(c->in, k, false);
}

/* ── restore_policy seams ── */
static bool sp_forward(void *ctx, const json_t *policy) {
    script_ctx_t *c = ctx;
    (void)policy;
    bool r = in_bool(c, "forward_policy");
    json_t *e = gflow_ev("is_forward_policy");
    gflow_evb(e, "ret", r);
    rec(c, e);
    return r;
}
static void sp_log(void *ctx, int level, const char *fmt, const json_t *args) {
    script_ctx_t *c = ctx;
    json_t *e = gflow_ev("log");
    gflow_evn(e, "level", level);
    gflow_evs(e, "fmt", fmt);
    gflow_evj(e, "args", args);
    rec(c, e);
}

static void emit_refusal(json_t *out, const gsp_refusal_t *r) {
    json_t *ro = json_object();
    json_set(ro, "reason", json_string(r->reason));
    json_set(ro, "field", r->field[0] ? json_string(r->field) : json_null());
    json_set(ro, "detail", r->detail[0] ? json_string(r->detail) : json_null());
    json_set(out, "refusal", ro);
    json_set(out, "ret", json_null());
}

/* ── dispatch ── */
static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, NULL };
    gflow_trace_t trace = { NULL };
    ctx.trace = &trace;

    if (strcmp(func, "session_pinned") == 0) {
        json_t *r = gsp_flow_session_pinned(
            json_obj_get(in, "config"), json_get_str(in, "field", ""),
            json_obj_get(in, "launched"));
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "created_surface") == 0) {
        char *r = gsp_flow_created_surface(json_obj_get(in, "label"));
        json_set(out, "ret", r ? json_string(r) : json_null());
        free(r);
    } else if (strcmp(func, "profile_target") == 0) {
        json_t *r = gsp_flow_profile_target(json_obj_get(in, "config"));
        json_set(out, "ret", r ? r : json_array());
    } else if (strcmp(func, "present_sections") == 0) {
        json_t *r = gsp_flow_present_sections(json_obj_get(in, "config"));
        json_set(out, "ret", r ? r : json_object());
    } else if (strcmp(func, "terminal_uses_host_cwd") == 0) {
        bool r = gsp_flow_terminal_uses_host_cwd(json_obj_get(in, "terminal"));
        json_set(out, "ret", json_bool(r));
    } else if (strcmp(func, "launch_skills") == 0) {
        gsp_refusal_t refusal = { false, {0}, {0}, {0} };
        json_t *r = gsp_flow_launch_skills(json_obj_get(in, "params"), &refusal);
        if (refusal.raised) {
            emit_refusal(out, &refusal);
            json_free(r);
        } else {
            json_set(out, "ret", r ? r : json_array());
        }
    } else if (strcmp(func, "valid_client_env") == 0) {
        bool kind_list = strcmp(json_get_str(in, "kind", "str"), "list") == 0;
        bool r = gsp_flow_valid_client_env(json_obj_get(in, "value"), kind_list);
        json_set(out, "ret", json_bool(r));
    } else if (strcmp(func, "stored_cwd_is_absolute") == 0) {
        bool r = gsp_flow_stored_cwd_is_absolute(json_get_str(in, "cwd", ""));
        json_set(out, "ret", json_bool(r));
    } else if (strcmp(func, "restore_policy") == 0) {
        gsp_restore_seams_t s = {
            .ctx = &ctx, .is_forward_policy = sp_forward, .log = sp_log,
        };
        gsp_refusal_t refusal = { false, {0}, {0}, {0} };
        json_t *r = gsp_flow_restore_policy(json_obj_get(in, "data"), &s, &refusal);
        if (refusal.raised) {
            emit_refusal(out, &refusal);
            json_free(r);
        } else {
            json_set(out, "ret", r ? r : json_null());
        }
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    json_set(out, "trace", trace.events ? trace.events : json_array());
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
        /* Serialize a string node for func (bounds-safe); never call
         * json_escape_string (off-by-one heap overflow in libjson). */
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
