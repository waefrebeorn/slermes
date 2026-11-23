/*
 * venv_sync.c — C port of upstream Hermes hermes_cli/venv_sync.py.
 *
 * PoP: w7_venvsync_d6306e_is_read_only_gateway_status @ hermes_cli/venv_sync.py:_is_read_only_gateway_status
 * PoP: w7_venvsync_d6306e_supervised_child @ hermes_cli/venv_sync.py:_supervised_child
 * PoP: w7_venvsync_d6306e_completion_retry_constants @ hermes_cli/venv_sync.py:COMPLETION_RETRY_*
 * PoP: w7_venvsync_d6306e_string_set @ hermes_cli/venv_sync.py:_METADATA_FLAGS / _REPORT_ONLY_UPDATE_FLAGS / _TAIL_SCRIPTS / _SYNC_INPUTS
 * PoP: w7_venvsync_d6306e_relaunch_command @ hermes_cli/venv_sync.py:relaunch_command
 *
 * Filesystem/process side effects are out of scope; env values are injected
 * through explicit seams instead of mutating the CRT environment.
 */
#include "noc_w7_venvsync_d6306e.h"
#include "hermes_json.h"
#include <ctype.h>
#include <string.h>

#define COMPLETION_RETRY_BACKOFF_ATTEMPTS 2
#define COMPLETION_RETRY_BACKOFF_SECONDS (15 * 60)
#define COMPLETION_RETRY_MAX_ATTEMPTS 6

int w7_venvsync_d6306e_is_read_only_gateway_status(const json_t *command) {
    if (!json_is_array(command) || json_len(command) < 2) return 0;
    const json_t *a = json_get(command, 0), *b = json_get(command, 1);
    return (json_is_string(a) && !strcmp(a->str_val, "gateway") &&
            json_is_string(b) && !strcmp(b->str_val, "status")) ? 1 : 0;
}

/* Python: v.strip().lower() in ("1", "true", "yes", "on") */
static int truthy_marker(const char *v) {
    if (!v) return 0;
    while (*v && isspace((unsigned char)*v)) v++;
    const char *e = v + strlen(v);
    while (e > v && isspace((unsigned char)e[-1])) e--;
    char buf[64];
    size_t n = (size_t)(e - v);
    if (n >= sizeof(buf)) return 0;
    for (size_t i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)v[i]);
    buf[n] = '\0';
    return (!strcmp(buf, "1") || !strcmp(buf, "true") ||
            !strcmp(buf, "yes") || !strcmp(buf, "on")) ? 1 : 0;
}

int w7_venvsync_d6306e_supervised_child(const char *supervised, const char *s6_supervised) {
    return truthy_marker(supervised) || truthy_marker(s6_supervised);
}

struct json_t *w7_venvsync_d6306e_completion_retry_constants(void) {
    json_t *o = json_object();
    json_set(o, "backoff_attempts", json_number(COMPLETION_RETRY_BACKOFF_ATTEMPTS));
    json_set(o, "backoff_seconds", json_number(COMPLETION_RETRY_BACKOFF_SECONDS));
    json_set(o, "max_attempts", json_number(COMPLETION_RETRY_MAX_ATTEMPTS));
    return o;
}

struct json_t *w7_venvsync_d6306e_string_set(const char *which) {
    static const char *const metadata[] = {"--help", "--version", "-V", "-h"};
    static const char *const report_only[] = {"--check", "--list-venv-holders", "--plan"};
    static const char *const tail_scripts[] = {"source_completion.py", "update_completion.py"};
    static const char *const sync_inputs[] = {"pyproject.toml", "setup.py", "uv.lock", "pm/lock.json"};
    const char *const *items = NULL;
    size_t n = 0;
    if (which && !strcmp(which, "metadata_flags")) { items = metadata; n = 4; }
    else if (which && !strcmp(which, "report_only_update_flags")) { items = report_only; n = 3; }
    else if (which && !strcmp(which, "tail_scripts")) { items = tail_scripts; n = 2; }
    else if (which && !strcmp(which, "sync_inputs")) { items = sync_inputs; n = 4; }
    json_t *arr = json_array();
    for (size_t i = 0; i < n; i++) json_append(arr, json_string(items[i]));
    return arr;
}

/* ---- Python repr() of the simple string/list values relaunch_command builds ---- */
typedef struct { char b[16384]; size_t n; } SB;
static void sb_ch(SB *s, char c) {
    if (s->n + 1 < sizeof(s->b)) { s->b[s->n++] = c; s->b[s->n] = '\0'; }
}
static void sb(SB *s, const char *t) {
    for (; *t; t++) sb_ch(s, *t);
}
static void repr_str(SB *s, const char *v) {
    if (!v) v = "";
    const char *q = (!strchr(v, '\'') || strchr(v, '"')) ? "'" : "\"";
    sb_ch(s, *q);
    for (const char *p = v; *p; p++) {
        char c = *p;
        if (c == *q || c == '\\') { sb_ch(s, '\\'); sb_ch(s, c); }
        else if (c == '\n') { sb_ch(s, '\\'); sb_ch(s, 'n'); }
        else if (c == '\r') { sb_ch(s, '\\'); sb_ch(s, 'r'); }
        else if (c == '\t') { sb_ch(s, '\\'); sb_ch(s, 't'); }
        else sb_ch(s, c);
    }
    sb_ch(s, *q);
}
static void repr_list(SB *s, const json_t *arr) {
    sb_ch(s, '[');
    size_t n = json_is_array(arr) ? json_len(arr) : 0;
    for (size_t i = 0; i < n; i++) {
        if (i) { sb_ch(s, ','); sb_ch(s, ' '); }
        const json_t *e = json_get(arr, i);
        if (json_is_string(e)) repr_str(s, e->str_val);
        else if (!e || e->type == JSON_NULL) sb(s, "None");
        else sb(s, "None");
    }
    sb_ch(s, ']');
}

struct json_t *w7_venvsync_d6306e_relaunch_command(const char *python, const char *root,
                                                  const json_t *argv,
                                                  const json_t *original,
                                                  const char *module) {
    json_t *options = json_array();
    size_t index = 1;
    size_t olen = json_is_array(original) ? json_len(original) : 0;
    while (index < olen) {
        const json_t *en = json_get(original, index);
        const char *o = json_is_string(en) ? en->str_val : "";
        if (!strcmp(o, "-c") || !strcmp(o, "-m") || !strcmp(o, "--") ||
            !strcmp(o, "-") || o[0] != '-')
            break;
        json_append(options, json_copy(en));
        index++;
        if ((!strcmp(o, "-W") || !strcmp(o, "-X")) && index < olen) {
            json_append(options, json_copy(json_get(original, index)));
            index++;
        }
    }
    const json_t *a0 = json_len(argv) ? json_get(argv, 0) : NULL;
    const char *argv0 = json_is_string(a0) ? a0->str_val : "";
    const char *mod = (module && module[0]) ? module : NULL;

    if (!strcmp(argv0, "-m") && index < olen) {
        const json_t *t = json_get(original, index);
        if (json_is_string(t) && !strcmp(t->str_val, "-m") && index + 1 < olen) {
            const json_t *m = json_get(original, index + 1);
            if (json_is_string(m)) mod = m->str_val;
        }
    }

    SB prefix = {{0}, 0}, body = {{0}, 0}, full = {{0}, 0};
    sb(&prefix, "import sys, runpy; sys.path.insert(0, ");
    repr_str(&prefix, root);
    sb(&prefix, "); sys.argv = ");
    repr_list(&prefix, argv);
    sb(&prefix, "; sys._hermes_relaunched = True; ");

    if (!strcmp(argv0, "-c") && index + 1 < olen) {
        const json_t *cn = json_get(original, index + 1);
        sb(&body, "exec(");
        repr_str(&body, json_is_string(cn) ? cn->str_val : "");
        sb(&body, ")");
    } else if (mod && strcmp(mod, "__main__")) {
        sb(&body, "runpy.run_module(");
        repr_str(&body, mod);
        sb(&body, ", run_name='__main__', alter_sys=True)");
    } else {
        sb(&body, "runpy.run_path(");
        repr_str(&body, argv0);
        sb(&body, ", run_name='__main__')");
    }
    sb(&full, prefix.b);
    sb(&full, body.b);

    json_t *out = json_array();
    json_append(out, json_string(python ? python : ""));
    for (size_t i = 0; i < json_len(options); i++)
        json_append(out, json_copy(json_get(options, i)));
    json_append(out, json_string("-I"));
    json_append(out, json_string("-c"));
    json_append(out, json_string(full.b));
    json_free(options);
    return out;
}
