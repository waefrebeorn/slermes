/* t_port_main_cli_flow.c — harness for the main.py CLI-dispatch FLOW port.
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
 * errno?,code?}, "stdout": <text>, "stderr": <text>}. "stdout"/"stderr" are
 * the parser-write effects of parser.parse_args: routed to the speculative
 * buffer when args.speculative is true (speculative stderr is discarded, as
 * redirect_stderr discards it live), else to the real capture buffers.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * State-resolved seams (no script entry): env.get/env.pop read/mutate in.env.
 * Echo seams (script entry required as a marker): print (effect: append to
 * the real capture buffers), stringio.getvalue (returns the speculative
 * buffer).
 */
#include "hermes_json.h"
#include "port_main_cli_flow.h"

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
    json_t *env;      /* borrowed pin.env: env.get/env.pop state */
    sbuf_t spec;      /* speculative stdout (redirect region) */
    sbuf_t real;      /* real stdout capture */
    sbuf_t err;       /* real stderr capture */
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

static json_t *entry_result(json_t *entry, const char *name, json_t *args, script_ctx_t *sc) {
    if (entry && entry->type == JSON_OBJECT && json_get_bool(entry, "__script__", false)) {
        if (strcmp(name, "parser.parse_args") == 0) {
            /* argparse writes diagnostics/help BEFORE exiting */
            bool spec = json_get_bool(args, "speculative", false);
            json_t *so = json_obj_get(entry, "stdout");
            if (so && so->type == JSON_STRING)
                sbuf_append(spec ? &sc->spec : &sc->real, so->str_val);
            json_t *se = json_obj_get(entry, "stderr");
            if (se && se->type == JSON_STRING && !spec)
                sbuf_append(&sc->err, se->str_val);
        }
        json_t *raise = json_obj_get(entry, "raise");
        if (raise && raise->type == JSON_OBJECT) return make_exc(raise);
        json_t *ret = json_obj_get(entry, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return entry ? json_copy(entry) : json_null();
}

static json_t *dispatch(mcl_io_t *io, const char *name, json_t *args) {
    script_ctx_t *sc = (script_ctx_t *)io->ctx;
    /* state-resolved env seams (recorded by mcl_call; no script) */
    if (strcmp(name, "env.get") == 0) {
        const char *key = json_get_str(args, "name", "");
        json_t *v = sc->env ? json_obj_get(sc->env, key) : NULL;
        if (v) return json_copy(v);
        json_t *def = json_obj_get(args, "default");
        return def ? json_copy(def) : json_null();
    }
    if (strcmp(name, "env.pop") == 0) {
        const char *key = json_get_str(args, "name", "");
        json_t *v = sc->env ? json_obj_get(sc->env, key) : NULL;
        if (v) {
            json_t *out = json_copy(v);
            json_obj_del(sc->env, key);
            return out;
        }
        json_t *def = json_obj_get(args, "default");
        return def ? json_copy(def) : json_null();
    }
    /* echo/effect seams: script entry required as a marker */
    if (strcmp(name, "print") == 0 || strcmp(name, "stringio.getvalue") == 0) {
        json_t *entry = sc->io_map ? json_obj_get(sc->io_map, name) : NULL;
        if (!entry) return no_script(name, "no script");
        if (strcmp(name, "print") == 0) {
            const char *file = json_get_str(args, "file", "stdout");
            sbuf_append(strcmp(file, "stderr") == 0 ? &sc->err : &sc->real,
                        json_get_str(args, "text", ""));
            return json_null();
        }
        return json_string(sc->spec.s ? sc->spec.s : "");
    }
    json_t *entry = sc->io_map ? json_obj_get(sc->io_map, name) : NULL;
    if (!entry) return no_script(name, "no script");
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) return no_script(name, "script exhausted");
        json_set(sc->cursors, name, json_number(cur + 1));
        return entry_result(json_get(entry, idx), name, args, sc);
    }
    return entry_result(entry, name, args, sc);
}

typedef json_t *(*mcl_fn_t)(mcl_io_t *, json_t *);
/* state kinds: 0 none, 1 env, 2 args, 3 subparsers.required */
typedef struct { const char *func; mcl_fn_t fn; int state; } mcl_case_t;

static const mcl_case_t CASES[] = {
    { "_early_interface_config_path", mcl_early_interface_config_path, 0 },
    { "_resolve_sudo_user_profile_env", mcl_resolve_sudo_user_profile_env, 0 },
    { "_under_gateway_supervisor", mcl_under_gateway_supervisor, 0 },
    { "_s6_supervised_gateway_run", mcl_s6_supervised_gateway_run, 0 },
    { "explicit_cli_profile", mcl_explicit_cli_profile, 0 },
    { "_read_packed_ref", mcl_read_packed_ref, 0 },
    { "_dotenv_has_provider_key", mcl_dotenv_has_provider_key, 0 },
    { "_latest_session_id", mcl_latest_session_id, 0 },
    { "_first_run_setup_guard", mcl_first_run_setup_guard, 0 },
    { "_finalize_update_receipt", mcl_finalize_update_receipt, 0 },
    { "_update_preflight_handled", mcl_update_preflight_handled, 0 },
    { "_dashboard_lifecycle_flags", mcl_dashboard_lifecycle_flags, 0 },
    { "_dashboard_sanitize_desktop_env", mcl_dashboard_sanitize_desktop_env, 1 },
    { "_require_dashboard_web_deps", mcl_require_dashboard_web_deps, 0 },
    { "_guard_noninteractive_user_config", mcl_guard_noninteractive_user_config, 2 },
    { "_is_tui_chat_launch", mcl_is_tui_chat_launch, 0 },
    { "_default_to_chat", mcl_default_to_chat, 0 },
    { "_parse_cli_args", mcl_parse_cli_args, 3 },
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

        const mcl_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            out = json_object();
            json_set_hdr(out, "error", "unknown func");
        } else {
            mcl_io_t io;
            script_ctx_t sc;
            io.call = dispatch;
            io.trace = json_array();
            io.ctx = &sc;
            sc.io_map = json_obj_get(in, "io");
            sc.cursors = json_object();
            json_t *pin = json_copy(in);
            sc.env = json_obj_get(pin, "env");
            sbuf_init(&sc.spec);
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
                json_t *args = json_obj_get(pin, "args");
                json_set(state, "args", args ? json_copy(args) : json_object());
                json_set(out, "state", state);
            } else if (tc->state == 3) {
                json_t *state = json_object();
                json_t *sp = json_obj_get(pin, "subparsers");
                json_set(state, "subparsers_required",
                         json_bool(json_get_bool(sp, "required", false)));
                json_set(out, "state", state);
            }
            sbuf_free(&sc.spec);
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
