/*
 * t_port_platforms_base_flow2.c — oracle harness for the UNCLAIMED-remainder
 * FLOW cores of gateway/platforms/base.py ported in
 * src/gateway/platforms/platforms_base_flow2.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * Wires the universal seam to a SCRIPTED recording stub (in["io"][name]: a
 * single value reused, a bare array consumed per call, or
 * {"__script__":true,"ret":X} for a constant list) that appends one
 * {ev,args,ret} trace event per call, in call order. FAIL-CLOSED: a seam
 * with NO script entry returns a RuntimeError raise spec ("unscripted seam:
 * <name>"); a scripted null is a valid None. _session_task_is_stale runs on
 * a COPY of in["self"] and emits it (read-only flow) as out["self"]. Emits
 * {"func","in","out":{"ret","exc","trace"[,"self"]}} per case; the original
 * "in" is echoed unmutated. tests/oracle_platforms_base_flow2.py drives the
 * LIVE AST-extracted Python with the same scripts and compares ordered
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

#include "gateway/platforms/platforms_base_flow2.c"

typedef struct {
    const json_t *in;
    pb2_trace_t *trace;
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
        /* no script entry: fail closed with a RuntimeError raise spec */
        fail_closed = true;
    } else if (script->type == JSON_OBJECT && json_get_bool(script, "__script__", false)) {
        val = json_obj_get(script, "ret");   /* constant (list) result */
    } else if (script->type == JSON_ARRAY) {
        double cnt = json_get_num(c->counts, name, 0);
        size_t idx = (size_t)cnt;
        if (script->c.count) {
            if (idx >= script->c.count) idx = script->c.count - 1;
            val = script->c.items[idx];
        }
        json_set(c->counts, name, json_number(cnt + 1));
    } else {
        val = script;   /* single value reused (NULL when absent) */
    }
    json_t *fc = NULL;
    if (fail_closed) {
        char msg[256];
        snprintf(msg, sizeof(msg), "unscripted seam: %s", name);
        fc = pb2_raise_spec("RuntimeError", msg);
        val = fc;
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args ? json_copy(args) : json_object());
    json_set(e, "ret", val ? json_copy(val) : json_null());
    pb2_trace_add(c->trace, e);
    if (fail_closed) return fc;   /* owned raise spec */
    /* a scripted JSON null returns a NULL pointer (None), not a json_null node */
    if (!val || val->type == JSON_NULL) return NULL;
    return json_copy(val);
}

/* ── input readers ── */
static const json_t *in_node(const json_t *in, const char *k) { return json_obj_get(in, k); }
static const char *in_str(const json_t *in, const char *k, const char *def) { return json_get_str(in, k, def); }
static bool in_bool(const json_t *in, const char *k, bool def) { return json_get_bool(in, k, def); }
static double in_num(const json_t *in, const char *k, double def) { return json_get_num(in, k, def); }
static const char *in_str_or_null(const json_t *in, const char *k) {
    const json_t *n = json_obj_get(in, k);
    return (n && n->type == JSON_STRING) ? n->str_val : NULL;
}
/* Path-string input materialization: PurePosixPath normalization (the oracle
 * drive builds RecPath doubles, whose str() is normalized). */
static char *in_path(const json_t *in, const char *k, const char *def) {
    return pb2_posix_norm(in_str(in, k, def));
}
static json_t *copy_obj(const json_t *in, const char *k) {
    const json_t *n = json_obj_get(in, k);
    json_t *c = n ? json_copy(n) : json_object();
    if (!c || c->type != JSON_OBJECT) { json_free(c); c = json_object(); }
    return c;
}

/* ── per-case dispatch ── */
static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = { in, NULL, json_object() };
    pb2_trace_t trace = { NULL };
    ctx.trace = &trace;
    pb2_seams_t s = { &ctx, sc_call };
    json_t *exc = NULL;
    json_t *ret = NULL;
    json_t *self = NULL;
    bool has_self = false;

    /* reserve envelope positions (ret, exc, trace) before state keys */
    json_set(out, "ret", json_null());
    json_set(out, "exc", json_null());
    json_set(out, "trace", json_array());

    if (strcmp(func, "_secure_media_cache_dir") == 0) {
        char *cd = in_path(in, "cache_dir", "/cache");
        ret = pb2__secure_media_cache_dir(cd, in_bool(in, "config_import_fail", false), &s, &exc);
        free(cd);
    } else if (strcmp(func, "_write_cache_file") == 0) {
        char *cd = in_path(in, "cache_dir", "/cache");
        ret = pb2__write_cache_file(cd, in_str(in, "prefix", "img"), in_str(in, "ext", ".jpg"),
                                    in_str(in, "data", ""), &s, &exc);
        free(cd);
    } else if (strcmp(func, "cache_image_from_bytes") == 0) {
        ret = pb2_cache_image_from_bytes(in_str(in, "data", ""), in_str(in, "ext", ".jpg"), &s, &exc);
    } else if (strcmp(func, "cache_image_from_bytes_async") == 0) {
        ret = pb2_cache_image_from_bytes_async(in_str(in, "data", ""), in_str(in, "ext", ".jpg"), &s, &exc);
    } else if (strcmp(func, "cache_audio_from_bytes") == 0) {
        ret = pb2_cache_audio_from_bytes(in_str(in, "data", ""), in_str(in, "ext", ".ogg"),
                                         in_bool(in, "audio_import_fail", false), &s, &exc);
    } else if (strcmp(func, "cache_audio_from_bytes_async") == 0) {
        ret = pb2_cache_audio_from_bytes_async(in_str(in, "data", ""), in_str(in, "ext", ".ogg"), &s, &exc);
    } else if (strcmp(func, "cache_video_from_bytes") == 0) {
        ret = pb2_cache_video_from_bytes(in_str(in, "data", ""), in_str(in, "ext", ".mp4"), &s, &exc);
    } else if (strcmp(func, "cache_video_from_bytes_async") == 0) {
        ret = pb2_cache_video_from_bytes_async(in_str(in, "data", ""), in_str(in, "ext", ".mp4"), &s, &exc);
    } else if (strcmp(func, "cache_document_from_bytes") == 0) {
        ret = pb2_cache_document_from_bytes(in_str(in, "data", ""), in_str(in, "filename", ""), &s, &exc);
    } else if (strcmp(func, "cache_document_from_bytes_async") == 0) {
        ret = pb2_cache_document_from_bytes_async(in_str(in, "data", ""), in_str(in, "filename", ""), &s, &exc);
    } else if (strcmp(func, "_resolve_media_ext") == 0) {
        ret = pb2__resolve_media_ext(in_str(in, "filename", ""), in_str(in, "mime_type", ""));
    } else if (strcmp(func, "cache_media_bytes") == 0) {
        ret = pb2_cache_media_bytes(in_str(in, "data", ""), in_str(in, "filename", ""),
                                    in_str(in, "mime_type", ""), in_str_or_null(in, "default_kind"), &s, &exc);
    } else if (strcmp(func, "cache_media_bytes_async") == 0) {
        ret = pb2_cache_media_bytes_async(in_str(in, "data", ""), in_str(in, "filename", ""),
                                          in_str(in, "mime_type", ""), in_str_or_null(in, "default_kind"), &s, &exc);
    } else if (strcmp(func, "_sqlite_files") == 0) {
        ret = pb2__sqlite_files(in_str(in, "name", ""));
    } else if (strcmp(func, "_docker_env_active") == 0) {
        ret = pb2__docker_env_active(&s, &exc);
    } else if (strcmp(func, "_docker_persistent_active") == 0) {
        ret = pb2__docker_persistent_active(&s, &exc);
    } else if (strcmp(func, "_cache_dir_container_mounts") == 0) {
        ret = pb2__cache_dir_container_mounts(in_bool(in, "cred_import_fail", false), &s, &exc);
    } else if (strcmp(func, "_media_delivery_recency_seconds") == 0) {
        ret = pb2__media_delivery_recency_seconds(in_bool(in, "policy_import_fail", false), &s, &exc);
    } else if (strcmp(func, "_resolve_path") == 0) {
        char *p = in_path(in, "path", "");
        ret = pb2__resolve_path(p, in_bool(in, "strict", false), in_bool(in, "expand", false), &s, &exc);
        free(p);
    } else if (strcmp(func, "_parse_docker_volume_mounts") == 0) {
        ret = pb2__parse_docker_volume_mounts(&s, &exc);
    } else if (strcmp(func, "_profile_dirs") == 0) {
        char *hr = in_path(in, "hermes_root", "/root/.hermes");
        ret = pb2__profile_dirs(hr, &s, &exc);
        free(hr);
    } else if (strcmp(func, "_credential_home_roots") == 0) {
        char *hr = in_path(in, "hermes_root", "/root/.hermes");
        ret = pb2__credential_home_roots(hr, &s, &exc);
        free(hr);
    } else if (strcmp(func, "_existing_regular_file") == 0) {
        ret = pb2__existing_regular_file(in_str(in, "raw", ""), &s, &exc);
    } else if (strcmp(func, "_mask_media_scan_text") == 0) {
        ret = pb2__mask_media_scan_text(in_str(in, "text", ""), &s, &exc);
    } else if (strcmp(func, "_session_task_is_stale") == 0) {
        self = copy_obj(in, "self");
        has_self = true;
        ret = pb2__session_task_is_stale(self, in_str(in, "session_key", ""), &s, &exc);
    } else if (strcmp(func, "truncate_message") == 0) {
        const json_t *lf = json_obj_get(in, "len_fn");
        ret = pb2_truncate_message(in_str(in, "content", ""), in_num(in, "max_length", 4096),
                                   lf && lf->type != JSON_NULL,
                                   in_bool(in, "chunking_import_fail", false), &s, &exc);
    } else if (strcmp(func, "supports_exec_approval_buttons") == 0) {
        ret = pb2_supports_exec_approval_buttons(in_bool(in, "override", false));
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    /* ret envelope: on a propagated exception the reference projects ret=None */
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
    if (has_self) json_set(out, "self", self);
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