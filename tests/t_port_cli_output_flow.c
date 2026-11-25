/*
 * t_port_cli_output_flow.c — oracle harness for the UNCLAIMED hermes_cli/
 * cli_output.py rendering-cluster FLOW cores + leaves ported in
 * src/cli/cli_output_flow.c (cli_output.print_truncated, secret_prompt._write,
 * the agent/i18n.py catalog-resolution flows and the agent/i18n_layers.py
 * pure leaves + pack/overlay state flows).
 *
 * Self-contained: includes the port .c by AREA-QUALIFIED name. Reads JSON-line
 * cases from stdin (tests/fixtures/cli_output_flow.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...], "state":{...}}}.
 * Injected state (supported_cache / packs / pack_cache / overlay_cache / out)
 * is mutated in place and echoed back in out.state.
 *
 * Fail-closed: a missing or empty seam script yields a RuntimeError raise spec
 * (recorded as the event ret, then propagated/swallowed by the port exactly as
 * live). A scripted JSON null is delivered as a NULL pointer (Python None).
 * tests/oracle_cli_output_flow.py drives the LIVE Python with the same scripts
 * and compares the ordered seam trace + return + mutated state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/cli_output_flow.c"

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
            char *m = co2_fmt("no script for %s", name);
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
        char *m = co2_fmt("no script for %s", name);
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
    co2_io_t io = {script_call, &ctx};
    json_t *state = NULL;
    json_t *r = NULL;

    if (!strcmp(func, "print_truncated")) {
        r = co2_print_truncated(&io, J("more"), SD("hint", ""));
    } else if (!strcmp(func, "_write")) {
        r = co2_write(&io, SD("text", ""));
    } else if (!strcmp(func, "_current_home")) {
        r = co2_current_home(&io);
    } else if (!strcmp(func, "_config_language")) {
        r = co2_config_language(&io);
    } else if (!strcmp(func, "_resolve_language")) {
        r = co2_resolve_language(&io, SD("home", ""));
    } else if (!strcmp(func, "supported_languages")) {
        state = copy_state(in);
        r = co2_supported_languages(&io, SD("home", ""), state);
    } else if (!strcmp(func, "resolve_language_id")) {
        state = copy_state(in);
        r = co2_resolve_language_id(&io, J("value"), SD("home", ""), state);
    } else if (!strcmp(func, "_load_bundled")) {
        r = co2_load_bundled(&io, SD("lang", ""));
    } else if (!strcmp(func, "surface_catalog")) {
        r = co2_surface_catalog(&io, SD("lang", ""), SD("surface", "core"));
    } else if (!strcmp(func, "language_surfaces")) {
        r = co2_language_surfaces(&io, SD("home", ""));
    } else if (!strcmp(func, "normalize_language_id")) {
        r = co2_normalize_language_id(J("value"));
    } else if (!strcmp(func, "is_language_id")) {
        r = co2_is_language_id(J("value"));
    } else if (!strcmp(func, "flatten")) {
        const json_t *oj = J("out");
        json_t *outobj = (oj && oj->type == JSON_OBJECT) ? json_copy(oj) : NULL;
        r = co2_flatten(J("node"), SD("prefix", ""), outobj);
        state = json_object();
        /* r IS outobj (mutated in place) and is ALSO adopted into out.ret by
         * ret_json below — copy it here or teardown double-frees it. */
        json_set(state, "out", json_copy(r));
    } else if (!strcmp(func, "non_text_leaves")) {
        r = co2_non_text_leaves(J("node"), SD("prefix", ""));
    } else if (!strcmp(func, "overlay_dir")) {
        r = co2_overlay_dir(SD("home", ""));
    } else if (!strcmp(func, "load_locale_source")) {
        r = co2_load_locale_source(&io, J("source"));
    } else if (!strcmp(func, "pack_layer")) {
        state = copy_state(in);
        r = co2_pack_layer(SD("lang", ""), SD("surface", "core"), state);
    } else if (!strcmp(func, "pack_info")) {
        state = copy_state(in);
        r = co2_pack_info(SD("lang", ""), state);
    } else if (!strcmp(func, "overlay_layer")) {
        state = copy_state(in);
        r = co2_overlay_layer(&io, SD("home", ""), SD("lang", ""), SD("surface", "core"), state);
    } else if (!strcmp(func, "scan_locale_dir")) {
        r = co2_scan_locale_dir(&io, SD("directory", ""));
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
