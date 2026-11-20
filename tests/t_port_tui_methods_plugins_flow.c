/* t_port_tui_methods_plugins_flow.c — harness for the TUI plugin-method FLOW
 * port (src/gateway/tui_methods_plugins_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the module-global mirror ("state" — echoed; no ported
 * function here mutates module globals). The echoed "in" is the ORIGINAL
 * input (the oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 * io["toolsets.data"] scripts the toolsets module DATA attributes (untraced).
 *
 * func names are the live def names; the three register() defs and the
 * anonymous catalog handler are module-qualified to disambiguate:
 *   "_"                                methods_plugin_catalog.py:_
 *   "methods_plugin_catalog.register"
 *   "methods_tools_plugin_commands.register"
 *   "methods_tools_plugin_servers.register"
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/tui_methods_plugins_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*tmp_fn_t)(tmp_ctx_t *, const json_t *);
typedef struct { const char *func; tmp_fn_t fn; } tmp_case_t;

static const tmp_case_t CASES[] = {
    { "_", tmp_catalog_lookup },
    { "methods_plugin_catalog.register", tmp_catalog_register },
    { "_plugin_command_handler", tmp_plugin_command_handler },
    { "_run_plugin_command", tmp_run_plugin_command },
    { "_dispatch_plugin_in_profile_scope", tmp_dispatch_plugin_in_profile_scope },
    { "methods_tools_plugin_commands.register", tmp_plugin_commands_register },
    { "_plugin_server_rows", tmp_plugin_server_rows },
    { "_server_identity", tmp_server_identity },
    { "_portable_server_tools", tmp_portable_server_tools },
    { "_plugin_tool_ownership", tmp_plugin_tool_ownership },
    { "methods_tools_plugin_servers.register", tmp_plugin_servers_register },
    { "_tools_mod", tmp_tools_mod },
    { "_is_registry_command", tmp_is_registry_command },
    { "_dispatch_plugin", tmp_dispatch_plugin },
    { "refresh_plugin_sessions", tmp_refresh_plugin_sessions },
    { "_plugin_rows", tmp_plugin_rows },
    { "_session_home_scope", tmp_session_home_scope },
    { "_refresh_live_sessions", tmp_refresh_live_sessions },
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

        const tmp_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (CASES[i].func && strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            tmp_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
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