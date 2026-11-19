/*
 * t_port_gateway_migrate_flow2.c — oracle harness for the
 * hermes_cli/gateway_migrate.py UNCLAIMED-remainder FLOW port in
 * src/cli/gateway_migrate_flow2.c.
 *
 * Self-contained: includes the port .c area-qualified. Reads JSON-line cases
 * from stdin (tests/fixtures/gateway_migrate_flow2.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...], "state":...}}.
 * A scripted JSON null returns a NULL pointer (None), never a json_null node;
 * {"__raise__": cls, "msg": ...} scripts an exception. For plan-taking funcs
 * the harness mutates a working copy of in.plan and echoes it as out.state
 * (even on a raise — partial mutation history); other funcs echo state {}.
 * tests/oracle_gateway_migrate_flow2.py drives the LIVE Python with the same
 * scripts and compares ordered trace + ret + state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/gateway_migrate_flow2.c"

typedef struct {
    const json_t *in;
    gm2_trace_t *trace;
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
    if (sc && sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx;
        size_t n = json_len(sc);
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else if (sc) {
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
    json_set(e, "args", args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    gm2_trace_add(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define SD(k, d) json_get_str(in, k, d)
#define J(k) json_obj_get(in, k)
#define B(k, d) json_get_bool(in, k, d)

static void ret_json(json_t *out, json_t *r) {
    json_set(out, "ret", r ? r : json_null());
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    gm2_trace_t trace = {NULL};
    json_t *counters = json_object();
    script_ctx_t ctx = {in, &trace, counters};
    gm2_seams_t seams = {&ctx, script_call};
    const gm2_seams_t *sp = &seams;
    json_t *state = json_object();
    json_t *plan = NULL;   /* working copy for plan-taking funcs */

    if (!strcmp(func, "profile_homes")) {
        ret_json(out, gm2_flow_profile_homes(sp));
    } else if (!strcmp(func, "gateway_identity")) {
        ret_json(out, gm2_flow_gateway_identity(SD("home", ""), J("pid"), J("services"), sp));
    } else if (!strcmp(func, "windows_task_installed")) {
        ret_json(out, gm2_flow_windows_task_installed(sp));
    } else if (!strcmp(func, "systemd_service_user")) {
        ret_json(out, gm2_flow_systemd_service_user(SD("home", ""), J("services"), sp));
    } else if (!strcmp(func, "s6_slot_op")) {
        ret_json(out, gm2_flow_s6_slot_op(SD("verb", ""), SD("home", ""), sp));
    } else if (!strcmp(func, "service_op")) {
        ret_json(out, gm2_flow_service_op(SD("kind", ""), B("system", false),
                                          SD("verb", ""), SD("home", ""), S("run_as_user"), sp));
    } else if (!strcmp(func, "stop_gateway_process")) {
        ret_json(out, gm2_flow_stop_gateway_process(SD("home", ""), sp));
    } else if (!strcmp(func, "spawn_detached_gateway")) {
        ret_json(out, gm2_flow_spawn_detached_gateway(SD("home", ""), sp));
    } else if (!strcmp(func, "read_multiplex_flag")) {
        ret_json(out, gm2_flow_read_multiplex_flag(SD("default_home", ""), sp));
    } else if (!strcmp(func, "write_multiplex_flag")) {
        ret_json(out, gm2_flow_write_multiplex_flag(SD("default_home", ""), B("value", false), sp));
    } else if (!strcmp(func, "credential_probe")) {
        ret_json(out, gm2_flow_credential_probe(J("platform_config")));
    } else if (!strcmp(func, "credential_claims")) {
        ret_json(out, gm2_flow_credential_claims(J("config"), sp));
    } else if (!strcmp(func, "credential_key_names")) {
        ret_json(out, gm2_flow_credential_key_names(SD("platform_value", ""), sp));
    } else if (!strcmp(func, "duplicate_credential_lines")) {
        ret_json(out, gm2_flow_duplicate_credential_lines(J("configs"), sp));
    } else if (!strcmp(func, "duplicate_credential_findings")) {
        ret_json(out, gm2_flow_duplicate_credential_findings(sp));
    } else if (!strcmp(func, "check_duplicate_credentials")) {
        plan = json_copy(J("plan"));
        ret_json(out, gm2_flow_check_duplicate_credentials(plan, J("configs"), sp));
    } else if (!strcmp(func, "listener_url")) {
        ret_json(out, gm2_flow_listener_url(J("default_cfg"), SD("platform_value", ""),
                                            SD("profile", ""), sp));
    } else if (!strcmp(func, "check_secondary_port_binders")) {
        plan = json_copy(J("plan"));
        ret_json(out, gm2_flow_check_secondary_port_binders(plan, J("configs"), sp));
    } else if (!strcmp(func, "load_profile_configs")) {
        plan = json_copy(J("plan"));
        ret_json(out, gm2_flow_load_profile_configs(plan, sp));
    } else if (!strcmp(func, "plan_tail")) {
        plan = json_copy(J("plan"));
        ret_json(out, gm2_flow_plan_tail(plan, sp));
    } else if (!strcmp(func, "format_update_warning")) {
        plan = json_copy(J("plan"));
        ret_json(out, gm2_flow_format_update_warning(plan, J("auto_blockers")));
    } else if (!strcmp(func, "host_supports_migration")) {
        ret_json(out, gm2_flow_host_supports_migration(sp));
    } else if (!strcmp(func, "build_migration_plan")) {
        ret_json(out, gm2_flow_build_migration_plan(sp));
    } else if (!strcmp(func, "format_plan")) {
        plan = json_copy(J("plan"));
        ret_json(out, gm2_flow_format_plan(plan, B("dry_run", false), sp));
    } else if (!strcmp(func, "cmd_migrate")) {
        ret_json(out, gm2_flow_cmd_migrate(J("args"), sp));
    } else if (!strcmp(func, "maybe_auto_migrate_after_update")) {
        ret_json(out, gm2_flow_maybe_auto_migrate_after_update(sp));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
        ret_json(out, NULL);
    }

    if (plan) { json_free(state); state = plan; }
    json_set(out, "state", state);
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
        /* Reserve envelope key positions: the oracle's want is built
         * {ret, trace, state} and canon() treats insertion order as semantic. */
        json_set(out, "ret", json_null());
        json_set(out, "trace", json_array());
        json_set(out, "state", json_object());
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
