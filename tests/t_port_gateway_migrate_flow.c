/*
 * t_port_gateway_migrate_flow.c — oracle harness for the
 * hermes_cli/gateway_migrate.py migration FLOW cores + leaves ported in
 * src/cli/port_gateway_migrate_flow.c.
 *
 * Self-contained: includes the port .c by bare name. Reads JSON-line cases
 * from stdin (tests/fixtures/gateway_migrate_flow.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 * A scripted JSON null returns a NULL pointer (None), never a json_null node;
 * {"__raise__": cls, "msg": ...} scripts an exception. tests/oracle_gateway_migrate_flow.py
 * drives the LIVE Python with the same scripts and compares ordered trace + ret.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "port_gateway_migrate_flow.c"

typedef struct {
    const json_t *in;
    gmf_trace_t *trace;
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
    gmf_trace_add(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define SD(k, d) json_get_str(in, k, d)
#define J(k) json_obj_get(in, k)
#define N(k, d) json_get_num(in, k, d)

static void ret_json(json_t *out, json_t *r) {
    json_set(out, "ret", r ? r : json_null());
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    gmf_trace_t trace = {NULL};
    json_t *counters = json_object();
    script_ctx_t ctx = {in, &trace, counters};
    gmf_seams_t seams = {&ctx, script_call};
    const gmf_seams_t *sp = &seams;

    if (!strcmp(func, "service_label")) {
        ret_json(out, gmf_flow_service_label(J("service")));
    } else if (!strcmp(func, "remove_verb")) {
        ret_json(out, gmf_flow_remove_verb(J("service")));
    } else if (!strcmp(func, "service_from_dict")) {
        ret_json(out, gmf_flow_service_from_dict(J("service")));
    } else if (!strcmp(func, "recorded_services")) {
        ret_json(out, gmf_flow_recorded_services(J("rec")));
    } else if (!strcmp(func, "manifest_secondaries")) {
        ret_json(out, gmf_flow_manifest_secondaries(J("manifest")));
    } else if (!strcmp(func, "target_from_manifest")) {
        ret_json(out, gmf_flow_target_from_manifest(J("manifest")));
    } else if (!strcmp(func, "manifest_not_yet_served")) {
        ret_json(out, gmf_flow_manifest_not_yet_served(J("manifest"), J("live_served"), sp));
    } else if (!strcmp(func, "read_manifest")) {
        ret_json(out, gmf_flow_read_manifest(SD("default_home", ""), sp));
    } else if (!strcmp(func, "own_gateway_pid")) {
        ret_json(out, gmf_flow_own_gateway_pid(SD("home", ""), J("owner"), sp));
    } else if (!strcmp(func, "preflight_apply")) {
        ret_json(out, gmf_flow_preflight_apply(J("plan"), J("target"), S("run_as_user"), sp));
    } else if (!strcmp(func, "restart_default")) {
        ret_json(out, gmf_flow_restart_default(J("plan_default"), J("target"),
                                               SD("default_home", ""), S("run_as_user"), sp));
    } else if (!strcmp(func, "enable_default_at_boot")) {
        ret_json(out, gmf_flow_enable_default_at_boot(J("plan"), sp));
    } else if (!strcmp(func, "remove_secondary_gateways")) {
        ret_json(out, gmf_flow_remove_secondary_gateways(J("plan"), sp));
    } else if (!strcmp(func, "resume_target")) {
        ret_json(out, gmf_flow_resume_target(J("plan")));
    } else if (!strcmp(func, "resume_findings")) {
        ret_json(out, gmf_flow_resume_findings(J("plan"), J("target"), S("run_as_user"), sp));
    } else if (!strcmp(func, "signalled_gateways")) {
        ret_json(out, gmf_flow_signalled_gateways(J("plan")));
    } else if (!strcmp(func, "apply_migration")) {
        ret_json(out, gmf_flow_apply_migration(J("plan"), N("served_wait", 90.0), sp));
    } else if (!strcmp(func, "rollback_migration")) {
        ret_json(out, gmf_flow_rollback_migration(S("default_home"), sp));
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
        /* Reserve envelope key positions: the oracle's want is built
         * {ret, trace} and canon() treats insertion order as semantic. */
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
