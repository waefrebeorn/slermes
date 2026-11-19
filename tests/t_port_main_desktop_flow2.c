/* t_port_main_desktop_flow2.c — harness for the main_desktop.py UNCLAIMED
 * remainder FLOW port (src/cli/main_desktop_flow2.c).
 *
 * Self-contained: #includes the area-qualified port .c (libjson-only; gate
 * links lib/libjson/json.o alone).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam calls {"name","args"} in "trace"), the final return
 * ("ret", a {"__raise__": ...} node for escaping exceptions incl. SystemExit),
 * the captured real "stdout"/"stderr" text, and — for stateful ports — the
 * mutated state ("state"). The echoed "in" is the ORIGINAL input (the oracle
 * re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim), a LIST consumed in call order (a
 * queue), or {"__script__": true, "ret": <value>, "raise": {type,message,
 * errno?,code?,returncode?,cmd?,output?}}. A missing/exhausted script is a
 * fail-closed RuntimeError on both sides (recorded in the trace first).
 *
 * Effect seams (script entry required as a marker): print appends args.text
 * to the real stdout/stderr capture buffers.
 */
#include "hermes_json.h"
#include "cli/main_desktop_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct { char *s; size_t len, cap; } h_sbuf_t;

static void h_sbuf_init(h_sbuf_t *b) { b->s = NULL; b->len = 0; b->cap = 0; }
static void h_sbuf_append(h_sbuf_t *b, const char *text) {
    if (!text) return;
    size_t n = strlen(text);
    if (b->len + n + 1 > b->cap) {
        size_t nc = (b->cap ? b->cap : 64);
        while (nc < b->len + n + 1) nc *= 2;
        b->s = realloc(b->s, nc);
        b->cap = nc;
    }
    memcpy(b->s + b->len, text, n);
    b->len += n;
    b->s[b->len] = '\0';
}
static void h_sbuf_free(h_sbuf_t *b) { free(b->s); }

typedef struct {
    json_t *io_map;    /* borrowed pin.io */
    json_t *cursors;   /* owned per-name queue cursors */
    h_sbuf_t real;     /* real stdout capture */
    h_sbuf_t err;      /* real stderr capture */
} h_script_ctx_t;

static json_t *h_make_exc(json_t *spec) {
    json_t *e = json_object();
    const char *type = json_get_str(spec, "type", "RuntimeError");
    json_set_hdr(e, "__raise__", type);
    if (strcmp(type, "SystemExit") == 0) {
        json_t *code = json_obj_get(spec, "code");
        json_set(e, "code", code ? json_copy(code) : json_null());
        return e;
    }
    json_set_hdr(e, "message", json_get_str(spec, "message", ""));
    json_t *en = json_obj_get(spec, "errno");
    if (en && en->type == JSON_NUMBER) json_set(e, "errno", json_copy(en));
    json_t *rc = json_obj_get(spec, "returncode");
    if (rc && rc->type == JSON_NUMBER) json_set(e, "returncode", json_copy(rc));
    json_t *cmd = json_obj_get(spec, "cmd");
    if (cmd) json_set(e, "cmd", json_copy(cmd));
    json_t *outp = json_obj_get(spec, "output");
    if (outp) json_set(e, "output", json_copy(outp));
    json_t *errp = json_obj_get(spec, "stderr");
    if (errp) json_set(e, "stderr", json_copy(errp));
    return e;
}

static json_t *h_no_script(const char *name, const char *why) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", "RuntimeError");
    char msg[256];
    snprintf(msg, sizeof msg, "%s for %s", why, name);
    json_set_hdr(e, "message", msg);
    return e;
}

static json_t *h_entry_result(json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return h_make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

static json_t *h_dispatch(md2_io_t *io, const char *name, json_t *args) {
    h_script_ctx_t *sc = (h_script_ctx_t *)io->ctx;
    /* identity echo seam: path.resolve returns the input path (fixture paths
     * are canonical); a script marker is required, scripted raises honored. */
    if (strcmp(name, "path.resolve") == 0) {
        json_t *entry = sc->io_map ? json_obj_get(sc->io_map, name) : NULL;
        if (!entry) return h_no_script(name, "no script");
        if (entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
            json_t *raise = json_obj_get(entry, "raise");
            if (raise && raise->type == JSON_OBJECT) return h_make_exc(raise);
        }
        return json_string(json_get_str(args, "path", ""));
    }
    /* echo/effect seam: script entry required as a marker */
    if (strcmp(name, "print") == 0) {
        json_t *entry = sc->io_map ? json_obj_get(sc->io_map, name) : NULL;
        if (!entry) return h_no_script(name, "no script");
        const char *file = json_get_str(args, "file", "stdout");
        h_sbuf_append(strcmp(file, "stderr") == 0 ? &sc->err : &sc->real,
                      json_get_str(args, "text", ""));
        return json_null();
    }
    json_t *entry = sc->io_map ? json_obj_get(sc->io_map, name) : NULL;
    if (!entry) return h_no_script(name, "no script");
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) return h_no_script(name, "script exhausted");
        json_set(sc->cursors, name, json_number(cur + 1));
        return h_entry_result(json_get(entry, idx));
    }
    return h_entry_result(entry);
}

typedef json_t *(*h_fn_t)(md2_io_t *, json_t *);
/* state kinds: 0 none, 1 in.held_by (the mutated caller list) */
typedef struct { const char *func; h_fn_t fn; int state; } h_case_t;

static const h_case_t CASES[] = {
    { "_packaged_resources_dir", md2_packaged_resources_dir, 0 },
    { "_recover_interrupted_swap", md2_recover_interrupted_swap, 0 },
    { "_move_live_app_aside", md2_move_live_app_aside, 1 },
    { "_discard_desktop_staging", md2_discard_desktop_staging, 0 },
    { "_kernel32", md2_kernel32, 0 },
    { "_runs_from", md2_runs_from, 0 },
    { "_desktop_ancestor_in", md2_desktop_ancestor_in, 0 },
    { "_sandbox_helper_lstat", md2_sandbox_helper_lstat, 0 },
    { "_stage_macos_bundle_copy", md2_stage_macos_bundle_copy, 0 },
    { "_launch_installed_macos_desktop_app", md2_launch_installed_macos_desktop_app, 0 },
    { "_desktop_macos_bundle_id", md2_desktop_macos_bundle_id, 0 },
    { "_desktop_macos_local_signing_identity", md2_desktop_macos_local_signing_identity, 0 },
    { "_codesign_verify", md2_codesign_verify, 0 },
    { "_macos_signature_summary", md2_macos_signature_summary, 0 },
    { "_desktop_macos_has_valid_real_signature", md2_desktop_macos_has_valid_real_signature, 0 },
    { "_macos_legacy_adhoc_resign", md2_macos_legacy_adhoc_resign, 0 },
    { "_app_asar_hash", md2_app_asar_hash, 0 },
    { "_swap_in_new_macos_bundle", md2_swap_in_new_macos_bundle, 0 },
    { "_running_macos_app_bundles", md2_running_macos_app_bundles, 0 },
    { "_update_owned_macos_bundles", md2_update_owned_macos_bundles, 0 },
    { "_owns_installed_desktop_apps", md2_owns_installed_desktop_apps, 0 },
    { "_installed_desktop_apps", md2_installed_desktop_apps, 0 },
    { "_install_rebuilt_macos_bundles", md2_install_rebuilt_macos_bundles, 0 },
    { "_install_rebuilt_desktop_app", md2_install_rebuilt_desktop_app, 0 },
    { "_refresh_installed_desktop_apps", md2_refresh_installed_desktop_apps, 0 },
    { "_installed_desktop_launch_target", md2_installed_desktop_launch_target, 0 },
    { "_promote_staged_desktop_app", md2_promote_staged_desktop_app, 0 },
    { "build_prepared_desktop", md2_build_prepared_desktop, 0 },
    { "_desktop_macos_local_codesign", md2_desktop_macos_local_codesign, 0 },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *h_read_line(FILE *f) {
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
    while ((line = h_read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        json_t *in_owned = NULL;
        if (!in) { in = in_owned = json_object(); }
        json_t *out = NULL;

        const h_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            out = json_object();
            json_set_hdr(out, "error", "unknown func");
        } else {
            md2_io_t io;
            h_script_ctx_t sc;
            io.call = h_dispatch;
            io.trace = json_array();
            io.ctx = &sc;
            json_t *pin = json_copy(in);
            sc.io_map = json_obj_get(pin, "io");
            sc.cursors = json_object();
            h_sbuf_init(&sc.real);
            h_sbuf_init(&sc.err);
            json_t *r = tc->fn(&io, pin);
            out = json_object();
            json_set(out, "ret", r ? r : json_null());
            json_set(out, "trace", json_copy(io.trace));
            json_set_hdr(out, "stdout", sc.real.s ? sc.real.s : "");
            json_set_hdr(out, "stderr", sc.err.s ? sc.err.s : "");
            if (tc->state == 1) {
                json_t *state = json_object();
                json_t *hb = json_obj_get(pin, "held_by");
                json_set(state, "held_by", hb ? json_copy(hb) : json_null());
                json_set(out, "state", state);
            }
            h_sbuf_free(&sc.real);
            h_sbuf_free(&sc.err);
            json_free(pin);
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
        if (in_owned) json_free(in_owned);
        json_free(rec);
    }
    return 0;
}