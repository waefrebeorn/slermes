/*
 * t_port_session_policy_flow4.c -- oracle harness for the gateway/session_policy.py
 * fourth-wave FLOW cores ported in src/gateway/session_policy_flow4.c.
 *
 * Self-contained: includes the port .c by AREA-QUALIFIED name. Reads JSON-line
 * cases from stdin (tests/fixtures/session_policy_flow4.jsonl), wires the
 * universal seam to the SCRIPTED results in each case's in.io object (a
 * recording stub appending one trace event per seam call, in call order), and
 * emits {"func":..., "in":..., "out":{"ret":..., "trace":[...], "state":...}}.
 *
 * Script shapes: a bare JSON array is a SEQUENCE (one result per call, the LAST
 * element repeats after exhaustion); any other value is reused on every call;
 * a scripted JSON null is delivered as a NULL pointer (Python None). Fail-closed:
 * a MISSING or EMPTY script yields a RuntimeError raise spec. Full builtin
 * exception hierarchy is enforced by the port's sp4_exc_isa.
 *
 * Mutable state: build_policy / _apply_launch_overrides / _resolve_toolsets
 * mutate a working copy of in.config (out.state.config). All other funcs echo
 * out.state = null.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/session_policy_flow4.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *counters;
} sp4_script_ctx_t;

static json_t *sp4_make_raise(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}

static json_t *sp4_args_drop_null(const json_t *args) {
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

static json_t *sp4_script_call(void *vctx, const char *name, const json_t *args) {
    sp4_script_ctx_t *ctx = vctx;
    const json_t *io = json_obj_get(ctx->in, "io");
    const json_t *sc = io ? json_obj_get(io, name) : NULL;
    json_t *result = NULL;
    bool fail_closed = false;
    if (sc && sc->type == JSON_ARRAY) {
        size_t n = json_len(sc);
        if (n == 0) {
            fail_closed = true;
        } else {
            double idx = json_get_num(ctx->counters, name, 0);
            size_t i = (size_t)idx;
            result = json_copy(json_get(sc, i < n ? i : n - 1));
            json_set(ctx->counters, name, json_number(idx + 1));
        }
    } else if (sc) {
        result = json_copy(sc);
    } else {
        fail_closed = true;
    }
    if (fail_closed) {
        char m[160];
        snprintf(m, sizeof(m), "no script for %s", name);
        result = sp4_make_raise("RuntimeError", m);
    }
    if (result && result->type == JSON_NULL) { json_free(result); result = NULL; }
    if (result && result->type == JSON_OBJECT && result->c.count == 1
        && strcmp(result->c.keys[0], "__list__") == 0) {
        json_t *u = json_copy(json_obj_get(result, "__list__"));
        json_free(result); result = u;
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", sp4_args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    json_append(ctx->trace, e);
    return result;
}

#define J(k) json_obj_get(in, k)
#define S(k) json_get_str(in, k, NULL)
#define B(k, d) json_get_bool(in, k, d)

static void run_case(const char *func, const json_t *in, json_t *out) {
    sp4_script_ctx_t ctx = { in, json_array(), json_object() };
    sp4_seams_t seams = { &ctx, sp4_script_call };
    const sp4_seams_t *sp = &seams;
    json_t *r = NULL;
    json_t *state = NULL;
    bool config_state = strcmp(func, "build_policy") == 0
                     || strcmp(func, "_apply_launch_overrides") == 0
                     || strcmp(func, "_resolve_toolsets") == 0;
    json_t *config = NULL;
    if (config_state) {
        const json_t *c = J("config");
        config = (c && c->type == JSON_OBJECT) ? json_copy(c) : json_object();
    }

    if (strcmp(func, "active_policy") == 0) {
        r = sp4_active_policy(J("active"));
    } else if (strcmp(func, "turn_model") == 0) {
        r = sp4_turn_model(J("policy"), J("authority"), sp);
    } else if (strcmp(func, "profile_read_scope") == 0) {
        r = sp4_profile_read_scope(J("policy"), J("authority"), sp);
    } else if (strcmp(func, "_serving_local_desktop_within") == 0) {
        r = sp4_serving_local_desktop_within(J("scope"), sp);
    } else if (strcmp(func, "local_desktop_turn") == 0) {
        r = sp4_local_desktop_turn(J("runner"), J("source"), J("scope"), sp);
    } else if (strcmp(func, "_profile_workspace_cwd") == 0) {
        r = sp4_profile_workspace_cwd(S("home"), sp);
    } else if (strcmp(func, "_unnamed_extension_toolsets") == 0) {
        r = sp4_unnamed_extension_toolsets(J("explicit"), J("requested"), J("config"), J("toolsets"), sp);
    } else if (strcmp(func, "_resolve_toolsets") == 0) {
        r = sp4_resolve_toolsets(J("params"), config, S("source"), B("safe_mode", false),
                                 B("coding_posture", false), B("bypass", false),
                                 J("toolsets"), J("surface_toolsets"), sp);
    } else if (strcmp(func, "_apply_launch_overrides") == 0) {
        r = sp4_apply_launch_overrides(J("params"), config, sp);
    } else if (strcmp(func, "build_policy") == 0) {
        r = sp4_build_policy(J("params"), config, J("private_secrets"),
                             B("profile_terminal", true), B("workspace_posture", false),
                             B("client_local", false), S("home"), J("toolsets"),
                             J("surface_toolsets"), sp);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    if (config_state) {
        state = json_object();
        json_set(state, "config", config);
        config = NULL;
    }
    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", ctx.trace);
    json_set(out, "state", state ? state : json_null());
    json_free(ctx.counters);
    json_free(config);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    FILE *f = (argc > 1) ? fopen(argv[1], "rb") : stdin;
    if (!f) { fprintf(stderr, "cannot open fixture\n"); return 1; }
    char line[1 << 20];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '#') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case line: %s\n", err ? err : "not an object");
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
        json_t *func_node = json_string(func);
        char *func_s = json_serialize(func_node);
        json_free(func_node);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out);
        json_free(c);
    }
    if (f != stdin) fclose(f);
    return 0;
}
