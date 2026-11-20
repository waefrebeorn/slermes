/* t_port_kanban_db_dispatch_flow2.c — harness for the kanban_db_dispatch
 * dead-worker / crash-accounting remainder FLOW port
 * (src/cli/kanban_db_dispatch_flow2.c).
 *
 * hermes_cli/kanban_db_dispatch.py is partly claimed by port_kanban_db_dispatch.c,
 * kanban_workers.c, kanban_db*.c and session_kanban_flow3.c. This port covers
 * the NEXT unclaimed cluster: describe_suppression, the worker-log exit/output
 * readers, the _DeadWorker classification + run_outcome, the reclaim sweep, the
 * crash breaker, _is_handoff_event and adopt_worker_pid. See the port header for
 * the seam contract.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the module-global mirror ("state", echoed verbatim — no
 * function here mutates state). The echoed "in" is the ORIGINAL input (the
 * oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "cli/kanban_db_dispatch_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*kdd2_fn_t)(kdd2_ctx_t *, const json_t *);
typedef struct { const char *func; kdd2_fn_t fn; } kdd2_case_t;

static const kdd2_case_t CASES[] = {
    { "describe_suppression", kdd2_describe_suppression },
    { "_worker_log_exit_code", kdd2_worker_log_exit_code },
    { "_exit_summary_marker", kdd2_exit_summary_marker },
    { "_log_noise_prefixes", kdd2_log_noise_prefixes },
    { "_worker_final_output", kdd2_worker_final_output },
    { "run_outcome", kdd2_run_outcome },
    { "_classify_dead_worker", kdd2_classify_dead_worker },
    { "_classify_dead_worker_exit", kdd2_classify_dead_worker_exit },
    { "_reclaim_dead_workers", kdd2_reclaim_dead_workers },
    { "_account_crashes", kdd2_account_crashes },
    { "_is_handoff_event", kdd2_is_handoff_event },
    { "adopt_worker_pid", kdd2_adopt_worker_pid },
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

        const kdd2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (CASES[i].func && strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            kdd2_ctx_t ctx;
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
