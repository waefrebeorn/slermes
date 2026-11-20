/*
 * t_port_i18n_flow2.c — oracle harness for the agent/i18n.py FLOW remainder
 * ported in src/agent/i18n_flow2.c (_locales_dir, _bundled_surfaces,
 * _normalize_lang, _flatten_into, _load_catalog, _config_language_cached,
 * reset_language_cache, get_language, t) plus the composed-real
 * agent/i18n_layers.py clear_cache.
 *
 * Self-contained: includes the port .c by AREA-QUALIFIED name. Reads JSON-line
 * cases from stdin (tests/fixtures/i18n_flow2.jsonl), wires the universal seam
 * to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...], "state":{...}}}.
 * Injected state (catalog_cache / config_lang_cache / bundled_surfaces_cache /
 * supported_cache / pack_cache / overlay_cache / overlay_files_cache / out)
 * is mutated in place and echoed back in out.state.
 *
 * Fail-closed: a missing or empty seam script yields a RuntimeError raise spec
 * (recorded as the event ret, then propagated/swallowed by the port exactly as
 * live). A scripted JSON null is delivered as a NULL pointer (Python None).
 * tests/oracle_i18n_flow2.py drives the LIVE Python with the same scripts and
 * compares the ordered seam trace + return + mutated state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "agent/i18n_flow2.c"

typedef struct {
    const json_t *in;
    json_t *trace;     /* array of events */
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
            char *m = i18n2_fmt("no script for %s", name);
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
        char *m = i18n2_fmt("no script for %s", name);
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

static json_t *copy_state(const json_t *in) {
    const json_t *v = json_obj_get(in, "state");
    return (v && v->type == JSON_OBJECT) ? json_copy(v) : json_object();
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, NULL, json_object()};
    i18n2_io_t io = {script_call, &ctx};
    json_t *state = NULL;
    json_t *r = NULL;

    if (!strcmp(func, "_locales_dir")) {
        r = i18n2_locales_dir(&io);
    } else if (!strcmp(func, "_bundled_surfaces")) {
        state = copy_state(in);
        r = i18n2_bundled_surfaces(&io, SD("locales_dir", ""), state);
    } else if (!strcmp(func, "_normalize_lang")) {
        r = i18n2_normalize_lang(&io, J("value"), J("home"));
    } else if (!strcmp(func, "_flatten_into")) {
        r = i18n2_flatten_into(&io, J("node"), SD("prefix", ""), J("out"));
        state = json_object();
        const json_t *oj = J("out");
        json_set(state, "out", oj ? json_copy(oj) : json_null());
    } else if (!strcmp(func, "_load_catalog")) {
        state = copy_state(in);
        r = i18n2_load_catalog(&io, SD("lang", ""), J("home"), state);
    } else if (!strcmp(func, "_config_language_cached")) {
        state = copy_state(in);
        r = i18n2_config_language_cached(&io, SD("hermes_home", ""), state);
    } else if (!strcmp(func, "reset_language_cache")) {
        state = copy_state(in);
        r = i18n2_reset_language_cache(state);
    } else if (!strcmp(func, "get_language")) {
        r = i18n2_get_language(&io);
    } else if (!strcmp(func, "t")) {
        state = copy_state(in);
        r = i18n2_t(&io, SD("key", ""), J("lang"), J("kwargs"), state);
    } else if (!strcmp(func, "clear_cache")) {
        state = copy_state(in);
        r = i18n2_clear_cache(state);
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
        r = make_raise("RuntimeError", "unknown func");
    }

    ret_json(out, r);
    json_set(out, "trace", ctx.trace ? ctx.trace : json_array());
    if (state) json_set(out, "state", state);
    json_free(ctx.counters);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
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