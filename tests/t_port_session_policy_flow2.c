/*
 * t_port_session_policy_flow2.c -- oracle harness for the gateway/session_policy.py
 * second-wave FLOW cores ported in src/gateway/session_policy_flow2.c.
 *
 * Self-contained: includes the port .c by AREA-QUALIFIED name. Reads JSON-line
 * cases from stdin (tests/fixtures/session_policy_flow2.jsonl), wires the
 * universal seam to the SCRIPTED results in each case's in.io object (a
 * recording stub appending one trace event per seam call, in call order), and
 * emits {"func":..., "in":..., "out":{"ret":..., "trace":[...], "state":...}}.
 *
 * Mutable state: pin_reasoning_effort / unpin_reasoning mutate a working copy
 * of in.config (echoed as out.state.config); bind/unbind_turn_workspace mutate
 * a working copy of in.overrides (out.state.overrides). All other funcs echo
 * out.state = null.
 *
 * Script shapes: a bare JSON array is a SEQUENCE (one result per call, the LAST
 * element repeats after exhaustion); any other value is reused on every call;
 * a scripted JSON null is delivered as a NULL pointer (Python None). Fail-closed:
 * a MISSING or EMPTY script yields a RuntimeError raise spec ("no script for
 * <name>"), recorded as the event ret then propagated or swallowed by the port
 * exactly as live. BaseException kinds pass through every except Exception site.
 *
 * tests/oracle_session_policy_flow2.py drives the LIVE Python with the same
 * scripts and compares the ordered decision sequence + return + state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/session_policy_flow2.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *counters;
} sp2_script_ctx_t;

static json_t *sp2_make_raise(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}

static json_t *sp2_args_drop_null(const json_t *args) {
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

static json_t *sp2_script_call(void *vctx, const char *name, const json_t *args) {
    sp2_script_ctx_t *ctx = vctx;
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
        result = sp2_make_raise("RuntimeError", m);
    }
    if (result && result->type == JSON_NULL) { json_free(result); result = NULL; }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", sp2_args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    json_append(ctx->trace, e);
    return result;
}

#define J(k) json_obj_get(in, k)
#define S(k) json_get_str(in, k, NULL)
#define B(k, d) json_get_bool(in, k, d)

static void run_case(const char *func, const json_t *in, json_t *out) {
    sp2_script_ctx_t ctx = { in, json_array(), json_object() };
    sp2_seams_t seams = { &ctx, sp2_script_call };
    const sp2_seams_t *sp = &seams;
    json_t *r = NULL;
    json_t *state = NULL;
    json_t *config = NULL;
    json_t *overrides = NULL;

    bool cfg_state = strcmp(func, "pin_reasoning_effort") == 0
                  || strcmp(func, "unpin_reasoning") == 0;
    bool ov_state = strcmp(func, "bind_turn_workspace") == 0
                 || strcmp(func, "unbind_turn_workspace") == 0;
    if (cfg_state) {
        const json_t *c = J("config");
        config = (c && c->type == JSON_OBJECT) ? json_copy(c) : json_object();
    }
    if (ov_state) {
        const json_t *o = J("overrides");
        overrides = (o && o->type == JSON_OBJECT) ? json_copy(o) : json_object();
    }

    if (strcmp(func, "model_route") == 0) {
        r = sp2_model_route(J("block"), sp);
    } else if (strcmp(func, "_held_pick") == 0) {
        r = sp2_held_pick(J("frozen"), J("live"), sp);
    } else if (strcmp(func, "entry_serves") == 0) {
        r = sp2_entry_serves(J("block"), J("config"), sp);
    } else if (strcmp(func, "bound_route_holds") == 0) {
        r = sp2_bound_route_holds(J("bound"), J("bound_config"), J("block"), J("config"), sp);
    } else if (strcmp(func, "_without_auto_pick") == 0) {
        r = sp2_without_auto_pick(J("params"));
    } else if (strcmp(func, "_keeps_profile_route") == 0) {
        r = sp2_keeps_profile_route(J("picked"), J("config"), sp);
    } else if (strcmp(func, "_with_entry_pick") == 0) {
        r = sp2_with_entry_pick(J("params"), J("config"), sp);
    } else if (strcmp(func, "pin_reasoning_effort") == 0) {
        r = sp2_pin_reasoning_effort(config, J("effort"), B("session", false));
    } else if (strcmp(func, "unpin_reasoning") == 0) {
        r = sp2_unpin_reasoning(config, J("profile"), sp);
    } else if (strcmp(func, "profile_config") == 0) {
        r = sp2_profile_config(J("authority"), sp);
    } else if (strcmp(func, "_key_slot") == 0) {
        r = sp2_key_slot(J("credential_ref"));
    } else if (strcmp(func, "launch_key") == 0) {
        r = sp2_launch_key(J("authority"), J("policy"), sp);
    } else if (strcmp(func, "route_launch_key") == 0) {
        r = sp2_route_launch_key(J("authority"), J("policy"), J("config"), sp);
    } else if (strcmp(func, "workspace_ready") == 0) {
        r = sp2_workspace_ready(J("policy"), sp);
    } else if (strcmp(func, "bind_turn_workspace") == 0) {
        r = sp2_bind_turn_workspace(J("policy"), S("task_id"), overrides, sp);
    } else if (strcmp(func, "unbind_turn_workspace") == 0) {
        r = sp2_unbind_turn_workspace(S("task_id"), overrides, sp);
    } else if (strcmp(func, "accept_launch_hooks") == 0) {
        r = sp2_accept_launch_hooks(J("policy"), sp);
    } else if (strcmp(func, "refresh_profile_hooks") == 0) {
        r = sp2_refresh_profile_hooks(J("policy"), sp);
    } else if (strcmp(func, "register_worker_hooks") == 0) {
        r = sp2_register_worker_hooks(J("policy"), sp);
    } else if (strcmp(func, "_in_coding_workspace") == 0) {
        r = sp2_in_coding_workspace(J("config"), S("source"), S("cwd"), sp);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    if (cfg_state) {
        state = json_object();
        json_set(state, "config", config);
    } else if (ov_state) {
        state = json_object();
        json_set(state, "overrides", overrides);
    }
    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", ctx.trace);
    json_set(out, "state", state ? state : json_null());
    json_free(ctx.counters);
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
