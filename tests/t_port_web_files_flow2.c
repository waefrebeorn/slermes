/*
 * t_port_web_files_flow2.c — oracle harness for the files.py UNCLAIMED FLOW port
 * in src/cli/web_files_flow2.c (5 funcs: _media_proxy_host_allowed, _fs_looks_binary,
 * _sanitize_chat_image_filename, _decode_data_url, _decode_chat_image_upload).
 *
 * Self-contained: includes the port .c by area-qualified path ("cli/..."). Reads
 * JSON-line cases from stdin, wires the universal seam to the SCRIPTED results in
 * each case's "seams" object (recording one ordered trace event per seam call), and
 * emits {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 *
 * FAIL-CLOSED (identical to the oracle): a MISSING seam script returns a RuntimeError
 * raise spec; a scripted JSON null is a NULL pointer (None), never a json_null node;
 * an ARRAY script is a per-call queue (last value repeats, empty -> None);
 * {"__raise__": cls, "msg": ...} scripts an exception. Byte caps (manage_max_bytes /
 * chat_max_bytes) are fixture-supplied with the live defaults so the 413 branches
 * are reachable. tests/oracle_web_files_flow2.py drives LIVE Python with the same
 * scripts and compares ordered trace + return value.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli/web_files_flow2.c"

typedef struct {
    const json_t *in;
    json_t *trace;
    json_t *counters;
} script_ctx_t;

static json_t *args_drop_null(const json_t *args) {
    json_t *out = json_object();
    if (args && args->type == JSON_OBJECT) {
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
        char *m = wf2_xsf("no script for seam: %s", name);
        result = wf2_raise("RuntimeError", m);
        free(m);
    } else if (sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx;
        size_t n = json_len(sc);
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else {
        result = json_copy(sc);
    }
    /* NULL == None: a scripted null must NOT surface as a json_null node. */
    if (result && result->type == JSON_NULL) {
        json_free(result);
        result = NULL;
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args_drop_null(args));   /* borrowed args: copy, do not free */
    json_set(e, "ret", result ? json_copy(result) : json_null());
    if (!ctx->trace) ctx->trace = json_array();
    json_append(ctx->trace, e);
    return result;
}

#define S(k)  json_get_str(in, k, NULL)
#define D(k, d) json_get_num(in, k, (double)(d))

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, NULL, json_object()};
    wf2_seams_t seams = {&ctx, script_call};
    const wf2_seams_t *sp = &seams;
    json_t *r = NULL;
    char *tmp = NULL;

    if (!strcmp(func, "media_proxy_host_allowed")) {
        r = wf2_media_proxy_host_allowed(S("host") ? S("host") : "");
    } else if (!strcmp(func, "fs_looks_binary")) {
        r = wf2_fs_looks_binary(json_obj_get(in, "data"));
    } else if (!strcmp(func, "sanitize_chat_image_filename")) {
        tmp = wf2_sanitize_chat_image_filename(S("filename"));
        r = json_string(tmp ? tmp : "");
    } else if (!strcmp(func, "decode_data_url")) {
        r = wf2_decode_data_url(S("data_url"), D("manage_max_bytes", 100L * 1024L * 1024L));
    } else if (!strcmp(func, "decode_chat_image_upload")) {
        r = wf2_decode_chat_image_upload(sp, S("data_url"),
                D("manage_max_bytes", 100L * 1024L * 1024L),
                D("chat_max_bytes", 25L * 1024L * 1024L));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }
    free(tmp);
    json_set(out, "ret", r ? r : json_null());
    json_set(out, "trace", ctx.trace ? ctx.trace : json_array());
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
            free(err);
            json_free(c);
            continue;
        }
        const char *func = json_get_str(c, "func", "");
        const json_t *in = json_obj_get(c, "in");
        json_t *empty = json_object();
        json_t *out = json_object();
        /* Reserve key positions: the oracle builds want {ret, trace} and canon()
         * treats insertion order as semantic. */
        json_set(out, "ret", json_null());
        json_set(out, "trace", json_array());
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
