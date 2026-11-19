/*
 * t_port_gateway_windows_flow2.c — oracle harness for the UNCLAIMED-remainder
 * FLOW cores of hermes_cli/gateway_windows.py ported in
 * src/cli/gateway_windows_flow2.c.
 *
 * Self-contained: includes the port .c by area-qualified name. Reads JSON-line
 * cases from stdin (tests/fixtures/gateway_windows_flow2.jsonl), wires the
 * universal seam to the SCRIPTED results in each case's "seams" object (a
 * recording stub appending one trace event per seam call, in call order), and
 * emits {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 *
 * FAIL-CLOSED: a seam key MISSING from the script delivers
 * {"__raise__":"RuntimeError","msg":"unscripted seam: <name>"}; a present JSON
 * null is a valid None result; a JSON array script is one result PER CALL
 * (last repeats, empty array -> None). tests/oracle_gateway_windows_flow2.py
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

#include "cli/gateway_windows_flow2.c"

typedef struct {
    const json_t *in;
    gw2_trace_t *trace;
    json_t *counters;
} gw2_script_ctx_t;

static json_t *gw2_args_drop_null(const json_t *args) {
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

static json_t *gw2_script_call(void *vctx, const char *name, const json_t *args) {
    gw2_script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (!sc) {
        /* fail-closed: an unscripted seam raises RuntimeError on both sides */
        char *msg = gw2_xsf("unscripted seam: %s", name);
        result = gw2_raise_spec("RuntimeError", msg);
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
    json_set(e, "args", gw2_args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    gw2_trace_add(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)

static void run_case(const char *func, const json_t *in, json_t *out) {
    gw2_trace_t trace = {NULL};
    json_t *counters = json_object();
    gw2_script_ctx_t ctx = {in, &trace, counters};
    gw2_seams_t seams = {&ctx, gw2_script_call};
    const gw2_seams_t *sp = &seams;
    json_t *ret = NULL;
    json_t *tmpnode = NULL;  /* defaulted input node owned by the harness */

    if (!strcmp(func, "attested_pids_from")) {
        ret = gw2_attested_pids_from(J("data"));
    } else if (!strcmp(func, "attestation_generation")) {
        ret = gw2_attestation_generation(J("data"));
    } else if (!strcmp(func, "attested_create_time")) {
        ret = gw2_attested_create_time(J("data"), json_get_num(in, "pid", 0));
    } else if (!strcmp(func, "read_start_attestation")) {
        ret = gw2_read_start_attestation(S("home"), sp);
    } else if (!strcmp(func, "clear_start_attestation")) {
        ret = gw2_clear_start_attestation(S("home"), sp);
    } else if (!strcmp(func, "attestation_within_horizon")) {
        ret = gw2_attestation_within_horizon(J("data"), sp);
    } else if (!strcmp(func, "write_start_attestation")) {
        ret = gw2_write_start_attestation(J("pids"), json_get_str(in, "via", ""), S("home"), sp);
    } else if (!strcmp(func, "check_start_attestation")) {
        ret = gw2_check_start_attestation(J("current_pids"), sp);
    } else if (!strcmp(func, "format_attestation_warning")) {
        ret = gw2_format_attestation_warning(J("attested"), J("data"), sp);
    } else if (!strcmp(func, "task_run_hint")) {
        ret = gw2_task_run_hint(json_get_str(in, "fmt", ""), sp);
    } else if (!strcmp(func, "print_task_run_hint")) {
        ret = gw2_print_task_run_hint(json_get_str(in, "fmt", ""), sp);
    } else if (!strcmp(func, "print_start_attestation_warning")) {
        ret = gw2_print_start_attestation_warning(sp);
    } else if (!strcmp(func, "query_scheduled_task_xml")) {
        ret = gw2_query_scheduled_task_xml(json_get_str(in, "task_name", ""), sp);
    } else if (!strcmp(func, "task_xml_leaf_values")) {
        const json_t *xml = J("xml");
        if (!xml) { tmpnode = json_string(""); xml = tmpnode; }
        ret = gw2_task_xml_leaf_values(xml);
    } else if (!strcmp(func, "compare_scheduled_task_drift")) {
        json_t *rx = json_string(json_get_str(in, "registered_xml", ""));
        json_t *tx = json_string(json_get_str(in, "template_xml", ""));
        ret = gw2_compare_scheduled_task_drift(rx, tx);
        json_free(rx);
        json_free(tx);
    } else if (!strcmp(func, "scheduled_task_drift")) {
        ret = gw2_scheduled_task_drift(json_get_str(in, "task_name", ""), sp);
    } else if (!strcmp(func, "print_scheduled_task_drift")) {
        ret = gw2_print_scheduled_task_drift(json_get_str(in, "task_name", ""), sp);
    } else if (!strcmp(func, "registered_task_launchers")) {
        ret = gw2_registered_task_launchers(json_get_str(in, "platform", "win32"), sp);
    } else if (!strcmp(func, "launcher_owned_exit_line")) {
        ret = gw2_launcher_owned_exit_line(json_get_str(in, "hermes_home", ""), sp);
    } else if (!strcmp(func, "launcher_pythonpath_entries")) {
        const json_t *extra = J("extra_pythonpath");
        if (!extra) { tmpnode = json_array(); extra = tmpnode; }
        ret = gw2_launcher_pythonpath_entries(extra, sp);
    } else if (!strcmp(func, "render_launchers")) {
        ret = gw2_render_launchers(J("settings"), J("script_path"), sp);
    } else if (!strcmp(func, "write_launcher_scripts")) {
        const json_t *scripts = J("scripts");
        if (!scripts) { tmpnode = json_array(); scripts = tmpnode; }
        ret = gw2_write_launcher_scripts(scripts, sp);
    } else if (!strcmp(func, "atomic_write")) {
        const json_t *content = J("content");
        if (!content) { tmpnode = json_string(""); content = tmpnode; }
        ret = gw2_atomic_write(json_get_str(in, "path", ""), content,
                               json_get_str(in, "tmp", ""),
                               json_get_str(in, "encoding", "utf-8"), sp);
    } else if (!strcmp(func, "report_already_running")) {
        const json_t *pids = J("pids");
        if (!pids) { tmpnode = json_array(); pids = tmpnode; }
        ret = gw2_report_already_running(pids, sp);
    } else if (!strcmp(func, "startup_staging_path")) {
        ret = gw2_startup_staging_path(sp);
    } else if (!strcmp(func, "hermes_home")) {
        ret = gw2_hermes_home(sp);
    } else if (!strcmp(func, "mark_planned_restart")) {
        ret = gw2_mark_planned_restart(sp);
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", trace.events ? json_copy(trace.events) : json_array());
    gw2_trace_free(&trace);
    json_free(counters);
    if (tmpnode) json_free(tmpnode);
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