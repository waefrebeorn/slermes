/*
 * t_port_memory_manager_flow2.c — harness for the memory_manager_flow2 FLOW port.
 *
 * Reads JSON-line cases from stdin: {"func":..., "in":{...,"seams":{...}}}.
 * Emits JSON lines: {"func":..., "in":..., "out":{"ret":...,"trace":[...],
 * "state":{...}}}. "ret" is None->null / raise-spec; "trace" is the ordered
 * seam-call sequence; "state" is the 5-key state window echoed verbatim so the
 * oracle's state-doubles can mutate it in place (history on raise included).
 *
 * Self-contained: #includes the port .c by area-qualified path (libjson only).
 * Script semantics are FAIL-CLOSED and identical to the oracle: a MISSING
 * script -> RuntimeError raise spec; a present null -> None (NULL pointer); an
 * array script is a per-call queue (last value repeats, empty -> None);
 * {"__raise__":cls,"msg":...} scripts a raise.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent/memory_manager_flow2.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *counters;
} script_ctx_t;

static json_t *args_drop_null(const json_t *args) {
    json_t *out = json_object();
    if (mm2_is_obj(args)) {
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
        char *m = mm2_xsf("no script for seam: %s", name);
        result = mm2_raise("RuntimeError", m);
        free(m);
    } else if (sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx, n = (size_t)sc->c.count;
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else {
        result = json_copy(sc);
    }
    /* NULL == None: a scripted null must NOT surface as a json_null node. */
    if (result && result->type == JSON_NULL) { json_free(result); result = NULL; }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args_drop_null(args));   /* borrowed: copy, do not free */
    json_set(e, "ret", result ? json_copy(result) : json_null());
    json_append(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)
static bool bool_arg(const json_t *in_obj, const char *k, bool dflt) {
    const json_t *v = json_obj_get(in_obj, k);
    return v ? mm2_jtruth(v) : dflt;
}

/* Build the fixed 5-key state window. Every fixture carries "state" with all
 * five keys (explicit null/false as needed), matching the oracle's deepcopy. */
static json_t *mk_state(const json_t *in_obj) {
    const json_t *s = json_obj_get(in_obj, "state");
    json_t *empty = NULL;
    if (!mm2_is_obj(s)) { empty = json_object(); s = empty; }
    json_t *st = json_object();
    const json_t *v;
    json_set(st, "bound_scope", (v = json_obj_get(s, "bound_scope")) ? json_copy(v) : json_null());
    json_set(st, "home_key",    (v = json_obj_get(s, "home_key"))    ? json_copy(v) : json_null());
    json_set(st, "agent_context",(v = json_obj_get(s, "agent_context"))? json_copy(v) : json_null());
    json_set(st, "teardown_claim",(v = json_obj_get(s, "teardown_claim"))? json_copy(v) : json_null());
    json_set(st, "shutting_down",(v = json_obj_get(s, "shutting_down"))? json_copy(v) : json_bool(false));
    json_free(empty);
    return st;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, json_array(), json_object()};
    mm2_seams_t seams = {&ctx, script_call};
    json_t *st = mk_state(in);
    const json_t *bound = json_obj_get(st, "bound_scope");
    const char *home = json_get_str(in, "home_key", "");
    json_t *r = NULL;

    if (!strcmp(func, "signature_params")) {
        r = mm2_signature_params(&seams, J("fn"));
    } else if (!strcmp(func, "has_var_kwargs")) {
        r = json_bool(mm2_has_var_kwargs(J("params")));
    } else if (!strcmp(func, "accepts_require_checkpoint")) {
        r = mm2_accepts_require_checkpoint(&seams, J("fn"));
    } else if (!strcmp(func, "scrub")) {
        r = mm2_scrub(&seams, J("text"), bool_arg(in, "urls", true));
    } else if (!strcmp(func, "owner_allows")) {
        r = mm2_owner_allows(&seams, J("providers"), bound, S("hook") ? S("hook") : "");
    } else if (!strcmp(func, "fork_turn_hooks_suppressed")) {
        r = mm2_fork_turn_hooks_suppressed(json_get_str(st, "agent_context", ""));
    } else if (!strcmp(func, "provider_sync_accepts")) {
        r = mm2_provider_sync_accepts(&seams, J("sync_fn"), S("keyword") ? S("keyword") : "");
    } else if (!strcmp(func, "registry_key")) {
        r = mm2_registry_key(bound, home, S("provider_name") ? S("provider_name") : "");
    } else if (!strcmp(func, "checkpoint_api_version")) {
        r = mm2_checkpoint_api_version(J("provider"));
    } else if (!strcmp(func, "supports_pre_compress_checkpoint")) {
        r = mm2_supports_pre_compress_checkpoint(&seams, J("providers"),
                                                 json_get_num(in, "api_version", 2));
    } else if (!strcmp(func, "each_provider")) {
        r = mm2_each_provider(&seams, J("providers"), S("label") ? S("label") : "",
                              S("call_name") ? S("call_name") : "",
                              json_get_num(in, "level", 10), bool_arg(in, "exc_info", false));
    } else if (!strcmp(func, "call_unless_stalled")) {
        r = mm2_call_unless_stalled(&seams, J("provider"), bound, home,
                                    S("call_name") ? S("call_name") : "");
    } else if (!strcmp(func, "claim_teardown")) {
        r = mm2_claim_teardown(st, bool_arg(in, "session_end", false));
    } else if (!strcmp(func, "release_soft")) {
        r = mm2_release_soft(&seams, st);
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", ctx.trace);
    json_set(out, "state", st);
    json_free(ctx.counters);
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
            free(err); json_free(c); continue;
        }
        const char *func = json_get_str(c, "func", "");
        const json_t *in = json_obj_get(c, "in");
        json_t *empty = json_object();
        json_t *out = json_object();
        /* Reserve envelope key positions: canon() treats dict order as semantic. */
        json_set(out, "ret", json_null());
        json_set(out, "trace", json_array());
        json_set(out, "state", json_object());
        run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
        json_t *fn = json_string(func);
        char *fn_s = json_serialize(fn);
        json_free(fn);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", fn_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(fn_s);
        json_free(out); json_free(c);
    }
    free(line);
    return 0;
}
