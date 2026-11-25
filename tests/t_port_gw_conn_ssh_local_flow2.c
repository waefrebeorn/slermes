/*
 * t_port_gw_conn_ssh_local_flow2.c — oracle harness for gw_conn_ssh_local_flow2.
 * Includes the port .c by area-qualified name. Reads JSON-line cases from stdin
 * (fixtures/gw_conn_ssh_local_flow2.jsonl), wires the universal seam to SCRIPTED
 * results with a recording stub (one trace event per seam call), and emits
 * {"func":...,"in":...,"out":{"ret":...,"trace":[...]}}.
 * FAIL-CLOSED: a missing seam key -> {"__raise__":"RuntimeError",...}; a present
 * JSON null is a valid None; a JSON array is one result per call.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "cli/gw_conn_ssh_local_flow2.c"

typedef struct { const json_t *in; gcl2_trace_t *trace; json_t *counters; } cs2_ctx_t;

static json_t *cs2_drop_null(const json_t *args) {
    json_t *o = json_object();
    if (args && args->type == JSON_OBJECT)
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (v && v->type == JSON_NULL) continue;
            json_set(o, args->c.keys[i], json_copy(v));
        }
    return o;
}
static json_t *cs2_script_call(void *vctx, const char *name, const json_t *args) {
    cs2_ctx_t *ctx = vctx;
    const json_t *sc = json_obj_get(json_obj_get(ctx->in, "seams"), name);
    json_t *result = NULL;
    if (!sc) {
        char *m = gcl2_xsf("unscripted seam: %s", name);
        result = gcl2_raise_spec("RuntimeError", m); free(m);
    } else if (sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx, n = json_len(sc);
        if (n) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else result = json_copy(sc);
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", cs2_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    gcl2_trace_add(ctx->trace, e);
    return result;
}
#define S(k) json_get_str(in, k, "")
#define J(k) json_obj_get(in, k)

static void run_case(const char *func, const json_t *in, json_t *out) {
    gcl2_trace_t trace = {NULL};
    json_t *counters = json_object();
    cs2_ctx_t ctx = {in, &trace, counters};
    gcl2_seams_t seams = {&ctx, cs2_script_call};
    const gcl2_seams_t *sp = &seams;
    json_t *ret = NULL;

    if (!strcmp(func, "ownership_id")) ret = gcl2_ownership_id(J("root"), J("connection_id"));
    else if (!strcmp(func, "_fingerprint")) ret = gcl2_fingerprint(J("connection"));
    else if (!strcmp(func, "_target")) ret = gcl2_target(J("connected"));
    else if (!strcmp(func, "_health_ok")) ret = gcl2_health_ok(sp, S("base_url"));
    else if (!strcmp(func, "_probe_reuse_proof")) ret = gcl2_probe_reuse_proof(sp, S("base_url"), S("token"), S("nonce"));
    else if (!strcmp(func, "_wait_for_health")) ret = gcl2_wait_for_health(sp, S("base_url"));
    else if (!strcmp(func, "_live_reuse")) ret = gcl2_live_reuse(sp, J("tunnel"));
    else if (!strcmp(func, "_endpoint")) ret = gcl2_endpoint(sp, J("connection"), J("root"), J("ownership_id"));
    else if (!strcmp(func, "_teardown")) ret = gcl2_teardown(sp, J("tunnel"));
    else if (!strcmp(func, "tunnel_status")) ret = gcl2_tunnel_status(J("tunnel"));
    else if (!strcmp(func, "disconnect")) ret = gcl2_disconnect(sp, J("tunnel"));
    else if (!strcmp(func, "_bootstrap")) ret = gcl2_bootstrap(sp, J("connection"), J("root"), J("previous"));
    else if (!strcmp(func, "ensure_tunnel")) ret = gcl2_ensure_tunnel(sp, J("connection"), J("root"), J("tunnel"));
    else if (!strcmp(func, "interactive_shell_argv")) ret = gcl2_interactive_shell_argv(sp, J("connection"), J("cwd"), J("root"), J("tunnel"));
    else if (!strcmp(func, "close_all")) ret = gcl2_close_all(sp, J("tunnels"));
    else fprintf(stderr, "unknown func: %s\n", func);

    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", trace.events ? json_copy(trace.events) : json_array());
    gcl2_trace_free(&trace);
    json_free(counters);
}

int main(void) {
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    char *line = NULL; size_t cap = 0; ssize_t len;
    while ((len = getline(&line, &cap, stdin)) > 0) {
        if (len && line[len - 1] == '\n') line[len - 1] = '\0';
        if (line[0] != '{') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case\n"); free(err); if (c) json_free(c); continue;
        }
        free(err);
        const char *func = json_get_str(c, "func", "");
        const json_t *in = json_obj_get(c, "in");
        json_t *empty = json_object(), *out = json_object();
        run_case(func, in ? in : empty, out);
        json_free(empty);
        char *is = json_serialize(in ? in : empty);
        char *os = json_serialize(out);
        char *fs = json_escape_string(func);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", fs, is ? is : "null", os ? os : "null");
        free(is); free(os); free(fs);
        json_free(out); json_free(c);
    }
    free(line); return 0;
}
