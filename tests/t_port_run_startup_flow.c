/*
 * t_port_run_startup_flow.c — oracle harness for the gateway/run_startup.py
 * FLOW cores ported in src/gateway/run_startup_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * Wires each flow's seams to the SCRIPTED results in "in" (recording stubs,
 * one trace event per seam call in call order) and emits
 * {"func": ..., "in": ..., "out": {"ret": ..., "trace": [...], "state": ...}}.
 * tests/oracle_run_startup_flow.py drives the LIVE Python with the same scripts.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "run_startup_flow.c"

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
static void su_log(void *ctx, int level, const char *fmt, const json_t *args) {
    script_ctx_t *c = ctx;
    json_t *e = gflow_ev("log");
    gflow_evn(e, "level", level);
    gflow_evs(e, "fmt", fmt);
    gflow_evj(e, "args", args);
    rec(c, e);
}
static bool su_shutdown_set(void *ctx) {
    script_ctx_t *c = ctx;
    bool r = in_bool(c, "shutdown_event_set");
    json_t *e = gflow_ev("shutdown_event_is_set");
    gflow_evb(e, "ret", r);
    rec(c, e);
    return r;
}
static bool su_authorized(void *ctx, const json_t *source, bool *raised, const char **err) {
    script_ctx_t *c = ctx;
    (void)source;
    *raised = false; *err = NULL;
    if (in_bool(c, "authorize_raises")) {
        json_t *e = gflow_ev("is_user_authorized");
        gflow_evb(e, "raised", true);
        rec(c, e);
        *raised = true;
        *err = in_str(c, "authorize_error", "");
        return false;
    }
    bool r = in_bool(c, "authorized");
    json_t *e = gflow_ev("is_user_authorized");
    gflow_evb(e, "ret", r);
    rec(c, e);
    return r;
}
static bool su_conflict(void *ctx, const char *code) {
    script_ctx_t *c = ctx;
    bool r = in_bool(c, "conflict");
    json_t *e = gflow_ev("is_global_startup_conflict");
    gflow_evs(e, "code", code);
    gflow_evb(e, "ret", r);
    rec(c, e);
    return r;
}
static void su_update_status(void *ctx, const char *state) {
    script_ctx_t *c = ctx;
    json_t *e = gflow_ev("update_runtime_status");
    gflow_evs(e, "state", state);
    rec(c, e);
}
static void su_write_status(void *ctx, const char *state, const char *reason) {
    script_ctx_t *c = ctx;
    json_t *e = gflow_ev("write_runtime_status_quiet");
    gflow_evs(e, "gateway_state", state);
    gflow_evs(e, "exit_reason", reason);
    rec(c, e);
}
static void su_fail_fatal(void *ctx, const char *reason) {
    script_ctx_t *c = ctx;
    json_t *e = gflow_ev("startup_fail_fatal_config");
    gflow_evs(e, "reason", reason);
    rec(c, e);
}
static void su_clean_exit(void *ctx, const char *reason) {
    script_ctx_t *c = ctx;
    json_t *e = gflow_ev("request_clean_exit");
    gflow_evs(e, "reason", reason);
    rec(c, e);
}

/* ── dispatch ── */
static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, NULL };
    gflow_trace_t trace = { NULL };
    ctx.trace = &trace;
    const json_t *state = json_obj_get(in, "state");

    if (strcmp(func, "serving_state") == 0) {
        const char *r = gsu_flow_serving_state(state);
        json_set(out, "ret", json_string(r));
    } else if (strcmp(func, "startup_should_abort") == 0) {
        gsu_abort_seams_t s = { .ctx = &ctx, .shutdown_event_is_set = su_shutdown_set };
        bool r = gsu_flow_startup_should_abort(state, &s);
        json_set(out, "ret", json_bool(r));
    } else if (strcmp(func, "resume_owner_authorized") == 0) {
        gsu_resume_seams_t s = {
            .ctx = &ctx, .is_user_authorized = su_authorized, .log = su_log,
        };
        bool r = gsu_flow_resume_owner_authorized(
            json_get_str(in, "session_key", ""), json_obj_get(in, "source"), &s);
        json_set(out, "ret", json_bool(r));
    } else if (strcmp(func, "park_last_fatal_platform") == 0) {
        gsu_park_seams_t s = {
            .ctx = &ctx, .is_global_startup_conflict = su_conflict,
            .update_runtime_status = su_update_status, .log = su_log,
        };
        json_t *st = state ? json_copy(state) : json_object();
        bool r = gsu_flow_park_last_fatal_platform(st, json_obj_get(in, "adapter"), &s);
        json_set(out, "ret", json_bool(r));
        json_set(out, "state", st);
    } else if (strcmp(func, "start_handle_no_connections") == 0) {
        gsu_noconn_seams_t s = {
            .ctx = &ctx, .log = su_log, .write_runtime_status_quiet = su_write_status,
            .startup_fail_fatal_config = su_fail_fatal,
        };
        json_t *st = state ? json_copy(state) : json_object();
        bool r = gsu_flow_start_handle_no_connections(
            st, (int)json_get_num(in, "connected_count", 0),
            (int)json_get_num(in, "enabled_platform_count", 0),
            json_obj_get(in, "retryable"), json_obj_get(in, "nonretryable"), &s);
        json_set(out, "ret", json_bool(r));
        json_set(out, "state", st);
    } else if (strcmp(func, "startup_fail_fatal_config") == 0) {
        gsu_fatal_seams_t s = {
            .ctx = &ctx, .write_runtime_status_quiet = su_write_status,
            .request_clean_exit = su_clean_exit,
        };
        json_t *st = state ? json_copy(state) : json_object();
        gsu_flow_startup_fail_fatal_config(st, json_get_str(in, "reason", ""), &s);
        json_set(out, "ret", json_null());
        json_set(out, "state", st);
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
