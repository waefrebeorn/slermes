/* t_port_tui_methods_cron_flow.c -- harness for the TUI cron-method FLOW port
 * (src/gateway/tui_methods_cron_flow.c). The named quarry module
 * tui_gateway/methods_cron.py does not exist; the real defining module of the
 * TUI cron method (cron.manage) is tui_gateway/methods_tools.py.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the module-global mirror ("state"; these flows keep it
 * empty). The echoed "in" is the ORIGINAL input (the oracle re-drives the live
 * reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either a
 * plain JSON value (returned verbatim on every call), a LIST consumed in call
 * order, or {"__script__": true, "ret": <value>, "raise": {type,...}}. A
 * missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The two decorated `def _` handlers dispatch under synthetic keys
 * (_h_cron_manage / _h_learning_frames); every other func uses its live name.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/tui_methods_cron_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*tmc2_fn_t)(tmc2_ctx_t *, const json_t *);
typedef struct { const char *func; tmc2_fn_t fn; } tmc2_case_t;

static const tmc2_case_t CASES[] = {
    { "_profile_scoped_rpc", tmc2_profile_scoped_rpc },
    { "_guarded", tmc2_guarded },
    { "_rpc", tmc2_rpc },
    { "_scoped_rpc", tmc2_scoped_rpc },
    { "_mcp_rpc", tmc2_mcp_rpc },
    { "_h_cron_manage", tmc2_cron_manage },
    { "_mcp_server_rows", tmc2_mcp_server_rows },
    { "_mcp_named_server", tmc2_mcp_named_server },
    { "_mcp_config_server_or_error", tmc2_mcp_config_server_or_error },
    { "_user_turn_indices", tmc2_user_turn_indices },
    { "_rewind_prelude", tmc2_rewind_prelude },
    { "_rewind_or_err", tmc2_rewind_or_err },
    { "_learning_mutation", tmc2_learning_mutation },
    { "_h_learning_frames", tmc2_learning_frames },
    { "_skills_search", tmc2_skills_search },
    { "_skills_browse", tmc2_skills_browse },
    { "_capture_run_kwargs", tmc2_capture_run_kwargs },
    { "_captured_exec", tmc2_captured_exec },
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

        const tmc2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (CASES[i].func && strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            tmc2_ctx_t ctx;
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
