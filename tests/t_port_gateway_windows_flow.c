/*
 * t_port_gateway_windows_flow.c — oracle harness for the Windows gateway
 * service FLOW cores ported in src/cli/port_gateway_windows_flow.c.
 *
 * Self-contained: includes the port .c by bare name. Reads JSON-line cases
 * from stdin (tests/fixtures/gateway_windows_flow.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 * tests/oracle_gateway_windows_flow.py drives the LIVE Python with the same
 * scripts and compares the ordered decision sequence + return value.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "port_gateway_windows_flow.c"

typedef struct {
    const json_t *in;
    gww_trace_t *trace;
    json_t *counters;
} script_ctx_t;

static json_t *args_drop_null(const json_t *args) {
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

static json_t *script_call(void *vctx, const char *name, const json_t *args) {
    script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (sc && sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx;
        size_t n = json_len(sc);
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else if (sc) {
        result = json_copy(sc);
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    gww_trace_add(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)
#define B(k, d) json_get_bool(in, k, d)

static void ret_json(json_t *out, json_t *r) { json_set(out, "ret", r ? r : json_null()); }
static void ret_bool(json_t *out, bool b) { json_set(out, "ret", json_bool(b)); }
static void ret_str(json_t *out, char *s) {
    json_set(out, "ret", s ? json_string(s) : json_null());
    free(s);
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    gww_trace_t trace = {NULL};
    json_t *counters = json_object();
    script_ctx_t ctx = {in, &trace, counters};
    gww_seams_t seams = {&ctx, script_call};
    const gww_seams_t *sp = &seams;

    if (!strcmp(func, "start_or_report_running")) {
        gww_start_or_report_running(J("running_pids"), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "offer_elevated_install")) {
        ret_bool(out, gww_offer_elevated_install(S("headline"), B("force", false),
                                                 B("start_now", false), B("start_on_login", false), sp));
    } else if (!strcmp(func, "reconcile_autostart_launchers")) {
        ret_json(out, gww_reconcile_autostart_launchers(sp));
    } else if (!strcmp(func, "redundant_autostart_entries")) {
        ret_json(out, gww_redundant_autostart_entries(sp));
    } else if (!strcmp(func, "remove_startup_entries")) {
        ret_json(out, gww_remove_startup_entries(sp));
    } else if (!strcmp(func, "attested_death_generation")) {
        ret_str(out, gww_attested_death_generation(J("current_pids"), S("home"), sp));
    } else if (!strcmp(func, "attested_dead")) {
        ret_bool(out, gww_attested_dead(J("attested"), J("current_pids"), J("data"), S("home"), sp));
    } else if (!strcmp(func, "attested_pid_exited_cleanly")) {
        const json_t *ct = J("create_time");
        bool present = ct && ct->type == JSON_NUMBER;
        double ctv = present ? ct->num_val : 0.0;
        ret_bool(out, gww_attested_pid_exited_cleanly((int)json_get_num(in, "pid", 0),
                                                      present, ctv, S("home"), sp));
    } else if (!strcmp(func, "consume_start_attestation")) {
        gww_consume_start_attestation(S("generation"), S("home"), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "reconcile_scheduled_task")) {
        ret_bool(out, gww_reconcile_scheduled_task(S("task_name"), sp));
    } else if (!strcmp(func, "live_gateway_pids")) {
        ret_json(out, gww_live_gateway_pids(B("all_profiles", false), S("home"),
                                            B("pid_filter_present", false), sp));
    } else if (!strcmp(func, "gateway_pid_identities")) {
        ret_json(out, gww_gateway_pid_identities(J("pids"), sp));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
        ret_json(out, NULL);
    }

    json_set(out, "trace", trace.events ? json_copy(trace.events) : json_array());
    gww_trace_free(&trace);
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