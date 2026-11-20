/*
 * t_port_context_references_flow3.c — harness for the context_references_flow3
 * FLOW port. Reads JSON-line cases from stdin: {"func":..., "in":{...,"seams":{}}}.
 * Emits JSON lines: {"func":..., "in":..., "out":{"ret":...,"trace":[...]}}.
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

#include "agent/context_references_flow3.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *counters;
} script_ctx_t;

static json_t *args_drop_null(const json_t *args) {
    json_t *out = json_object();
    if (cr3_is_obj(args)) {
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
        char *m = cr3_xsf("no script for seam: %s", name);
        result = cr3_raise("RuntimeError", m);
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
    return v ? cr3_jtruth(v) : dflt;
}
static double num_arg(const json_t *in_obj, const char *k, double dflt) {
    const json_t *v = json_obj_get(in_obj, k);
    return (v && v->type == JSON_NUMBER) ? v->num_val : dflt;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, json_array(), json_object()};
    cr3_seams_t seams = {&ctx, script_call};
    json_t *r = NULL;

    if (!strcmp(func, "parse_context_references")) {
        r = cr3_parse_context_references(&seams, S("message") ? S("message") : "",
                                         J("providers"), bool_arg(in, "plugins", true));
    } else if (!strcmp(func, "preprocess")) {
        r = cr3_preprocess(&seams, S("message") ? S("message") : "",
                           S("cwd") ? S("cwd") : "", num_arg(in, "context_length", 0),
                           J("allowed_root"));
    } else if (!strcmp(func, "preprocess_async")) {
        r = cr3_preprocess_async(&seams, S("message") ? S("message") : "",
                                 S("cwd") ? S("cwd") : "", num_arg(in, "context_length", 0),
                                 J("allowed_root"), bool_arg(in, "plugins", true));
    } else if (!strcmp(func, "expand_reference")) {
        r = cr3_expand_reference(&seams, J("ref"), S("cwd") ? S("cwd") : "",
                                 J("allowed_root"), J("max_inline_tokens"), J("providers"));
    } else if (!strcmp(func, "expand_git_reference")) {
        r = cr3_expand_git_reference(&seams, J("ref"), S("cwd") ? S("cwd") : "",
                                     J("args"), S("label") ? S("label") : "");
    } else if (!strcmp(func, "fetch_url_content")) {
        r = cr3_fetch_url_content(&seams, S("url") ? S("url") : "");
    } else if (!strcmp(func, "default_url_fetcher")) {
        r = cr3_default_url_fetcher(&seams, S("url") ? S("url") : "");
    } else if (!strcmp(func, "resolve_path")) {
        r = cr3_resolve_path(&seams, S("cwd") ? S("cwd") : "",
                             S("target") ? S("target") : "", J("allowed_root"));
    } else if (!strcmp(func, "ensure_path_allowed")) {
        r = cr3_ensure_path_allowed(&seams, S("path") ? S("path") : "",
                                    J("constants"), S("home") ? S("home") : "",
                                    S("hermes_home") ? S("hermes_home") : "");
    } else if (!strcmp(func, "build_folder_listing")) {
        r = cr3_build_folder_listing(&seams, S("path") ? S("path") : "",
                                     S("cwd") ? S("cwd") : "", num_arg(in, "limit", 200),
                                     J("display_base"));
    } else if (!strcmp(func, "iter_visible_entries")) {
        r = cr3_iter_visible_entries(&seams, S("path") ? S("path") : "",
                                     S("cwd") ? S("cwd") : "", num_arg(in, "limit", 200));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", ctx.trace);
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
