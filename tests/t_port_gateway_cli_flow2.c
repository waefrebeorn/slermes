/*
 * t_port_gateway_cli_flow2.c — oracle harness for the hermes_cli/gateway.py
 * UNCLAIMED FLOW remainder ported in src/cli/gateway_cli_flow2.c.
 *
 * Self-contained: includes the port .c by area-qualified path. Reads JSON-line
 * cases from stdin (tests/fixtures/gateway_cli_flow2.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 *
 * FAIL-CLOSED script semantics (identical to the oracle): a MISSING seams[name]
 * key returns a RuntimeError raise spec {"__raise__":"RuntimeError","msg":
 * "no script for seam: <name>"} (recorded as the seam ret, then propagated by the
 * port); a scripted JSON null returns a NULL pointer (None), never a json_null
 * node; an ARRAY is a per-call queue (last value repeats); a single value is a
 * constant. {"__raise__": cls, "msg": ...} scripts an exception. The flows are
 * stateless; observable effects are the ordered seam trace + the return value.
 * tests/oracle_gateway_cli_flow2.py drives the LIVE Python with the same scripts
 * and compares ordered trace + ret.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/gateway_cli_flow2.c"

typedef struct {
    const json_t *in;
    gc2_trace_t *trace;
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
        /* FAIL-CLOSED: a missing script is a RuntimeError raise spec (recorded
         * as the seam ret, then propagated). A scripted null is a valid None. */
        result = gc2_raise("RuntimeError", gc2_xsf("no script for seam: %s", name));
    } else if (sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx;
        size_t n = json_len(sc);
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else {
        result = json_copy(sc);
    }
    /* Contract: NULL == None. A scripted null must NOT surface as a non-NULL
     * json_null node (truthiness / is-not-None guards would read it wrong). */
    if (result && result->type == JSON_NULL) {
        json_free(result);
        result = NULL;
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args_drop_null(args));   /* borrowed args: copy, do not free */
    json_set(e, "ret", result ? json_copy(result) : json_null());
    gc2_trace_add(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)
#define B(k, d) json_get_bool(in, k, d)
#define N(k, d) json_get_num(in, k, d)

static void ret_json(json_t *out, json_t *r) {
    json_set(out, "ret", r ? r : json_null());
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    gc2_trace_t trace = {NULL};
    json_t *counters = json_object();
    script_ctx_t ctx = {in, &trace, counters};
    gc2_seams_t seams = {&ctx, script_call};
    const gc2_seams_t *sp = &seams;

    if (!strcmp(func, "systemd_command")) {
        ret_json(out, gc2_systemd_command(J("argv")));
    } else if (!strcmp(func, "completed_process_detail")) {
        ret_json(out, gc2_completed_process_detail(J("result")));
    } else if (!strcmp(func, "hermes_home_pinned_by_unit")) {
        ret_json(out, gc2_hermes_home_pinned_by_unit(S("unit_path"), sp));
    } else if (!strcmp(func, "installed_unit_ld_library_path")) {
        ret_json(out, gc2_installed_unit_ld_library_path(B("system", false), sp));
    } else if (!strcmp(func, "ld_library_path_line")) {
        ret_json(out, gc2_ld_library_path_line(B("system", false), S("target_home_dir"), sp));
    } else if (!strcmp(func, "loginctl_enable_linger")) {
        ret_json(out, gc2_loginctl_enable_linger(S("username"), sp));
    } else if (!strcmp(func, "wait_for_target_user_bus")) {
        ret_json(out, gc2_wait_for_target_user_bus(N("uid", 0), N("timeout", 5.0), sp));
    } else if (!strcmp(func, "user_systemd_unit_dir")) {
        ret_json(out, gc2_user_systemd_unit_dir(sp));
    } else if (!strcmp(func, "user_runtime_dir")) {
        ret_json(out, gc2_user_runtime_dir(sp));
    } else if (!strcmp(func, "ensure_system_service_linger")) {
        ret_json(out, gc2_ensure_system_service_linger(S("username"), sp));
    } else if (!strcmp(func, "refuses_container_user_scope_install")) {
        ret_json(out, gc2_refuses_container_user_scope_install(B("system", false), sp));
    } else if (!strcmp(func, "is_apt_termux_install")) {
        ret_json(out, gc2_is_apt_termux_install(sp));
    } else if (!strcmp(func, "print_service_not_installed")) {
        ret_json(out, gc2_print_service_not_installed(B("system", false), sp));
    } else if (!strcmp(func, "print_runtime_health")) {
        ret_json(out, gc2_print_runtime_health(sp));
    } else if (!strcmp(func, "systemd_unit_belongs_to_current_home")) {
        ret_json(out, gc2_systemd_unit_belongs_to_current_home(B("system", false), sp));
    } else if (!strcmp(func, "bare_unit_pinned_home")) {
        ret_json(out, gc2_bare_unit_pinned_home(sp));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
        ret_json(out, NULL);
    }

    json_set(out, "trace", trace.events ? trace.events : json_array());
    json_free(counters);
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
        /* Reserve envelope key positions: the oracle's want is built {ret, trace}
         * and canon() treats insertion order as semantic. */
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