/* t_port_tui_methods_tools_flow.c — harness for the tui_gateway/methods_tools.py
 * FLOW port (src/gateway/tui_methods_tools_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated module-global mirror ("state":
 * _plugin_activations / _plugin_activation_subscribed). The echoed "in" is the
 * ORIGINAL input (the oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * in.invoke_loaded (listener cases): after a successful on_plugin_loaded
 * registration, the harness drives the registered _on_loaded callback with
 * each summaries list, mutating _plugin_activations exactly as live does.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/tui_methods_tools_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*tmt_fn_t)(tmt_ctx_t *, const json_t *);
typedef struct { const char *func; tmt_fn_t fn; } tmt_case_t;

static const tmt_case_t CASES[] = {
    { "_joined_output", tmt_joined_output },
    { "_is_snapshot_restore", tmt_is_snapshot_restore },
    { "_worker_refused_skill", tmt_worker_refused_skill },
    { "_refusal_data", tmt_refusal_data },
    { "_with_activation", tmt_with_activation },
    { "_exec_out", tmt_exec_out },
    { "_run_action", tmt_run_action },
    { "_cmd_queue", tmt_cmd_queue },
    { "_cmd_snapshot", tmt_cmd_snapshot },
    { "_mcp_reload_confirm_required", tmt_mcp_reload_confirm_required },
    { "_session_key_or_err", tmt_session_key_or_err },
    { "_ensure_plugin_activation_listener", tmt_ensure_plugin_activation_listener },
    { "_plugins_list", tmt_plugins_list },
    { "_plugins_toggle", tmt_plugins_toggle },
    { "_plugins_install", tmt_plugins_install },
    { "_plugins_inspect", tmt_plugins_inspect },
    { "_plugins_install_desktop", tmt_plugins_install_desktop },
    { "_plugins_remove_desktop", tmt_plugins_remove_desktop },
    { "_plugins_update", tmt_plugins_update },
    { "_plugins_remove", tmt_plugins_remove },
    { "_portable_server_names", tmt_portable_server_names },
    { "_plugins_settings", tmt_plugins_settings },
    { "_plugins_update_all", tmt_plugins_update_all },
    { "_plugins_auto_update", tmt_plugins_auto_update },
    { "_plugins_onboarding", tmt_plugins_onboarding },
    { "_plugins_presence", tmt_plugins_presence },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *read_line(FILE *f) {
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    int ch;
    while ((ch = fgetc(f)) != EOF && ch != '\n') {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        buf[len++] = (char)ch;
    }
    if (ch == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

/* Drive the registered _on_loaded callback (listener cases). Returns NULL on
 * success or a raise spec (KeyError on a malformed entry, as live). */
static json_t *drive_invoke_loaded(tmt_ctx_t *ctx, const json_t *in) {
    const json_t *inv = json_obj_get(in, "invoke_loaded");
    if (!inv || inv->type != JSON_ARRAY) return NULL;
    /* registered iff an on_plugin_loaded event fired without a raise */
    bool registered = false;
    for (size_t i = 0; i < json_len(ctx->trace); i++) {
        const json_t *ev = json_get(ctx->trace, i);
        if (!strcmp(json_get_str(ev, "name", ""), "on_plugin_loaded")
            && !tmt_is_raise(json_obj_get(ev, "ret")))
            registered = true;
    }
    if (!registered) return NULL;
    json_t *acts = json_obj_get(ctx->state, "_plugin_activations");
    for (size_t i = 0; i < json_len(inv); i++) {
        const json_t *summaries = json_get(inv, i);
        if (!summaries || summaries->type != JSON_ARRAY) continue;
        for (size_t j = 0; j < json_len(summaries); j++) {
            const json_t *entry = json_get(summaries, j);
            const json_t *name = entry ? json_obj_get(entry, "name") : NULL;
            if (!name) return tmt_err_spec("KeyError", "'name'");
            const json_t *key = json_obj_get(entry, "key");
            if (!key) return tmt_err_spec("KeyError", "'key'");
            const char *ns = tmt_str(name);
            const char *ks = tmt_str(key);
            if (ns) json_set(acts, ns, json_copy(entry));
            if (ks) json_set(acts, ks, json_copy(entry));
        }
    }
    return NULL;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    json_t *empty_args = json_object();
    char *line;
    while ((line = read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in || in->type != JSON_OBJECT) in = NULL;
        json_t *out = json_object();

        const tmt_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (CASES[i].func && strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            tmt_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            if (!tmt_is_raise(ret) && in
                && !strcmp(func, "_ensure_plugin_activation_listener")) {
                json_t *ir = drive_invoke_loaded(&ctx, in);
                if (ir) { json_free(ret); ret = ir; }
            }
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
            json_set(out, "state", ctx.state);
            json_free(ctx.cursors);
        }
        json_t *echo = json_object();
        json_set_hdr(echo, "func", func);
        json_set(echo, "in", in ? json_copy(in) : json_null());
        json_set(echo, "out", out);
        char *s = json_serialize(echo);
        if (s) { printf("%s\n", s); free(s); }
        json_free(echo);
        json_free(rec);
    }
    json_free(empty_args);
    return 0;
}