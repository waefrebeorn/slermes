/* t_port_tui_methods_goals_flow.c -- harness for the TUI /goal command methods
 * FLOW port (src/gateway/tui_methods_goals_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated state mirror ("state": the mgr carrier).
 * The echoed "in" is the ORIGINAL input (the oracle re-drives the live
 * reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/tui_methods_goals_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*tmg_fn_t)(tmg_ctx_t *, const json_t *);
typedef struct { const char *func; tmg_fn_t fn; } tmg_case_t;

static const tmg_case_t CASES[] = {
    { "_english", tmg_english },
    { "_status", tmg_status_case },
    { "_show", tmg_show_case },
    { "_pause", tmg_pause_case },
    { "_resume", tmg_resume_case },
    { "_clear", tmg_clear_case },
    { "_unwait", tmg_unwait_case },
    { "_wait", tmg_wait_case },
    { "_gate_add", tmg_gate_add_case },
    { "_gate_remove", tmg_gate_remove_case },
    { "_gate_clear", tmg_gate_clear_case },
    { "_bare_control", tmg_bare_control_case },
    { "_gate", tmg_gate_case },
    { "_set", tmg_set_case },
    { "is_goal_control", tmg_is_goal_control },
    { "dispatch_goal_command", tmg_dispatch_case },
    { "clear", tmg_mgr_clear_case },
    { "add_gate", tmg_mgr_add_gate_case },
    { "remove_gate", tmg_mgr_remove_gate_case },
    { "clear_gates", tmg_mgr_clear_gates_case },
    { "render_gates", tmg_mgr_render_gates_case },
    { "render_contract", tmg_mgr_render_contract_case },
    { "wait_on_session", tmg_mgr_wait_on_session_case },
    { "_check_gates", tmg_mgr_check_gates_case },
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

        const tmg_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (CASES[i].func && strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            tmg_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            ctx.last_raise = false;
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