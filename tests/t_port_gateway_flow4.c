/*
 * t_port_gateway_flow4.c -- oracle harness for the UNCLAIMED-remainder FLOW
 * cores of hermes_cli/gateway.py ported in src/cli/gateway_flow4.c.
 *
 * Self-contained: includes the port .c by area-qualified name. Reads JSON-line
 * cases from stdin (tests/fixtures/gateway_flow4.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 *
 * FAIL-CLOSED: a seam key MISSING from the script delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present JSON
 * null is a valid None result; a JSON array script is one result PER CALL
 * (last repeats, empty array -> None).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli/gateway_flow4.c"

typedef struct {
    const json_t *in;
    gf4_trace_t *trace;
    json_t *counters;
} gf4_script_ctx_t;

static json_t *gf4_args_drop_null(const json_t *args) {
    json_t *out = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (v && v->type == JSON_NULL) continue;
            json_set(out, args->c.keys[i], json_copy(v));
        }
    }
    return out;
}

static json_t *gf4_script_call(void *vctx, const char *name, const json_t *args) {
    gf4_script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (!sc) {
        char *msg = gf4_xsf("unscripted seam: %s", name);
        result = gf4_raise_spec("RuntimeError", msg);
        free(msg);
    } else if (sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx;
        size_t n = json_len(sc);
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else {
        result = json_copy(sc);
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", gf4_args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    gf4_trace_add(ctx->trace, e);
    return result;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    gf4_trace_t trace = {NULL};
    json_t *counters = json_object();
    gf4_script_ctx_t ctx = {in, &trace, counters};
    gf4_seams_t seams = {&ctx, gf4_script_call};
    const gf4_seams_t *sp = &seams;
    json_t *ret = NULL;

    if (!strcmp(func, "cmd_run")) {
        ret = gf4_cmd_run(in, sp);
    } else if (!strcmp(func, "wait_for_api_server_port_free")) {
        ret = gf4_wait_for_api_server_port_free(in, sp);
    } else if (!strcmp(func, "pm_managed_node_dirs")) {
        ret = gf4_pm_managed_node_dirs(in, sp);
    } else if (!strcmp(func, "native_service_homes")) {
        ret = gf4_native_service_homes(in, sp);
    } else if (!strcmp(func, "windows_process_listing")) {
        ret = gf4_windows_process_listing(in, sp);
    } else if (!strcmp(func, "s6_gateway_snapshot")) {
        ret = gf4_s6_gateway_snapshot(in, sp);
    } else if (!strcmp(func, "reaper_exclusion_pids")) {
        ret = gf4_reaper_exclusion_pids(in, sp);
    } else if (!strcmp(func, "apply_startup_watchdog_config")) {
        ret = gf4_apply_startup_watchdog_config(in, sp);
    } else if (!strcmp(func, "exit_diag")) {
        ret = gf4_exit_diag(in, sp);
    } else if (!strcmp(func, "respawn_storm_backoff")) {
        ret = gf4_respawn_storm_backoff(in, sp);
    } else if (!strcmp(func, "install_systemd_from_cli")) {
        ret = gf4_install_systemd_from_cli(in, sp);
    } else if (!strcmp(func, "legacy_launchd_labels_for_install")) {
        ret = gf4_legacy_launchd_labels_for_install(in, sp);
    } else if (!strcmp(func, "prepare_service_launcher")) {
        ret = gf4_prepare_service_launcher(in, sp);
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", trace.events ? json_copy(trace.events) : json_array());
    gf4_trace_free(&trace);
    json_free(counters);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
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
        run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
        char *func_s = json_escape_string(func);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out);
        json_free(c);
    }
    free(line);
    return 0;
}
