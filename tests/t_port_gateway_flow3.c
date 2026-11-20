/*
 * t_port_gateway_flow3.c -- oracle harness for the UNCLAIMED-remainder FLOW
 * cores of hermes_cli/gateway.py ported in src/cli/gateway_flow3.c.
 *
 * Self-contained: includes the port .c by area-qualified name. Reads JSON-line
 * cases from stdin (tests/fixtures/gateway_flow3.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 *
 * FAIL-CLOSED: a seam key MISSING from the script delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present JSON
 * null is a valid None result; a JSON array script is one result PER CALL
 * (last repeats, empty array -> None). tests/oracle_gateway_flow3.py drives
 * the LIVE Python with the same scripts and compares the ordered decision
 * sequence + return value (raise envelopes included).
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli/gateway_flow3.c"

typedef struct {
    const json_t *in;
    gf3_trace_t *trace;
    json_t *counters;
} gf3_script_ctx_t;

static json_t *gf3_args_drop_null(const json_t *args) {
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

static json_t *gf3_script_call(void *vctx, const char *name, const json_t *args) {
    gf3_script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (!sc) {
        char *msg = gf3_xsf("unscripted seam: %s", name);
        result = gf3_raise_spec("RuntimeError", msg);
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
    json_set(e, "args", gf3_args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    gf3_trace_add(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)

static void run_case(const char *func, const json_t *in, json_t *out) {
    gf3_trace_t trace = {NULL};
    json_t *counters = json_object();
    gf3_script_ctx_t ctx = {in, &trace, counters};
    gf3_seams_t seams = {&ctx, gf3_script_call};
    const gf3_seams_t *sp = &seams;
    json_t *ret = NULL;

    if (!strcmp(func, "parse_ps_line")) {
        ret = gf3_parse_ps_line(in);
    } else if (!strcmp(func, "iter_windows_list_processes")) {
        ret = gf3_iter_windows_list_processes(in);
    } else if (!strcmp(func, "scm_service_field")) {
        ret = gf3_scm_service_field(in, sp);
    } else if (!strcmp(func, "gateway_detached_env")) {
        ret = gf3_gateway_detached_env(sp);
    } else if (!strcmp(func, "stdin_is_tty")) {
        ret = gf3_stdin_is_tty(in, sp);
    } else if (!strcmp(func, "home_owns_bare_service_name")) {
        ret = gf3_home_owns_bare_service_name(in, sp);
    } else if (!strcmp(func, "cmd_setup")) {
        ret = gf3_cmd_setup(sp);
    } else if (!strcmp(func, "cmd_list")) {
        ret = gf3_cmd_list(sp);
    } else if (!strcmp(func, "cmd_migrate_legacy")) {
        ret = gf3_cmd_migrate_legacy(in, sp);
    } else if (!strcmp(func, "print_unserved_shared_ingress")) {
        ret = gf3_print_unserved_shared_ingress(in, sp);
    } else if (!strcmp(func, "print_duplicate_credential_warnings")) {
        ret = gf3_print_duplicate_credential_warnings(sp);
    } else if (!strcmp(func, "absorb_windows_console_controls")) {
        ret = gf3_absorb_windows_console_controls(sp);
    } else if (!strcmp(func, "ping_loop_tick_witness")) {
        ret = gf3_ping_loop_tick_witness(in, sp);
    } else if (!strcmp(func, "iter_proc_cmdlines")) {
        ret = gf3_iter_proc_cmdlines(in, sp);
    } else if (!strcmp(func, "wait_for_tcp_port_free")) {
        ret = gf3_wait_for_tcp_port_free(in, sp);
    } else if (!strcmp(func, "restart_argv_is_host_gateway")) {
        ret = gf3_restart_argv_is_host_gateway(in, sp);
    } else if (!strcmp(func, "gw_windows")) {
        ret = gf3_gw_windows(sp);
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", trace.events ? json_copy(trace.events) : json_array());
    gf3_trace_free(&trace);
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
