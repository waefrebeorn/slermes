/*
 * t_port_run_shutdown_flow.c — oracle harness for the gateway/run_shutdown.py
 * FLOW cores ported in src/gateway/run_shutdown_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * Wires each flow's seams to the SCRIPTED results in "in" (recording stubs,
 * one trace event per seam call in call order) and emits
 * {"func": ..., "in": ..., "out": {"ret": ..., "trace": [...], ...}} per case.
 * tests/oracle_run_shutdown_flow.py drives the LIVE Python with the same scripts.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "run_shutdown_flow.c"

typedef struct {
    const json_t *in;
    gflow_trace_t *trace;
} script_ctx_t;

static void rec(script_ctx_t *c, json_t *e) { gflow_trace_add(c->trace, e); }
static bool in_bool(const script_ctx_t *c, const char *k) {
    return json_get_bool(c->in, k, false);
}
static const char *in_str(const script_ctx_t *c, const char *k, const char *def) {
    return json_get_str(c->in, k, def);
}

/* ── seams ── */
static void sd_log(void *ctx, int level, const char *fmt, const json_t *args) {
    script_ctx_t *c = ctx;
    json_t *e = gflow_ev("log");
    gflow_evn(e, "level", level);
    gflow_evs(e, "fmt", fmt);
    gflow_evj(e, "args", args);
    rec(c, e);
}
static bool sd_failure_verdict(void *ctx, const json_t *runner) {
    script_ctx_t *c = ctx;
    (void)runner;
    bool r = in_bool(c, "failure_verdict");
    json_t *e = gflow_ev("exit_with_failure_verdict");
    gflow_evb(e, "ret", r);
    rec(c, e);
    return r;
}
static json_t *sd_quiet_fn(void *ctx, bool *raised, const char **err) {
    script_ctx_t *c = ctx;
    *raised = false; *err = NULL;
    if (in_bool(c, "step_raises")) {
        json_t *e = gflow_ev("fn");
        gflow_evb(e, "raised", true);
        rec(c, e);
        *raised = true;
        *err = in_str(c, "step_error", "");
        return NULL;
    }
    const json_t *r = json_obj_get(c->in, "step_ret");
    json_t *e = gflow_ev("fn");
    gflow_evj(e, "ret", r);
    rec(c, e);
    return (json_t *)r;
}
static void sd_update_status(void *ctx, const char *state, bool *raised) {
    script_ctx_t *c = ctx;
    bool r = in_bool(c, "status_raises");
    json_t *e = gflow_ev("update_runtime_status");
    gflow_evs(e, "state", state);
    gflow_evb(e, "raised", r);
    rec(c, e);
    *raised = r;
}

/* ── dispatch ── */
static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, NULL };
    gflow_trace_t trace = { NULL };
    ctx.trace = &trace;

    if (strcmp(func, "exit_with_failure_verdict") == 0) {
        gsd_failure_seams_t s = { .ctx = &ctx, .log = sd_log };
        bool r = gsd_flow_exit_with_failure_verdict(json_obj_get(in, "runner"), &s);
        json_set(out, "ret", json_bool(r));
    } else if (strcmp(func, "resolve_gateway_exit_verdict") == 0) {
        gsd_resolve_seams_t s = {
            .ctx = &ctx, .log = sd_log, .exit_with_failure_verdict = sd_failure_verdict,
        };
        gsd_exit_verdict_t r = gsd_flow_resolve_gateway_exit_verdict(
            json_obj_get(in, "runner"), in_bool(&ctx, "signal_initiated_shutdown"), &s);
        if (r.exited) {
            json_t *e = gflow_ev("system_exit");
            gflow_evn(e, "code", r.exit_code);
            rec(&ctx, e);
            json_set(out, "ret", json_null());
            json_set(out, "exit", json_number(r.exit_code));
        } else {
            json_set(out, "ret", json_bool(r.verdict));
        }
    } else if (strcmp(func, "notice_allowed") == 0) {
        gsd_notice_seams_t s = { .ctx = &ctx, .log = sd_log };
        bool r = gsd_flow_notice_allowed(
            json_obj_get(in, "config_platforms"), json_get_str(in, "platform_value", ""),
            json_get_str(in, "what", ""), json_obj_get(in, "platform_cfg"), &s);
        json_set(out, "ret", json_bool(r));
    } else if (strcmp(func, "shutdown_interrupt_reason") == 0) {
        const char *r = gsd_flow_shutdown_interrupt_reason(in_bool(&ctx, "restart_requested"));
        json_set(out, "ret", json_string(r));
    } else if (strcmp(func, "quiet_step") == 0) {
        gsd_quiet_seams_t s = { .ctx = &ctx, .fn = sd_quiet_fn, .log = sd_log };
        json_t *r = gsd_flow_quiet_step(json_get_str(in, "label", ""), &s);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "scale_to_zero_status") == 0) {
        gsd_stz_seams_t s = { .ctx = &ctx, .update_runtime_status = sd_update_status, .log = sd_log };
        gsd_flow_scale_to_zero_status(json_get_str(in, "state", ""),
                                      json_get_str(in, "fail_msg", ""), &s);
        json_set(out, "ret", json_null());
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
