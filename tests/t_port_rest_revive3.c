/* t_port_rest_revive3.c — harness for the rest group-3 deep revival ports.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out = {ret, trace, state}:
 *   ret   — the port's return (NULL -> null; {"__raise__": ...} -> exception)
 *   trace — ordered seam calls {ev, args, ret}
 *   state — the mutated input object minus "io"
 * The echoed "in" is the ORIGINAL input; the oracle re-drives the live
 * Python reference from it under identical scripted seam results.
 *
 * Scripted I/O lives in in.io: seam-name -> result. A result is a plain
 * JSON value (reused), a LIST consumed in call order, or
 * {"__script__": true, "ret": <value>, "raise": {type, message}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * Gate (explicit objs; the harness #includes the five libjson-only port
 * sources, so ONLY libjson is linked):
 *   bash tests/run_one_oracle.sh rest_revive3 oracle_rest_revive3.py "" "lib/libjson/json.o"
 */
#include "gateway/platforms/base_gaps.c"
#include "tools/skill_ledger_gaps.c"
#include "hermes_cli/local_runtime/hf_browse_gaps.c"
#include "tools/bot_relay_gaps.c"
#include "gateway/kanban_watchers_gaps.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    json_t *io_map;   /* borrowed in.io */
    json_t *cursors;  /* per-seam queue cursors */
} r3_script_t;

static json_t *r3_make_exc(json_t *spec) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", json_get_str(spec, "type", "RuntimeError"));
    json_set_hdr(e, "message", json_get_str(spec, "message", ""));
    return e;
}

static json_t *r3_entry_result(json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return r3_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

static json_t *r3_raise_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message);
    return e;
}

/* Script lookup shared by every per-file dispatcher. */
static json_t *r3_dispatch_impl(void *ctx, const char *name, json_t *args) {
    (void)args;
    r3_script_t *sc = (r3_script_t *)ctx;
    json_t *entry = sc->io_map ? json_obj_get(sc->io_map, name) : NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        return r3_raise_spec("RuntimeError", msg);
    }
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            return r3_raise_spec("RuntimeError", msg);
        }
        json_set(sc->cursors, name, json_number(cur + 1));
        return r3_entry_result(json_get(entry, idx));
    }
    return r3_entry_result(entry);
}

/* One type-correct dispatcher per port io type. */
#define R3_DISP(P) \
    static json_t *P##_disp(P##_io_t *io, const char *name, json_t *args) { \
        return r3_dispatch_impl(io->ctx, name, args); \
    }

R3_DISP(bm)
R3_DISP(sl)
R3_DISP(hb)
R3_DISP(br)
R3_DISP(kw)

/* One wrapper per case; pure ports take void* io and ignore it. */
#define R3_WRAP(NAME, P, FN) \
    static json_t *wrap_##NAME(json_t *trace, void *sc, json_t *work) { \
        P##_io_t io; \
        io.call = P##_disp; \
        io.trace = trace; \
        io.ctx = sc; \
        return FN(&io, work); \
    }
#define R3_WRAP_PURE(NAME, FN) \
    static json_t *wrap_##NAME(json_t *trace, void *sc, json_t *work) { \
        (void)trace; (void)sc; \
        return FN(NULL, work); \
    }

R3_WRAP(translate_docker_container_media_path, bm, translate_docker_container_media_path)
R3_WRAP(transcode_to_ogg_opus, bm, transcode_to_ogg_opus)
R3_WRAP(docker_sandbox_dir_candidates, bm, docker_sandbox_dir_candidates)
R3_WRAP(default_docker_workspace_host_roots, bm, default_docker_workspace_host_roots)
R3_WRAP(fill_snapshot_from_curator_backup, sl, fill_snapshot_from_curator_backup)
R3_WRAP(package_prefixes, sl, package_prefixes)
R3_WRAP(package_rel, sl, package_rel)
R3_WRAP_PURE(strip_archive_timestamp, strip_archive_timestamp)
R3_WRAP(repo_files, hb, repo_files)
R3_WRAP(search_models, hb, search_models)
R3_WRAP_PURE(rough_fit, rough_fit)
R3_WRAP_PURE(quant_label, quant_label)
R3_WRAP(priced_repo_files, hb, priced_repo_files)
R3_WRAP(_hermes_cli, br, _hermes_cli)
R3_WRAP(wake_scope_id, kw, wake_scope_id)
R3_WRAP(safe_review_reason, kw, safe_review_reason)

typedef json_t *(*r3_fn_t)(json_t *, void *, json_t *);
typedef struct { const char *func; r3_fn_t fn; } r3_case_t;

static const r3_case_t R3_CASES[] = {
    { "translate_docker_container_media_path", wrap_translate_docker_container_media_path },
    { "transcode_to_ogg_opus", wrap_transcode_to_ogg_opus },
    { "docker_sandbox_dir_candidates", wrap_docker_sandbox_dir_candidates },
    { "default_docker_workspace_host_roots", wrap_default_docker_workspace_host_roots },
    { "fill_snapshot_from_curator_backup", wrap_fill_snapshot_from_curator_backup },
    { "package_prefixes", wrap_package_prefixes },
    { "package_rel", wrap_package_rel },
    { "strip_archive_timestamp", wrap_strip_archive_timestamp },
    { "repo_files", wrap_repo_files },
    { "search_models", wrap_search_models },
    { "rough_fit", wrap_rough_fit },
    { "quant_label", wrap_quant_label },
    { "priced_repo_files", wrap_priced_repo_files },
    { "_hermes_cli", wrap__hermes_cli },
    { "wake_scope_id", wrap_wake_scope_id },
    { "safe_review_reason", wrap_safe_review_reason },
};
#define R3_CASE_COUNT (sizeof(R3_CASES) / sizeof(R3_CASES[0]))

static char *r3_read_line(FILE *f) {
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    int c;
    while ((c = fgetc(f)) != EOF && c != '\n') {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        buf[len++] = (char)c;
    }
    if (c == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    char *line;
    while ((line = r3_read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = NULL;

        const r3_case_t *tc = NULL;
        for (size_t i = 0; i < R3_CASE_COUNT; i++) {
            if (strcmp(R3_CASES[i].func, func) == 0) { tc = &R3_CASES[i]; break; }
        }
        if (!tc) {
            out = json_object();
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *work = json_copy(in);
            r3_script_t sc;
            sc.io_map = json_obj_get(work, "io");
            sc.cursors = json_object();
            json_t *trace = json_array();
            json_t *r = tc->fn(trace, &sc, work);
            out = json_object();
            json_set(out, "ret", r ? r : json_null());
            json_set(out, "trace", json_copy(trace));
            json_obj_del(work, "io");
            json_set(out, "state", work); /* takes ownership */
            json_free(trace);
            json_free(sc.cursors);
        }

        json_t *line_out = json_object();
        json_set_hdr(line_out, "func", func);
        json_set(line_out, "in", json_copy(in));
        json_set(line_out, "out", out ? out : json_null());
        char *ser = json_serialize(line_out);
        printf("%s\n", ser ? ser : "{}");
        free(ser);
        json_free(line_out);
        json_free(rec);
    }
    return 0;
}