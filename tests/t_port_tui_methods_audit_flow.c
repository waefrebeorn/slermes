/* t_port_tui_methods_audit_flow.c -- harness for the
 * tui_gateway/methods_session_control.py FLOW port
 * (src/gateway/tui_methods_audit_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated state mirror ("state": the session dict).
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
#include "gateway/tui_methods_audit_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*tma_fn_t)(tma_ctx_t *, const json_t *);
typedef struct { const char *func; tma_fn_t fn; } tma_case_t;

static const tma_case_t CASES[] = {
    { "_safe_goal_snapshot", tma_safe_goal_snapshot },
    { "_extract_wait_barrier", tma_extract_wait_barrier },
    { "_safe_loop_snapshot", tma_safe_loop_snapshot },
    { "_safe_heartbeat_snapshot", tma_safe_heartbeat_snapshot },
    { "_snapshot_control", tma_snapshot_control },
    { "_snapshot_revision", tma_snapshot_revision },
    { "_snapshot_updated_at", tma_snapshot_updated_at },
    { "_load_goal_state", tma_load_goal_state },
    { "_load_loop_state", tma_load_loop_state },
    { "_load_heartbeat_state", tma_load_heartbeat_state },
    { "_goal_blocks_loop_tick", tma_goal_blocks_loop_tick },
    { "_session_publish_lock", tma_session_publish_lock },
    { "_publish_session_control_snapshot", tma_publish_session_control_snapshot },
    { "_h_control_read", tma_control_read },
    { "_h_control", tma_control },
    { "_validate_action_args", tma_validate_action_args },
    { "_dispatch_command", tma_dispatch_command },
    { "_execute_manager_action", tma_execute_manager_action },
    { "_execute_subgoal_action", tma_execute_subgoal_action },
    { "_execute_heartbeat_action", tma_execute_heartbeat_action },
    { "_manager_error_message", tma_manager_error_message },
    { "_dispatch_envelope", tma_dispatch_envelope },
    { "register", tma_register },
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

        const tma_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (CASES[i].func && strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            tma_ctx_t ctx;
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
