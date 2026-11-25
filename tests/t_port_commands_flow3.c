/*
 * t_port_commands_flow3.c — oracle harness for the commands.py remainder FLOW
 * surface ported in src/cli/commands_flow3.c (included by area-qualified name).
 *
 * Reads JSON-line cases from stdin (tests/fixtures/commands_flow3.jsonl), wires
 * the universal seam to each case's scripted "seams" object (recording one trace
 * event per call, in order), and emits
 *   {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 * Fail-closed: a missing/empty seam script yields a RuntimeError raise spec.
 * An uncaught raise parks in cm3_pending and is emitted as ret.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli/commands_flow3.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *counters;
} script_ctx_t;

static json_t *make_raise(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}
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
        size_t n = json_len(sc);
        if (n == 0) {
            char *m = xsf("no script for %s", name);
            result = make_raise("RuntimeError", m);
            free(m);
        } else {
            double idx = json_get_num(ctx->counters, name, 0);
            size_t i = (size_t)idx;
            result = json_copy(json_get(sc, i < n ? i : n - 1));
            json_set(ctx->counters, name, json_number(idx + 1));
        }
    } else if (sc) {
        result = json_copy(sc);
    } else {
        char *m = xsf("no script for %s", name);
        result = make_raise("RuntimeError", m);
        free(m);
    }
    /* A scripted JSON null is delivered as a NULL pointer, not a json_null node. */
    if (result && result->type == JSON_NULL) { json_free(result); result = NULL; }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    if (!ctx->trace) ctx->trace = json_array();
    json_append(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define SD(k, d) json_get_str(in, k, d)
#define J(k) json_obj_get(in, k)

static void ret_json(json_t *out, json_t *r) { json_set(out, "ret", r ? r : json_null()); }
static void ret_bool(json_t *out, bool b) { json_set(out, "ret", json_bool(b)); }

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, NULL, json_object()};
    cm3_seams_t seams = {&ctx, script_call};
    const cm3_seams_t *sp = &seams;
    cm3_take_pending();  /* clear any stale parked raise */

    if (!strcmp(func, "infer_argument_mode")) {
        ret_json(out, cm3_infer_argument_mode(J("cmd")));
    } else if (!strcmp(func, "command_desktop_meta")) {
        ret_json(out, cm3_command_desktop_meta(J("cmd")));
    } else if (!strcmp(func, "resolve_command")) {
        ret_json(out, cm3_resolve_command(S("name"), J("registry")));
    } else if (!strcmp(func, "build_description")) {
        ret_json(out, cm3_build_description(J("cmd"), sp));
    } else if (!strcmp(func, "is_gateway_known_command")) {
        ret_bool(out, cm3_is_gateway_known_command(S("name"), J("registry"), sp));
    } else if (!strcmp(func, "is_interrupt_then_dispatch")) {
        ret_bool(out, cm3_is_interrupt_then_dispatch(S("name"), J("registry")));
    } else if (!strcmp(func, "should_bypass_active_session")) {
        ret_bool(out, cm3_should_bypass_active_session(S("name"), J("registry")));
    } else if (!strcmp(func, "resolve_config_gates")) {
        ret_json(out, cm3_resolve_config_gates(J("registry"), in, sp));
    } else if (!strcmp(func, "gateway_help_lines")) {
        ret_json(out, cm3_gateway_help_lines(J("allowed"), J("registry"), sp));
    } else if (!strcmp(func, "iter_plugin_command_entries")) {
        ret_json(out, cm3_iter_plugin_command_entries(in, sp));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
        ret_json(out, NULL);
    }

    json_t *pending = cm3_take_pending();
    if (pending) json_set(out, "ret", pending);
    json_set(out, "trace", ctx.trace ? ctx.trace : json_array());
    json_free(ctx.counters);
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
