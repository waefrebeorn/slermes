/*
 * t_port_gateway_status_flow.c — oracle harness for the gateway/status.py
 * liveness/status FLOW cores + leaves ported in
 * src/cli/port_gateway_status_flow.c.
 *
 * Self-contained: includes the port .c by bare name. Reads JSON-line cases
 * from stdin (tests/fixtures/gateway_status_flow.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...], "st":...}}.
 * A scripted JSON null returns a NULL pointer (None), never a json_null node;
 * {"__raise__": cls, "msg": ...} scripts an exception. The injected module
 * state "st" (prepare/publish) and the apply_set_fields target are mutated in
 * place and echoed back. tests/oracle_gateway_status_flow.py drives the LIVE
 * Python with the same scripts and compares ordered trace + ret + state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "port_gateway_status_flow.c"

typedef struct {
    const json_t *in;
    gsf_trace_t *trace;
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
    gsf_trace_add(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define SD(k, d) json_get_str(in, k, d)
#define J(k) json_obj_get(in, k)
#define N(k, d) json_get_num(in, k, d)

static void ret_json(json_t *out, json_t *r) {
    json_set(out, "ret", r ? r : json_null());
}

static json_t *state_copy(const json_t *in) {
    const json_t *st = json_obj_get(in, "st");
    if (st) return json_copy(st);
    json_t *d = json_object();
    json_set(d, "state_path", json_null());
    json_set(d, "state", json_null());
    return d;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    gsf_trace_t trace = {NULL};
    json_t *counters = json_object();
    script_ctx_t ctx = {in, &trace, counters};
    gsf_seams_t seams = {&ctx, script_call};
    const gsf_seams_t *sp = &seams;

    if (!strcmp(func, "live_pid_from_record")) {
        ret_json(out, gsf_flow_live_pid_from_record(J("record"), sp));
    } else if (!strcmp(func, "read_live_pid_marker")) {
        ret_json(out, gsf_flow_read_live_pid_marker(SD("path", ""), N("ttl_s", 60), sp));
    } else if (!strcmp(func, "multiplexer_liveness")) {
        ret_json(out, gsf_flow_multiplexer_liveness(SD("profile_dir", ""), sp));
    } else if (!strcmp(func, "live_gateway_pid_for_home")) {
        ret_json(out, gsf_flow_live_gateway_pid_for_home(SD("home", ""), sp));
    } else if (!strcmp(func, "prepare_runtime_status")) {
        json_t *st = state_copy(in);
        ret_json(out, gsf_flow_prepare_runtime_status(st, J("fields"), sp));
        json_set(out, "st", st);
    } else if (!strcmp(func, "publish_runtime_status")) {
        json_t *st = state_copy(in);
        ret_json(out, gsf_flow_publish_runtime_status(st, J("fields"), sp));
        json_set(out, "st", st);
    } else if (!strcmp(func, "probe_lock_file")) {
        ret_json(out, gsf_flow_probe_lock_file(SD("handle", "h"), sp));
    } else if (!strcmp(func, "terminate_verified_owner")) {
        ret_json(out, gsf_flow_terminate_verified_owner(
            N("owner_pid", 0), N("owner_start_time", 0),
            N("graceful_attempts", 20), N("force_attempts", 20), sp));
    } else if (!strcmp(func, "host_gateway_serves_home")) {
        ret_json(out, gsf_flow_host_gateway_serves_home(N("pid", 0), SD("profile_home", ""), sp));
    } else if (!strcmp(func, "merge_over_on_disk")) {
        ret_json(out, gsf_flow_merge_over_on_disk(SD("path", ""), J("payload"), sp));
    } else if (!strcmp(func, "start_times_conflict")) {
        ret_json(out, gsf_start_times_conflict(J("recorded"), J("current")));
    } else if (!strcmp(func, "pid_marker_names_self")) {
        ret_json(out, gsf_pid_marker_names_self(N("target_pid", 0), J("target_start_time"), sp));
    } else if (!strcmp(func, "heartbeat_age_s")) {
        ret_json(out, gsf_runtime_status_heartbeat_age_s(J("record"), sp));
    } else if (!strcmp(func, "coerce_session_store")) {
        ret_json(out, gsf_coerce_session_store(J("session_store")));
    } else if (!strcmp(func, "profile_platforms")) {
        ret_json(out, gsf_profile_platforms_from_multiplexer(J("runtime"), SD("profile", "default"), sp));
    } else if (!strcmp(func, "profile_name_for_home")) {
        ret_json(out, gsf_profile_name_for_home(SD("profile_home", ""), sp));
    } else if (!strcmp(func, "apply_set_fields")) {
        const json_t *t = J("target");
        json_t *target = t ? json_copy(t) : json_object();
        ret_json(out, gsf_apply_set_fields(target, J("fields"), sp));
        json_set(out, "st", target);
    } else if (!strcmp(func, "emit_transition")) {
        ret_json(out, gsf_emit_runtime_status_transition(J("previous"), J("payload"), sp));
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
        run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
        /* Serialize a string node for func (bounds-safe); never call
         * json_escape_string (off-by-one heap overflow in libjson). */
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
