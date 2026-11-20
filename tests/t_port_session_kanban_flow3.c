/* t_port_session_kanban_flow3.c — harness for the session_kanban remainder
 * FLOW port (src/gateway/session_kanban_flow3.c).
 *
 * gateway/session_kanban.py is fully claimed by tui_methods_kanban_flow.c;
 * session_kanban_flow2.c ported the turn-recovery engine + worker exit policy.
 * This port covers the NEXT unclaimed collaborator cluster that flow SEAMS:
 * hermes_cli/kanban_db.py (board resolution, task/event reads, JSON column
 * codecs), hermes_cli/kanban_db_dispatch.py (worker liveness / fingerprints /
 * terminal timeout), hermes_cli/process_identity.py (birth reads) and
 * session_keys.py (profile namespace). See the port header for the resolution
 * and the seam contract.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the module-global mirror ("state": os.environ + psutil
 * flags, echoed verbatim — no function here mutates state). The echoed "in"
 * is the ORIGINAL input (the oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/session_kanban_flow3.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*sk3_fn_t)(sk3_ctx_t *, const json_t *);
typedef struct { const char *func; sk3_fn_t fn; } sk3_case_t;

static const sk3_case_t CASES[] = {
    { "_normalize_board_slug", sk3_normalize_board_slug },
    { "_require_slug", sk3_require_slug },
    { "_board_path", sk3_board_path },
    { "kanban_db_path", sk3_kanban_db_path },
    { "workspaces_root", sk3_workspaces_root },
    { "get_task", sk3_get_task },
    { "_json_or", sk3_json_or },
    { "_json_or_null", sk3_json_or_null },
    { "_append_event", sk3_append_event },
    { "_current_run_id", sk3_current_run_id },
    { "_latest_event", sk3_latest_event },
    { "_json_dict", sk3_json_dict },
    { "_row_get", sk3_row_get },
    { "_retry_status_for_run", sk3_retry_status_for_run },
    { "goal_run_status", sk3_goal_run_status },
    { "_worker_terminal_timeout_env", sk3_worker_terminal_timeout_env },
    { "_process_fingerprint", sk3_process_fingerprint },
    { "_pid_recycled", sk3_pid_recycled },
    { "_worker_alive", sk3_worker_alive },
    { "process_birth", sk3_process_birth },
    { "worker_birth", sk3_worker_birth },
    { "profile_from_session_key", sk3_profile_from_session_key },
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

        const sk3_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (CASES[i].func && strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            sk3_ctx_t ctx;
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
