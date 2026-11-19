/* t_port_transcription_flow.c — harness for the UNCLAIMED transcription-surface
 * FLOW port (src/tools/transcription_flow.c): tools/transcription_common.py +
 * tools/transcription_chunking.py + tools/transcription_whisper_cpp.py (the
 * task's "tools/transcription.py" is that family; the file does not exist).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...args...,
 * "state": {"verified_files": {path: [digest,ino,size,mtime_ns,ctime_ns]}},
 * "io": {seam: script}, "pm_ok": bool, "pm_downloader_ok": bool}}.
 * Emits JSON LINES: {"func","in","out":{"ret","trace","state"}}. "ret" is the
 * canonical return (tuples -> arrays, None -> null, uncaught exception ->
 * {"__raise__": cls}); "trace" is the ordered seam-call sequence
 * [{"ev","args","ret"}]; "state" is {"verified_files": {...}} on success and
 * {} on any propagating raise. The oracle drives the LIVE quarry defs under
 * the SAME scripts and diffs out.
 *
 * Self-contained: #includes the area-qualified port .c (libjson-only).
 * Gate: bash tests/run_one_oracle.sh transcription_flow \
 *           oracle_transcription_flow.py "" "lib/libjson/json.o"
 */
#include "hermes_json.h"
#include "tools/transcription_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* materialize the module state object from in.state (owned) */
static json_t *trh_state(const json_t *in) {
    json_t *st = json_object();
    const json_t *src = tr_get(in, "state");
    json_t *vf = json_object();
    const json_t *sv = tr_get(src, "verified_files");
    if (tr_is_obj(sv))
        for (size_t i = 0; i < sv->c.count; i++)
            json_set(vf, sv->c.keys[i], json_copy(sv->c.items[i]));
    json_set(st, "verified_files", vf);
    return st;
}

static json_t *trh_run(tr_ctx *ctx, const char *func, const json_t *in) {
    const char *path = json_get_str(in, "path", "");
    const char *provider = json_get_str(in, "provider", "");
    const char *key = json_get_str(in, "key", "");
    const char *extra = json_get_str(in, "extra", "");
    const char *label = json_get_str(in, "label", "");
    const char *reason = json_get_str(in, "reason", "");
    const char *ffmpeg = json_get_str(in, "ffmpeg", "");
    const char *work_dir = json_get_str(in, "work_dir", "");
    const char *digest = json_get_str(in, "digest", "");
    const char *model_name = json_get_str(in, "model_name", "");
    const char *cast_seam = json_get_str(in, "cast_seam", "cast");
    const char *dispatch_seam = json_get_str(in, "dispatch_seam", "dispatch");
    const json_t *model = tr_get(in, "model");
    const json_t *limit = tr_get(in, "limit");
    const json_t *duration = tr_get(in, "duration");
    const json_t *silences = tr_get(in, "silences");
    const json_t *cuts = tr_get(in, "cuts");
    const json_t *cfg = tr_get(in, "cfg");
    const json_t *exc = tr_get(in, "exc");
    const json_t *error = tr_get(in, "error");
    const json_t *extra_kw = tr_get(in, "extra_kw");
    const json_t *transcript = tr_get(in, "transcript");

    if (tr_str_eq(func, "_error_result")) return tr_error_result(ctx, error, extra_kw);
    if (tr_str_eq(func, "normalize_xai_stt_model")) return tr_normalize_xai_stt_model(ctx, model);
    if (tr_str_eq(func, "_ok_result")) return tr_ok_result(ctx, transcript, tr_get(in, "provider"));
    if (tr_str_eq(func, "_lazy_ensure_quietly")) return tr_lazy_ensure_quietly(ctx, extra);
    if (tr_str_eq(func, "_process_error_detail")) return tr_process_error_detail(ctx, exc);
    if (tr_str_eq(func, "_log_prompt_unsupported")) return tr_log_prompt_unsupported(ctx, label);
    if (tr_str_eq(func, "_config_number")) {
        json_t *defn = json_copy(tr_get_def(in, "default", NULL));
        if (!defn) defn = json_null();
        json_t *r = tr_config_number(ctx, cfg, key, defn, cast_seam);
        json_free(defn);
        return r;
    }
    if (tr_str_eq(func, "upload_limit")) return tr_upload_limit(ctx, provider, model);
    if (tr_str_eq(func, "_fits")) return tr_fits(ctx, path, limit, duration);
    if (tr_str_eq(func, "exceeds_upload_limit")) return tr_exceeds_upload_limit(ctx, path, limit);
    if (tr_str_eq(func, "_too_large"))
        return tr_too_large(ctx, path, json_get_num(in, "max_bytes", 0), reason);
    if (tr_str_eq(func, "_silence_midpoints")) return tr_silence_midpoints(ctx, ffmpeg, path);
    if (tr_str_eq(func, "_cut_points"))
        return tr_cut_points(ctx, json_get_num(in, "duration", 0),
                             json_get_num(in, "target", 0), silences);
    if (tr_str_eq(func, "_split")) return tr_split(ctx, ffmpeg, path, work_dir, cuts);
    if (tr_str_eq(func, "transcribe_oversized"))
        return tr_transcribe_oversized(ctx, path, limit, dispatch_seam);
    if (tr_str_eq(func, "_model_dir")) return tr_model_dir(ctx);
    if (tr_str_eq(func, "_file_signature")) return tr_file_signature(ctx, path, digest);
    if (tr_str_eq(func, "whisper_cpp_command")) return tr_whisper_cpp_command(ctx);
    if (tr_str_eq(func, "ensure_whisper_cpp_models"))
        return tr_ensure_whisper_cpp_models(ctx, model_name);
    return NULL;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    static char line[1 << 20];
    while (fgets(line, sizeof(line), stdin)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '\r') continue;
        json_t *rec = json_parse(line, NULL);
        if (!rec) { fprintf(stderr, "harness: unparseable case line\n"); return 1; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        json_t *empty_in = NULL;
        if (!in) { empty_in = json_object(); in = empty_in; }
        tr_ctx ctx;
        ctx.io = json_obj_get(in, "io");
        ctx.state = trh_state(in);
        ctx.trace = json_array();
        ctx.counters = json_object();
        ctx.exc = NULL;
        ctx.pm_ok = json_get_bool(in, "pm_ok", true);
        ctx.pm_downloader_ok = json_get_bool(in, "pm_downloader_ok", true);
        json_t *ret = trh_run(&ctx, func, in);
        json_t *ret_out;
        json_t *state_out;
        if (ctx.exc) {
            ret_out = json_object();
            json_set(ret_out, "__raise__", json_string(tr_exc_class(&ctx)));
            json_free(ctx.exc);
            json_free(ret);
            json_free(ctx.state);
            state_out = json_object();  /* {} on any raise (both sides) */
        } else {
            ret_out = ret ? ret : json_null();
            state_out = json_object();
            json_set(state_out, "verified_files",
                     json_copy(tr_get(ctx.state, "verified_files")));
            json_free(ctx.state);
        }
        json_t *out = json_object();
        json_set(out, "ret", ret_out);
        json_set(out, "trace", ctx.trace);
        json_set(out, "state", state_out);
        json_t *emit = json_object();
        json_set(emit, "func", json_string(func));
        json_set(emit, "in", json_copy(in));
        json_set(emit, "out", out);
        char *s = json_serialize(emit);
        if (s) { fputs(s, stdout); fputc('\n', stdout); free(s); }
        json_free(emit);
        json_free(ctx.counters);
        json_free(empty_in);
        json_free(rec);
    }
    return 0;
}
