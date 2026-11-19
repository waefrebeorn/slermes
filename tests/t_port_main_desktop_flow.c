/* t_port_main_desktop_flow.c — harness for the main_desktop.py launch FLOW port.
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
 * errno?,code?}}. A missing/exhausted script is a fail-closed RuntimeError on
 * both sides.
 *
 * State-resolved seams (no script entry): env.contains reads in.os_environ.
 * Echo/effect seams (script entry required as a marker): print (effect:
 * append to the real capture buffers), with_hermes_node_path (returns a copy
 * of in.env — the child-env initial state).
 */
#include "hermes_json.h"
#include "port_main_desktop_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct { char *s; size_t len, cap; } sbuf_t;

static void sbuf_init(sbuf_t *b) { b->s = NULL; b->len = 0; b->cap = 0; }
static void sbuf_append(sbuf_t *b, const char *text) {
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
static void sbuf_free(sbuf_t *b) { free(b->s); }

typedef struct {
    json_t *io_map;
    json_t *cursors;
    json_t *os_environ;  /* borrowed pin.os_environ: env.contains state */
    json_t *env_init;    /* borrowed pin.env: with_hermes_node_path effect */
    sbuf_t real;         /* real stdout capture */
    sbuf_t err;          /* real stderr capture */
} script_ctx_t;

static json_t *make_exc(json_t *spec) {
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
    return e;
}

static json_t *no_script(const char *name, const char *why) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", "RuntimeError");
    char msg[256];
    snprintf(msg, sizeof msg, "%s for %s", why, name);
    json_set_hdr(e, "message", msg);
    return e;
}

static json_t *entry_result(json_t *entry) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

static json_t *dispatch(mdf_io_t *io, const char *name, json_t *args) {
    script_ctx_t *sc = (script_ctx_t *)io->ctx;
    /* state-resolved env membership (recorded by mdf_call; no script) */
    if (strcmp(name, "env.contains") == 0) {
        const char *key = json_get_str(args, "name", "");
        bool has = sc->os_environ && json_has(sc->os_environ, key);
        return json_bool(has);
    }
    /* echo/effect seams: script entry required as a marker */
    if (strcmp(name, "print") == 0) {
        json_t *entry = sc->io_map ? json_obj_get(sc->io_map, name) : NULL;
        if (!entry) return no_script(name, "no script");
        const char *file = json_get_str(args, "file", "stdout");
        sbuf_append(strcmp(file, "stderr") == 0 ? &sc->err : &sc->real,
                    json_get_str(args, "text", ""));
        return json_null();
    }
    if (strcmp(name, "with_hermes_node_path") == 0) {
        json_t *entry = sc->io_map ? json_obj_get(sc->io_map, name) : NULL;
        if (!entry) return no_script(name, "no script");
        return sc->env_init ? json_copy(sc->env_init) : json_object();
    }
    json_t *entry = sc->io_map ? json_obj_get(sc->io_map, name) : NULL;
    if (!entry) return no_script(name, "no script");
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) return no_script(name, "script exhausted");
        json_set(sc->cursors, name, json_number(cur + 1));
        return entry_result(json_get(entry, idx));
    }
    return entry_result(entry);
}

typedef json_t *(*mdf_fn_t)(mdf_io_t *, json_t *);
/* state kinds: 0 none, 1 pin.env, 2 ret[0] (the returned child env) */
typedef struct { const char *func; mdf_fn_t fn; int state; } mdf_case_t;

static const mdf_case_t CASES[] = {
    { "_site_packages_install_kind", mdf_site_packages_install_kind, 0 },
    { "_explicit_profile_args", mdf_explicit_profile_args, 0 },
    { "_diagnose_esbuild_ignore_scripts", mdf_diagnose_esbuild_ignore_scripts, 0 },
    { "_macos_signing_downgrade_error", mdf_macos_signing_downgrade_error, 0 },
    { "_sandbox_helper_is_setuid_root", mdf_sandbox_helper_is_setuid_root, 0 },
    { "_windows_native_machine", mdf_windows_native_machine, 0 },
    { "_windows_user_runnable_pe_machines", mdf_windows_user_runnable_pe_machines, 0 },
    { "_expected_windows_pe_machines", mdf_expected_windows_pe_machines, 0 },
    { "_parse_pe_machine", mdf_parse_pe_machine, 0 },
    { "_packaged_node_pty_missing", mdf_packaged_node_pty_missing, 0 },
    { "_packaged_desktop_current_for_head", mdf_packaged_desktop_current_for_head, 0 },
    { "_prefer_wsl_d3d12", mdf_prefer_wsl_d3d12, 1 },
    { "_desktop_launch_env", mdf_desktop_launch_env, 2 },
    { "_check_desktop_skip_build", mdf_check_desktop_skip_build, 0 },
    { "_packaged_desktop_launch_command", mdf_packaged_desktop_launch_command, 0 },
    { "_run_desktop_one_shot", mdf_run_desktop_one_shot, 0 },
    { "_launch_bundled_desktop", mdf_launch_bundled_desktop, 0 },
    { "_rename_riding_out_file_lock", mdf_rename_riding_out_file_lock, 0 },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *read_line(FILE *f) {
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
    while ((line = read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        json_t *in_owned = NULL;
        if (!in) { in = in_owned = json_object(); }
        json_t *out = NULL;

        const mdf_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            out = json_object();
            json_set_hdr(out, "error", "unknown func");
        } else {
            mdf_io_t io;
            script_ctx_t sc;
            io.call = dispatch;
            io.trace = json_array();
            io.ctx = &sc;
            json_t *pin = json_copy(in);
            sc.io_map = json_obj_get(in, "io");
            sc.cursors = json_object();
            sc.os_environ = json_obj_get(pin, "os_environ");
            sc.env_init = json_obj_get(pin, "env");
            sbuf_init(&sc.real);
            sbuf_init(&sc.err);
            json_t *r = tc->fn(&io, pin);
            out = json_object();
            json_set(out, "ret", r ? r : json_null());
            json_set(out, "trace", json_copy(io.trace));
            json_set_hdr(out, "stdout", sc.real.s ? sc.real.s : "");
            json_set_hdr(out, "stderr", sc.err.s ? sc.err.s : "");
            if (tc->state == 1) {
                json_t *state = json_object();
                json_t *env = json_obj_get(pin, "env");
                json_set(state, "env", env ? json_copy(env) : json_object());
                json_set(out, "state", state);
            } else if (tc->state == 2) {
                json_t *state = json_object();
                json_t *env = (r && r->type == JSON_ARRAY) ? json_get(r, 0) : NULL;
                json_set(state, "env", env ? json_copy(env) : json_object());
                json_set(out, "state", state);
            }
            sbuf_free(&sc.real);
            sbuf_free(&sc.err);
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
