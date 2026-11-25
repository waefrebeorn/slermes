/*
 * t_port_gateway_windows_flow3.c -- oracle harness for the UNCLAIMED-remainder
 * FLOW cores of hermes_cli/gateway_windows.py ported in
 * src/cli/gateway_windows_flow3.c (third slice: console encodings/decode,
 * service roots, launcher settings/scripts, dead-launcher refresh/drift,
 * stdio probes, deep status probes).
 *
 * Self-contained: includes the port .c by area-qualified name. Reads JSON-line
 * cases from stdin (tests/fixtures/gateway_windows_flow3.jsonl), wires the
 * universal seam to the SCRIPTED results in each case's "seams" object (a
 * recording stub appending one trace event per seam call, in call order), and
 * emits {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 *
 * FAIL-CLOSED: a seam key MISSING from the script delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present JSON
 * null is a valid None result; a JSON array script is one result PER CALL
 * (last repeats, empty array -> None). tests/oracle_gateway_windows_flow3.py
 * drives the LIVE Python with the same scripts and compares the ordered
 * decision sequence + return value (raise envelopes included).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/gateway_windows_flow3.c"

typedef struct {
    const json_t *in;
    gw3_trace_t *trace;
    json_t *counters;
} gw3_script_ctx_t;

static json_t *gw3_args_drop_null(const json_t *args) {
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

static json_t *gw3_script_call(void *vctx, const char *name, const json_t *args) {
    gw3_script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (!sc) {
        /* fail-closed: an unscripted seam raises RuntimeError on both sides */
        char *msg = gw3_xsf("unscripted seam: %s", name);
        result = gw3_raise_spec("RuntimeError", msg);
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
    json_set(e, "args", gw3_args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    gw3_trace_add(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)

static void run_case(const char *func, const json_t *in, json_t *out) {
    gw3_trace_t trace = {NULL};
    json_t *counters = json_object();
    gw3_script_ctx_t ctx = {in, &trace, counters};
    gw3_seams_t seams = {&ctx, gw3_script_call};
    const gw3_seams_t *sp = &seams;
    json_t *ret = NULL;

    if (!strcmp(func, "windows_console_encodings")) {
        ret = gw3_windows_console_encodings(J("kernel32"), sp);
    } else if (!strcmp(func, "decode_schtasks_output")) {
        ret = gw3_decode_schtasks_output(J("data"), J("kernel32"), sp);
    } else if (!strcmp(func, "hermes_service_roots")) {
        ret = gw3_hermes_service_roots(J("executable"), sp);
    } else if (!strcmp(func, "launcher_settings")) {
        ret = gw3_launcher_settings(S("home"), sp);
    } else if (!strcmp(func, "launcher_scripts")) {
        ret = gw3_launcher_scripts(sp);
    } else if (!strcmp(func, "refresh_dead_launcher")) {
        ret = gw3_refresh_dead_launcher(S("home"), json_get_str(in, "task_name", ""), sp);
    } else if (!strcmp(func, "launcher_script_drift")) {
        ret = gw3_launcher_script_drift(sp);
    } else if (!strcmp(func, "stdout_isatty")) {
        ret = gw3_stdout_isatty(J("stdout"), sp);
    } else if (!strcmp(func, "stdin_console_mode_ok")) {
        ret = gw3_stdin_console_mode_ok(json_get_str(in, "platform", "win32"), J("kernel32"), sp);
    } else if (!strcmp(func, "probe")) {
        ret = gw3_probe(json_get_num(in, "index", 0), gw3_jtruth(J("ok")),
                        json_get_str(in, "message", ""), sp);
    } else if (!strcmp(func, "probe_missing")) {
        ret = gw3_probe_missing(json_get_num(in, "index", 0),
                                json_get_str(in, "path", ""),
                                json_get_str(in, "label", ""), sp);
    } else if (!strcmp(func, "probe_pid_file")) {
        ret = gw3_probe_pid_file(json_get_str(in, "pid_path", ""), sp);
    } else if (!strcmp(func, "probe_lock_file")) {
        ret = gw3_probe_lock_file(json_get_str(in, "lock_path", ""), sp);
    } else if (!strcmp(func, "probe_running_pid")) {
        ret = gw3_probe_running_pid(sp);
    } else if (!strcmp(func, "probe_pid_exists")) {
        ret = gw3_probe_pid_exists(J("candidate_pid"), sp);
    } else if (!strcmp(func, "probe_state_file")) {
        ret = gw3_probe_state_file(json_get_str(in, "state_path", ""), sp);
    } else if (!strcmp(func, "probe_exit_diag")) {
        ret = gw3_probe_exit_diag(json_get_str(in, "diag_path", ""), sp);
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", trace.events ? json_copy(trace.events) : json_array());
    gw3_trace_free(&trace);
    json_free(counters);
}

int main(void) {
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
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