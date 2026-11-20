/*
 * t_port_context_references_flow2.c — harness for the context_references_flow2
 * FLOW port. Reads JSON-line cases from stdin: {"func":..., "in":{...,"seams":{}}}.
 * Emits JSON lines: {"func":..., "in":..., "out":{"ret":...,"trace":[...],"state":{}}}.
 * Self-contained: includes the port .c by area-qualified path (libjson only).
 * Seam semantics are FAIL-CLOSED and identical to the oracle:
 *   MISSING script -> RuntimeError raise spec; present null -> None (NULL);
 *   array script -> per-call queue (last repeats, empty -> None);
 *   {"__raise__":cls,"msg":...} -> a raised exception.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agent/context_references_flow2.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *counters;
} script_ctx_t;

static json_t *args_drop_null(const json_t *args) {
    json_t *out = json_object();
    if (cr2_is_obj(args)) {
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
        char *m = cr2_xsf("no script for seam: %s", name);
        result = cr2_raise("RuntimeError", m);
        free(m);
    } else if (sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx, n = (size_t)sc->c.count;
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else {
        result = json_copy(sc);
    }
    /* present null -> None (NULL), never a JSON_NULL node */
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
    return v ? cr2_jtruth(v) : dflt;
}

static json_t *mk_state(const json_t *in_obj) {
    const json_t *s = json_obj_get(in_obj, "state");
    json_t *st = json_object();
    json_t *empty = NULL;
    if (!cr2_is_obj(s)) { empty = json_object(); s = empty; }
    json_t *providers = json_obj_get(s, "providers");
    json_t *owners = json_obj_get(s, "owners");
    json_set(st, "providers", json_object());
    json_set(st, "owners", json_object());
    if (cr2_is_obj(providers))
        for (size_t i = 0; i < providers->c.count; i++)
            json_set(json_obj_get(st, "providers"), providers->c.keys[i], json_copy(providers->c.items[i]));
    if (cr2_is_obj(owners))
        for (size_t i = 0; i < owners->c.count; i++)
            json_set(json_obj_get(st, "owners"), owners->c.keys[i], json_copy(owners->c.items[i]));
    json_free(empty);
    return st;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, json_array(), json_object()};
    cr2_seams_t seams = {&ctx, script_call};
    json_t *r = NULL;
    json_t *st = NULL;

    if (!strcmp(func, "format_reference_value")) {
        r = cr2_format_reference_value(J("value"));
    } else if (!strcmp(func, "strip_trailing_punctuation")) {
        r = cr2_strip_trailing_punctuation(J("value"));
    } else if (!strcmp(func, "strip_reference_wrappers")) {
        r = cr2_strip_reference_wrappers(J("value"));
    } else if (!strcmp(func, "parse_file_reference_value")) {
        r = cr2_parse_file_reference_value(J("value"));
    } else if (!strcmp(func, "get_providers")) {
        r = cr2_get_providers(J("providers"));
    } else if (!strcmp(func, "is_binary_file")) {
        r = cr2_is_binary_file(&seams, S("name") ? S("name") : "", S("path") ? S("path") : "");
    } else if (!strcmp(func, "agent_visible_path")) {
        r = cr2_agent_visible_path(&seams, S("path") ? S("path") : "");
    } else if (!strcmp(func, "binary_reference_block")) {
        r = cr2_binary_reference_block(&seams, S("raw") ? S("raw") : "", S("name") ? S("name") : "");
    } else if (!strcmp(func, "file_metadata")) {
        r = cr2_file_metadata(&seams, S("path") ? S("path") : "");
    } else if (!strcmp(func, "register_provider")) {
        st = mk_state(in);
        r = cr2_register_provider(st, S("prefix") ? S("prefix") : "",
                                  bool_arg(in, "is_provider", true), J("owner"),
                                  bool_arg(in, "reclaim", false),
                                  bool_arg(in, "publish", false),
                                  bool_arg(in, "publish_changed", false));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", ctx.trace);
    if (st) json_set(out, "state", st);
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
        /* reserve key positions: canon() treats dict order as semantic */
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
