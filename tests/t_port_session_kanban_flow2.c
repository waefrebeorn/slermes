/* t_port_session_kanban_flow2.c — harness for the session_kanban remainder
 * FLOW port (src/gateway/session_kanban_flow2.c).
 *
 * gateway/session_kanban.py is fully claimed by tui_methods_kanban_flow.c;
 * this port covers the unclaimed collaborator cluster that flow SEAMS:
 * agent/kanban_turn_recovery.py (the in-place turn-recovery engine) and
 * hermes_cli/turn_exit.py (the worker exit policy). See the port header for
 * the resolution and the seam contract.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the module-global mirror ("state": os.environ, echoed
 * verbatim — no function here mutates state). The echoed "in" is the ORIGINAL
 * input (the oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/session_kanban_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*sk2_fn_t)(sk2_ctx_t *, const json_t *);
typedef struct { const char *func; sk2_fn_t fn; } sk2_case_t;

static const sk2_case_t CASES[] = {
    { "kanban_task_id", sk2_kanban_task_id },
    { "_resolve_task_id", sk2_resolve_task_id },
    { "kanban_turn_recovery_enabled", sk2_kanban_turn_recovery_enabled },
    { "max_recovery_attempts", sk2_max_recovery_attempts },
    { "recovery_delay_seconds", sk2_recovery_delay_seconds },
    { "_turn_exit_is_terminal", sk2_turn_exit_is_terminal },
    { "should_recover_turn", sk2_should_recover_turn },
    { "_kanban_db_path", sk2_kanban_db_path },
    { "_now", sk2_now },
    { "worker_claim_is_live", sk2_worker_claim_is_live },
    { "_truncate", sk2_truncate },
    { "build_recovery_nudge", sk2_build_recovery_nudge },
    { "_emit_recovery_skipped", sk2_emit_recovery_skipped },
    { "recover_failed_kanban_turns", sk2_recover_failed_kanban_turns },
    { "turn_exit_code", sk2_turn_exit_code },
    { "credential_failure_flags", sk2_credential_failure_flags },
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

        const sk2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (CASES[i].func && strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            sk2_ctx_t ctx;
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