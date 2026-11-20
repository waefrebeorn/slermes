/*
 * t_port_memory_manager_flow3.c — harness for the memory_manager_flow3 FLOW port.
 *
 * Reads JSON-line cases from stdin: {"func":..., "in":{...,"seams":{...}}}.
 * Emits JSON lines: {"func":..., "in":..., "out":{"ret":...,"trace":[...],
 * "state":{...}}}. "ret" is None->null / raise-spec; "trace" is the ordered
 * seam-call sequence; "state" is the (mutated) state window echoed verbatim.
 *
 * Self-contained: #includes the port .c by area-qualified path (libjson only).
 * Script semantics are FAIL-CLOSED and identical to the oracle: a MISSING
 * script -> RuntimeError raise spec; a present null -> None (NULL pointer);
 * an array script is a per-call queue (last value repeats, empty -> None);
 * {"__raise__":cls,"msg":...} scripts a raise.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent/memory_manager_flow3.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *counters;
} script_ctx_t;

static json_t *args_drop_null(const json_t *args) {
    json_t *out = json_object();
    if (mm3_is_obj(args)) {
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
        char *m = mm3_xsf("no script for seam: %s", name);
        result = mm3_raise("RuntimeError", m);
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
    return v ? mm3_jtruth(v) : dflt;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, json_array(), json_object()};
    mm3_seams_t seams = {&ctx, script_call};
    const json_t *sin = json_obj_get(in, "state");
    json_t *st = sin ? json_copy(sin) : json_object();
    json_t *r = NULL;

    if (!strcmp(func, "redact_for_provider")) {
        r = mm3_redact_for_provider(&seams, J("values"), bool_arg(in, "urls", true));
    } else if (!strcmp(func, "redact_messages")) {
        r = mm3_redact_messages(&seams, J("messages"));
    } else if (!strcmp(func, "gate_init")) {
        r = mm3_gate_init(st);
    } else if (!strcmp(func, "gate_enter")) {
        r = mm3_gate_enter(st, J("key"), S("token") ? S("token") : "",
                           json_get_num(in, "timeout", 0), json_get_num(in, "now", 0));
    } else if (!strcmp(func, "gate_leave")) {
        r = mm3_gate_leave(st, J("key"), S("token") ? S("token") : "",
                           bool_arg(in, "ok", false), json_get_num(in, "now", 0));
    } else if (!strcmp(func, "manager_init")) {
        r = mm3_manager_init(&seams, st, J("external_prefetch_timeout"));
    } else if (!strcmp(func, "tool_call_keywords")) {
        json_t *cache = json_obj_get(st, "tool_call_keywords_cache");
        if (!mm3_is_obj(cache)) { cache = json_object(); json_set(st, "tool_call_keywords_cache", cache); }
        r = mm3_tool_call_keywords(&seams, cache, J("provider"));
    } else if (!strcmp(func, "drop_provider")) {
        r = mm3_drop_provider(st, J("provider"));
    } else if (!strcmp(func, "initialize_for_agent")) {
        r = mm3_initialize_for_agent(&seams, st, J("before"), J("kwargs"),
                                     bool_arg(in, "say", false));
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
        char *in_s = json_serialize(in ? in : empty);
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
