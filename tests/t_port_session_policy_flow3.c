/*
 * t_port_session_policy_flow3.c -- oracle harness for the gateway/session_policy.py
 * third-wave FLOW cores ported in src/gateway/session_policy_flow3.c.
 *
 * Self-contained: includes the port .c by AREA-QUALIFIED name. Reads JSON-line
 * cases from stdin (tests/fixtures/session_policy_flow3.jsonl), wires the
 * universal seam to the SCRIPTED results in each case's in.io object (a
 * recording stub appending one trace event per seam call, in call order), and
 * emits {"func":..., "in":..., "out":{"ret":..., "trace":[...], "state":...}}.
 *
 * Mutable state: bind_launch_key / rebind_launch_key / release_launch_secrets
 * mutate a working copy of in.authority (out.state.authority);
 * _extract_config_secrets mutates working copies of in.value + in.private
 * (out.state.{value,private}). All other funcs echo out.state = null.
 * _ADOPTED_KEY_SETS is reset per case (sp3_adopt_reset) so the adoption warning
 * fires iff the unknown-key set is non-empty within a case.
 *
 * Script shapes: a bare JSON array is a SEQUENCE (one result per call, the LAST
 * element repeats after exhaustion); any other value is reused on every call;
 * a scripted JSON null is delivered as a NULL pointer (Python None). Fail-closed:
 * a MISSING or EMPTY script yields a RuntimeError raise spec ("no script for
 * <name>"), recorded as the event ret then propagated or swallowed by the port
 * exactly as live. BaseException kinds pass through every except Exception site.
 *
 * tests/oracle_session_policy_flow3.py drives the LIVE Python with the same
 * scripts and compares the ordered decision sequence + return + state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/session_policy_flow3.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *counters;
} sp3_script_ctx_t;

static json_t *sp3_make_raise(const char *cls, const char *msg) {
    json_t *r = json_object();
    json_set(r, "__raise__", json_string(cls));
    json_set(r, "msg", json_string(msg ? msg : ""));
    return r;
}

static json_t *sp3_args_drop_null(const json_t *args) {
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

static json_t *sp3_script_call(void *vctx, const char *name, const json_t *args) {
    sp3_script_ctx_t *ctx = vctx;
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
        result = sp3_make_raise("RuntimeError", m);
    }
    if (result && result->type == JSON_NULL) { json_free(result); result = NULL; }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", sp3_args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    json_append(ctx->trace, e);
    return result;
}

#define J(k) json_obj_get(in, k)
#define S(k) json_get_str(in, k, NULL)
#define B(k, d) json_get_bool(in, k, d)

static void run_case(const char *func, const json_t *in, json_t *out) {
    sp3_script_ctx_t ctx = { in, json_array(), json_object() };
    sp3_seams_t seams = { &ctx, sp3_script_call };
    const sp3_seams_t *sp = &seams;
    json_t *r = NULL;
    json_t *state = NULL;
    json_t *authority = NULL;
    json_t *value = NULL;
    json_t *private = NULL;
    json_t *cs = NULL;
    bool priv_is_null = false;

    sp3_adopt_reset();

    bool auth_state = strcmp(func, "bind_launch_key") == 0
                   || strcmp(func, "rebind_launch_key") == 0
                   || strcmp(func, "release_launch_secrets") == 0;
    bool extract_state = strcmp(func, "_extract_config_secrets") == 0;
    if (auth_state) {
        const json_t *a = J("authority");
        authority = (a && a->type == JSON_OBJECT) ? json_copy(a) : json_object();
    }
    if (extract_state) {
        const json_t *v = J("value");
        value = v ? json_copy(v) : json_null();
        const json_t *p = J("private");
        if (p && p->type == JSON_OBJECT) private = json_copy(p);
        else { private = NULL; priv_is_null = true; }
    }

    if (strcmp(func, "_refuse") == 0) {
        r = sp3_refuse(S("field"), S("detail"));
    } else if (strcmp(func, "_policy_refused") == 0) {
        r = sp3_policy_refused(S("field"), S("detail"));
    } else if (strcmp(func, "_adopt_stored_policy") == 0) {
        r = sp3_adopt_stored_policy(J("data"), sp);
    } else if (strcmp(func, "_stored_policy") == 0) {
        r = sp3_stored_policy(J("data"), sp);
    } else if (strcmp(func, "_check_stored_surface") == 0) {
        r = sp3_check_stored_surface(J("policy"), sp);
    } else if (strcmp(func, "_extract_config_secrets") == 0) {
        json_t *path = json_array();
        r = sp3_extract_config_secrets(value, private, path, sp);
        json_free(path);
    } else if (strcmp(func, "render_launch_skills") == 0) {
        r = sp3_render_launch_skills(J("params"), S("session_id"), sp);
    } else if (strcmp(func, "bind_launch_key") == 0) {
        const json_t *c = J("config_secrets");
        cs = (c && c->type == JSON_OBJECT) ? json_copy(c) : json_object();
        r = sp3_bind_launch_key(authority, S("session_id"), J("policy"), J("api_key"),
                                cs, B("owner_key", false), J("client_request"), sp);
    } else if (strcmp(func, "rebind_launch_key") == 0) {
        r = sp3_rebind_launch_key(authority, J("session"), J("api_key"), sp);
    } else if (strcmp(func, "_secret_refs") == 0) {
        sp3_strset_t retired; sp3_set_init(&retired);
        const json_t *rt = J("retired");
        if (rt && rt->type == JSON_ARRAY)
            for (size_t i = 0; i < json_len(rt); i++) {
                char *sid = sp3_pystr(json_get(rt, i));
                sp3_set_add(&retired, sid); free(sid);
            }
        r = sp3_secret_refs((json_t *)J("authority"), &retired, sp);
        sp3_set_free(&retired);
    } else if (strcmp(func, "release_launch_secrets") == 0) {
        r = sp3_release_launch_secrets(authority, J("session_ids"), sp);
    } else if (strcmp(func, "policy_for_source") == 0) {
        r = sp3_policy_for_source(J("runner"), J("source"), sp);
    } else if (strcmp(func, "workspace_unavailable_result") == 0) {
        r = sp3_workspace_unavailable_result(J("policy"), sp);
    } else if (strcmp(func, "refused_turn_result") == 0) {
        r = sp3_refused_turn_result(J("policy"), J("exc"), sp);
    } else if (strcmp(func, "release_session_workspaces") == 0) {
        r = sp3_release_session_workspaces(J("authority"), J("session_ids"), sp);
    } else if (strcmp(func, "policy_turn_toolsets") == 0) {
        r = sp3_policy_turn_toolsets(J("authority"), J("policy"), J("config"), S("session_id"), sp);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    if (auth_state) {
        state = json_object();
        json_set(state, "authority", authority);
        authority = NULL;
    } else if (extract_state) {
        state = json_object();
        json_set(state, "value", value);
        json_set(state, "private", (private && !priv_is_null) ? private : json_null());
        value = NULL; private = NULL;
        if (priv_is_null) { /* private was None */ }
    }
    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", ctx.trace);
    json_set(out, "state", state ? state : json_null());
    json_free(ctx.counters);
    json_free(cs);
    json_free(authority);
    json_free(value);
    json_free(private);
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
