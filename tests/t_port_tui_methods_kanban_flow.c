/* t_port_tui_methods_kanban_flow.c — harness for the TUI kanban method FLOW
 * port (src/gateway/tui_methods_kanban_flow.c).
 *
 * The task named tui_gateway/methods_kanban.py, which does not exist in the
 * quarry; the real defining modules are gateway/session_kanban.py (the
 * "kanban.run" engine) and gateway/session_controls.py:kanban_run (the RPC
 * method). See the port header for the resolution and the seam contract.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated module-global mirror ("state":
 * _TURNS_DONE / environ / the closure result box). The echoed "in" is the
 * ORIGINAL input (the oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/tui_methods_kanban_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*tmk_fn_t)(tmk_ctx_t *, const json_t *);
typedef struct { const char *func; tmk_fn_t fn; } tmk_case_t;

static const tmk_case_t CASES[] = {
    { "build_kanban_policy", tmk_build_kanban_policy },
    { "run_task", tmk_run_task },
    { "admit_claim", tmk_admit_claim },
    { "bind_worker_context", tmk_bind_worker_context },
    { "_watch_submitter", tmk_watch_submitter },
    { "watch", tmk_watch },
    { "run_worker_turns", tmk_run_worker_turns },
    { "_worker_workspace", tmk_worker_workspace },
    { "_run_worker_turns", tmk_run_worker_turns_inner },
    { "record_start_failure", tmk_record_start_failure },
    { "_record_worker_result", tmk_record_worker_result },
    { "record_worker_loss", tmk_record_worker_loss },
    { "_bound_worker_matches", tmk_bound_worker_matches },
    { "_bound_claim_is_live", tmk_bound_claim_is_live },
    { "worker_result", tmk_worker_result },
    { "worker_exit_code", tmk_worker_exit_code },
    { "_stored_conversation", tmk_stored_conversation },
    { "_run_task_turns", tmk_run_task_turns },
    { "recover", tmk_recover },
    { "status", tmk_status },
    { "block", tmk_block },
    { "turn", tmk_turn },
    { "kanban_run", tmk_kanban_run },
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

        const tmk_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (CASES[i].func && strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            tmk_ctx_t ctx;
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