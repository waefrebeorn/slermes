/* t_port_tail_revive2.c — harness for the revival tail group 2 ports.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out = {ret, trace, state}:
 *   ret   — the port's return (NULL -> null; {"__raise__": ...} -> exception)
 *   trace — ordered seam calls {ev, args, ret}
 *   state — the input object minus "io" (these ports are read-only)
 * The echoed "in" is the ORIGINAL input; the oracle re-drives the live
 * Python reference from it under identical scripted seam results.
 *
 * Scripted I/O lives in in.io: seam-name -> result. A result is a plain
 * JSON value (reused), a LIST consumed in call order, or
 * {"__script__": true, "ret": <value>, "raise": {type, message}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The harness #includes the port .c files; the gate links the same
 * objects with --allow-multiple-definition (the included copy wins).
 *
 * Gate:
 *   bash tests/run_one_oracle.sh tail_revive2 oracle_tail_revive2.py "" "src/tools/approval_gaps.o src/hermes_cli/main_gaps.o src/hermes_cli/plugin_dev_gaps.o src/hermes_cli/_subprocess_compat_gaps.o src/agent/message_sanitization_gaps.o src/hermes_cli/local_runtime/catalog_gaps.o src/hermes_cli/local_runtime/context_policy_gaps.o src/hermes_cli/runtime_provider_gaps.o lib/libjson/json.o"
 */
#include "tools/approval_gaps.c"
#include "hermes_cli/main_gaps.c"
#include "hermes_cli/plugin_dev_gaps.c"
#include "hermes_cli/_subprocess_compat_gaps.c"
#include "agent/message_sanitization_gaps.c"
#include "hermes_cli/local_runtime/catalog_gaps.c"
#include "hermes_cli/local_runtime/context_policy_gaps.c"
#include "hermes_cli/runtime_provider_gaps.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct tr2_io {
    json_t *(*call)(struct tr2_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} tr2_io_t;

typedef struct {
    json_t *io_map;   /* borrowed in.io */
    json_t *cursors;  /* per-seam queue cursors */
} tr2_script_t;

static json_t *tr2_make_exc(json_t *spec) {
    json_t *e = json_object();
    const char *ty = json_get_str(spec, "type", "RuntimeError");
    const char *msg = json_get_str(spec, "message", "");
    json_set_hdr(e, "__raise__", ty);
    json_set_hdr(e, "message", msg);
    /* SystemExit(msg).code == msg — the oracle's per-case wrapper adds it */
    if (strcmp(ty, "SystemExit") == 0) json_set_hdr(e, "code", msg);
    return e;
}

static json_t *tr2_raise_spec(const char *type, const char *message) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", message ? message : "");
    return e;
}

static json_t *tr2_entry_result(json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return tr2_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

static json_t *tr2_dispatch(void *ctx, const char *name, json_t *args) {
    (void)args;
    tr2_script_t *sc = (tr2_script_t *)ctx;
    json_t *entry = sc->io_map ? json_obj_get(sc->io_map, name) : NULL;
    if (!entry) {
        char msg[256];
        snprintf(msg, sizeof msg, "no script for %s", name);
        return tr2_raise_spec("RuntimeError", msg);
    }
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) {
            char msg[256];
            snprintf(msg, sizeof msg, "script exhausted for %s", name);
            return tr2_raise_spec("RuntimeError", msg);
        }
        json_set(sc->cursors, name, json_number(cur + 1));
        return tr2_entry_result(json_get(entry, idx));
    }
    return tr2_entry_result(entry);
}

/* Per-file seam adapters: each port file owns its io struct type. */
static json_t *tr2_call_ap2(ap2_io_t *io, const char *n, json_t *a) { return tr2_dispatch(io->ctx, n, a); }
static json_t *tr2_call_mg2(mg2_io_t *io, const char *n, json_t *a) { return tr2_dispatch(io->ctx, n, a); }
static json_t *tr2_call_pd2(pd2_io_t *io, const char *n, json_t *a) { return tr2_dispatch(io->ctx, n, a); }
static json_t *tr2_call_sc2(sc2_io_t *io, const char *n, json_t *a) { return tr2_dispatch(io->ctx, n, a); }
static json_t *tr2_call_ms2(ms2_io_t *io, const char *n, json_t *a) { return tr2_dispatch(io->ctx, n, a); }
static json_t *tr2_call_cat2(cat2_io_t *io, const char *n, json_t *a) { return tr2_dispatch(io->ctx, n, a); }
static json_t *tr2_call_cp2(cp2_io_t *io, const char *n, json_t *a) { return tr2_dispatch(io->ctx, n, a); }
static json_t *tr2_call_rp2(rp2_io_t *io, const char *n, json_t *a) { return tr2_dispatch(io->ctx, n, a); }

#define TR2_WRAP(wname, iotype, fn, adapter)                          \
    static json_t *wname(tr2_io_t *io, json_t *in) {                  \
        iotype a;                                                     \
        a.call = adapter;                                             \
        a.trace = io->trace;                                          \
        a.ctx = io->ctx;                                              \
        return fn(&a, in);                                            \
    }

TR2_WRAP(w_unattended, ap2_io_t, is_unattended_platform_approval_context, tr2_call_ap2)
TR2_WRAP(w_single_query, ap2_io_t, is_single_query_approval_context, tr2_call_ap2)
TR2_WRAP(w_fall_through, ap2_io_t, should_fall_through_to_cli_approval, tr2_call_ap2)
TR2_WRAP(w_spliced, ap2_io_t, is_shell_token_spliced_gateway_lifecycle, tr2_call_ap2)
TR2_WRAP(w_argv, mg2_io_t, argv_is_gateway_run, tr2_call_mg2)
TR2_WRAP(w_userns, mg2_io_t, desktop_linux_userns_sandbox_available, tr2_call_mg2)
TR2_WRAP(w_setuid, mg2_io_t, desktop_linux_needs_disable_setuid_sandbox, tr2_call_mg2)
TR2_WRAP(w_has_manifest, pd2_io_t, _has_manifest, tr2_call_pd2)
TR2_WRAP(w_holds_plugin, pd2_io_t, holds_plugin, tr2_call_pd2)
TR2_WRAP(w_is_plugin_id, pd2_io_t, is_plugin_id, tr2_call_pd2)
TR2_WRAP(w_var_kwargs, pd2_io_t, accepts_var_kwargs, tr2_call_pd2)
TR2_WRAP(w_text_names, sc2_io_t, text_names_hermes, tr2_call_sc2)
TR2_WRAP(w_proc_cmd, sc2_io_t, process_command_is_hermes, tr2_call_sc2)
TR2_WRAP(w_pid_is_hermes, sc2_io_t, pid_is_hermes, tr2_call_sc2)
TR2_WRAP(w_ser_bytes, ms2_io_t, serialized_messages_bytes, tr2_call_ms2)
TR2_WRAP(w_img_reject, ms2_io_t, looks_like_image_content_rejection, tr2_call_ms2)
TR2_WRAP(w_weights, cat2_io_t, weights_bytes, tr2_call_cat2)
TR2_WRAP(w_download, cat2_io_t, download_bytes, tr2_call_cat2)
TR2_WRAP(w_decode, cat2_io_t, predicted_decode_tok_s, tr2_call_cat2)
TR2_WRAP(w_ub_logits, cp2_io_t, ub_logits_bytes, tr2_call_cp2)
TR2_WRAP(w_routable, rp2_io_t, is_routable_provider, tr2_call_rp2)
TR2_WRAP(w_ext_proc, rp2_io_t, is_external_process_provider, tr2_call_rp2)

typedef json_t *(*tr2_fn_t)(tr2_io_t *, json_t *);
typedef struct { const char *func; tr2_fn_t fn; } tr2_case_t;
static const tr2_case_t TR2_CASES[] = {
    { "is_unattended_platform_approval_context", w_unattended },
    { "is_single_query_approval_context", w_single_query },
    { "should_fall_through_to_cli_approval", w_fall_through },
    { "is_shell_token_spliced_gateway_lifecycle", w_spliced },
    { "argv_is_gateway_run", w_argv },
    { "desktop_linux_userns_sandbox_available", w_userns },
    { "desktop_linux_needs_disable_setuid_sandbox", w_setuid },
    { "_has_manifest", w_has_manifest },
    { "holds_plugin", w_holds_plugin },
    { "is_plugin_id", w_is_plugin_id },
    { "accepts_var_kwargs", w_var_kwargs },
    { "text_names_hermes", w_text_names },
    { "process_command_is_hermes", w_proc_cmd },
    { "pid_is_hermes", w_pid_is_hermes },
    { "serialized_messages_bytes", w_ser_bytes },
    { "looks_like_image_content_rejection", w_img_reject },
    { "weights_bytes", w_weights },
    { "download_bytes", w_download },
    { "predicted_decode_tok_s", w_decode },
    { "ub_logits_bytes", w_ub_logits },
    { "is_routable_provider", w_routable },
    { "is_external_process_provider", w_ext_proc },
};
#define TR2_CASE_COUNT (sizeof(TR2_CASES) / sizeof(TR2_CASES[0]))

static char *tr2_read_line(FILE *f) {
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
    while ((line = tr2_read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = NULL;

        const tr2_case_t *tc = NULL;
        for (size_t i = 0; i < TR2_CASE_COUNT; i++) {
            if (strcmp(TR2_CASES[i].func, func) == 0) { tc = &TR2_CASES[i]; break; }
        }
        if (!tc) {
            out = json_object();
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *work = json_copy(in);
            tr2_io_t io;
            tr2_script_t sc;
            io.call = NULL;
            io.trace = json_array();
            io.ctx = &sc;
            sc.io_map = json_obj_get(work, "io");
            sc.cursors = json_object();
            json_t *r = tc->fn(&io, work);
            out = json_object();
            json_set(out, "ret", r ? r : json_null());
            json_set(out, "trace", json_copy(io.trace));
            json_obj_del(work, "io");
            json_set(out, "state", work); /* takes ownership */
            json_free(io.trace);
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
