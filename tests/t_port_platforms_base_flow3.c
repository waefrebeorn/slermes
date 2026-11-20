/*
 * t_port_platforms_base_flow3.c — oracle harness for the UNCLAIMED-remainder
 * FLOW cores of gateway/platforms/base.py ported in
 * src/gateway/platforms/platforms_base_flow3.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * Wires the universal seam to a SCRIPTED recording stub (in["io"][name]: a
 * single value reused, a bare array consumed per call, or
 * {"__script__":true,"ret":X} for a constant list) that appends one
 * {ev,args,ret} trace event per call, in call order. FAIL-CLOSED: a seam with
 * NO script entry returns a RuntimeError raise spec ("unscripted seam:
 * <name>"); a scripted null is a valid None. _validated_delivery_path mutates a
 * COPY of in["dropped"] and _record_delivery a COPY of in["state"]; both emit
 * the mutated copy as out["dropped"] / out["state"]. Emits
 * {"func","in","out":{"ret","exc","trace"[,"dropped"|"state"]}} per case; the
 * original "in" is echoed unmutated. tests/oracle_platforms_base_flow3.py drives
 * the LIVE AST-extracted Python with the same scripts and compares ordered
 * trace + ret + state + exception.
 *
 * Self-contained: #includes the port .c (area-qualified); links only libjson.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/platforms/platforms_base_flow3.c"

typedef struct {
    const json_t *in;
    pb3_trace_t *trace;
    json_t *counts;   /* object: seam name -> call count (array scripts) */
} script_ctx_t;

/* ── generic scripted recording seam (fail-closed on a missing script) ── */
static json_t *sc_call(void *ctx, const char *name, const json_t *args) {
    script_ctx_t *c = ctx;
    const json_t *io = json_obj_get(c->in, "io");
    const json_t *script = io ? json_obj_get(io, name) : NULL;
    const json_t *val = NULL;
    bool fail_closed = false;
    if (!script) {
        fail_closed = true;
    } else if (script->type == JSON_OBJECT && json_get_bool(script, "__script__", false)) {
        val = json_obj_get(script, "ret");
    } else if (script->type == JSON_ARRAY) {
        double cnt = json_get_num(c->counts, name, 0);
        size_t idx = (size_t)cnt;
        if (script->c.count) {
            if (idx >= script->c.count) idx = script->c.count - 1;
            val = script->c.items[idx];
        }
        json_set(c->counts, name, json_number(cnt + 1));
    } else {
        val = script;
    }
    json_t *fc = NULL;
    if (fail_closed) {
        char msg[256];
        snprintf(msg, sizeof(msg), "unscripted seam: %s", name);
        fc = pb3_raise_spec("RuntimeError", msg);
        val = fc;
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args ? json_copy(args) : json_object());
    json_set(e, "ret", val ? json_copy(val) : json_null());
    pb3_trace_add(c->trace, e);
    if (fail_closed) return fc;
    if (!val || val->type == JSON_NULL) return NULL;
    return json_copy(val);
}

/* ── input readers ── */
static const char *in_str(const json_t *in, const char *k, const char *def) { return json_get_str(in, k, def); }
static bool in_bool(const json_t *in, const char *k, bool def) { return json_get_bool(in, k, def); }
static double in_num(const json_t *in, const char *k, double def) { return json_get_num(in, k, def); }
static const json_t *in_node(const json_t *in, const char *k) { return json_obj_get(in, k); }
static json_t *copy_arr(const json_t *in, const char *k) {
    const json_t *n = json_obj_get(in, k);
    if (n && n->type == JSON_ARRAY) return json_copy(n);
    return NULL;   /* absent / null -> None */
}
static json_t *copy_obj_or_null(const json_t *in, const char *k) {
    const json_t *n = json_obj_get(in, k);
    if (n && n->type == JSON_OBJECT) return json_copy(n);
    return NULL;
}

/* ── per-case dispatch ── */
static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, NULL, json_object() };
    pb3_trace_t trace = { NULL };
    ctx.trace = &trace;
    pb3_seams_t s = { &ctx, sc_call };
    json_t *exc = NULL;
    json_t *ret = NULL;
    json_t *dropped = NULL;
    json_t *state = NULL;
    bool has_dropped = false, has_state = false;

    json_set(out, "ret", json_null());
    json_set(out, "exc", json_null());
    json_set(out, "trace", json_array());

    if (strcmp(func, "_cache_dir_accessors") == 0) {
        ret = pb3__cache_dir_accessors(in_str(in, "kind", "image"));
    } else if (strcmp(func, "get_dir") == 0) {
        ret = pb3_cache_dir_get_dir(in_str(in, "new_subpath", "cache/images"),
                                    in_str(in, "old_name", "image_cache"),
                                    in_str(in, "constant_name", "IMAGE_CACHE_DIR"),
                                    in_node(in, "constant_value"),
                                    in_node(in, "import_defaults"), &s, &exc);
    } else if (strcmp(func, "cleanup") == 0) {
        ret = pb3_cache_dir_cleanup(in_str(in, "new_subpath", "cache/images"),
                                    in_str(in, "old_name", "image_cache"),
                                    in_str(in, "constant_name", "IMAGE_CACHE_DIR"),
                                    in_node(in, "constant_value"),
                                    in_node(in, "import_defaults"),
                                    in_num(in, "max_age_hours", 24), &s, &exc);
    } else if (strcmp(func, "validate_media_delivery_path") == 0) {
        ret = pb3_validate_media_delivery_path(in_str(in, "path", ""),
                                               in_str(in, "session_key", ""),
                                               in_bool(in, "hosted_policy_import_fail", false),
                                               in_bool(in, "media_policy_import_fail", false),
                                               &s, &exc);
    } else if (strcmp(func, "_validated_delivery_path") == 0) {
        dropped = copy_arr(in, "dropped");
        has_dropped = true;
        ret = pb3__validated_delivery_path(in_str(in, "raw_path", ""),
                                           in_str(in, "session_key", ""),
                                           in_str(in, "label", "MEDIA directive path"),
                                           dropped,
                                           in_bool(in, "media_fetch_import_fail", false),
                                           &s, &exc);
    } else if (strcmp(func, "_deliverable_tag_spans") == 0) {
        ret = pb3__deliverable_tag_spans(in_str(in, "text", ""), &s, &exc);
    } else if (strcmp(func, "_real_media_tag_spans") == 0) {
        ret = pb3__real_media_tag_spans(in_str(in, "masked", ""), &s, &exc);
    } else if (strcmp(func, "_extensionless_media_matches") == 0) {
        ret = pb3__extensionless_media_matches(in_str(in, "masked", ""), &s, &exc);
    } else if (strcmp(func, "_synthesize_auto_tts") == 0) {
        ret = pb3__synthesize_auto_tts(in_str(in, "name", "telegram"),
                                       in_str(in, "text_content", ""),
                                       in_node(in, "source"),
                                       in_bool(in, "auto_tts_import_fail", false), &s, &exc);
    } else if (strcmp(func, "_record_delivery") == 0) {
        state = copy_obj_or_null(in, "state");
        if (!state) state = json_object();
        has_state = true;
        ret = pb3__record_delivery(state, in_node(in, "result"));
    } else if (strcmp(func, "_opt") == 0) {
        ret = pb3__opt(in_node(in, "value"));
    } else if (strcmp(func, "_extract_response_content") == 0) {
        ret = pb3__extract_response_content(in_str(in, "response", ""),
                                            in_node(in, "source"),
                                            in_str(in, "session_key", ""),
                                            in_bool(in, "is_ephemeral_response", false),
                                            in_str(in, "name", "telegram"),
                                            in_bool(in, "docker_media_import_fail", false),
                                            &s, &exc);
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    if (exc) {
        json_set(out, "ret", json_null());
        if (ret) json_free(ret);
    } else if (ret) {
        json_set(out, "ret", ret);
    } else {
        json_set(out, "ret", json_null());
    }
    json_set(out, "exc", exc ? exc : json_null());
    json_set(out, "trace", trace.events ? trace.events : json_array());
    if (has_dropped) json_set(out, "dropped", dropped ? dropped : json_null());
    if (has_state) json_set(out, "state", state);
    json_free(ctx.counts);
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
            free(err); json_free(c); continue;
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