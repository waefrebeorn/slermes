/* t_port_session_authority_flow.c — harness for the session-authority FLOW
 * port (src/gateway/session_authority_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated self/world state ("state"). The echoed
 * "in" is the ORIGINAL input (the oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly (self-contained, libjson only).
 */
#include "gateway/session_authority_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*sau_fn_t)(sau_ctx_t *, const json_t *);
typedef struct { const char *func; sau_fn_t fn; } sau_case_t;

static const sau_case_t CASES[] = {
    { "route_epoch", sau_route_epoch },
    { "fence_route", sau_fence_route },
    { "_log_drain_failure", sau_log_drain_failure },
    { "_run_deferred_reset", sau_run_deferred_reset },
    { "backfill_legacy_session_owners", sau_backfill_legacy },
    { "backfill_compression_created_sources", sau_backfill_compression },
    { "check_approval_generation", sau_check_approval_generation },
    { "_drain_stopping", sau_drain_stopping },
    { "_pending_rows", sau_pending_rows },
    { "_retire_overflowed", sau_retire_overflowed },
    { "unless_deleted", sau_unless_deleted },
    { "resolve", sau_resolve },
    { "_pending_receipt", sau_pending_receipt },
    { "_publish_pending", sau_publish_pending },
    { "_schedule", sau_schedule },
    { "wake_after_worker", sau_wake_after_worker },
    { "_pause", sau_pause },
    { "_admitted", sau_admitted },
    { "_schedule_admitted", sau_schedule_admitted },
    { "_release_refused_capture", sau_release_refused_capture },
    { "_commit_cancel", sau_commit_cancel },
    { "_settle_cancelled", sau_settle_cancelled },
    { "_classic_turn_holds", sau_classic_turn_holds },
    { "_runs_own_agent", sau_runs_own_agent },
    { "steer_agent", sau_steer_agent },
    { "register_approval", sau_register_approval },
    { "register_clarify", sau_register_clarify },
    { "respond", sau_respond },
    { "_stamp_claim", sau_stamp_claim },
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

        const sau_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            sau_ctx_t ctx;
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