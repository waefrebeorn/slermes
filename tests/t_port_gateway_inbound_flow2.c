/* t_port_gateway_inbound_flow2.c — harness for the UNCLAIMED command-dispatch
 * / admission FLOW remainder of gateway/run_inbound.py
 * (src/cli/gateway_inbound_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), the mutated world state ("state") and the possibly
 * text-mutated INPUT event ("event"). The echoed "in" is the ORIGINAL input
 * (the oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly (self-contained, libjson only).
 */
#include "cli/gateway_inbound_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*gi2_fn_t)(gi2_ctx_t *, json_t *);
typedef struct { const char *func; gi2_fn_t fn; } gi2_case_t;

static const gi2_case_t CASES[] = {
    { "_hm_resolve_command", gi2_hm_resolve_command },
    { "_hm_confirm_destructive", gi2_hm_confirm_destructive },
    { "_hm_cmd_new", gi2_hm_cmd_new },
    { "_hm_cmd_start", gi2_hm_cmd_start },
    { "_hm_cmd_egress", gi2_hm_cmd_egress },
    { "_hm_rewrite_turn_to_prompt", gi2_hm_rewrite_turn_to_prompt },
    { "_hm_cmd_learn", gi2_hm_cmd_learn },
    { "_hm_cmd_plan", gi2_hm_cmd_plan },
    { "_hm_cmd_init", gi2_hm_cmd_init },
    { "_hm_cmd_blueprint", gi2_hm_cmd_blueprint },
    { "_hm_cmd_undo", gi2_hm_cmd_undo },
    { "_hm_cmd_queue", gi2_hm_cmd_queue },
    { "_hm_cmd_steer", gi2_hm_cmd_steer },
    { "_hm_dispatch_canonical_command", gi2_hm_dispatch_canonical_command },
    { "_hm_skill_slash_rewrite", gi2_hm_skill_slash_rewrite },
    { "_hm_pending_reply_intercepts", gi2_hm_pending_reply_intercepts },
    { "_hm_dispatch_idle_commands", gi2_hm_dispatch_idle_commands },
    { "_hm_rescue_orphaned_fifo", gi2_hm_rescue_orphaned_fifo },
    { "_hm_authority_admission", gi2_hm_authority_admission },
    { "_hm_admit_to_fifo", gi2_hm_admit_to_fifo },
    { "_handle_message", gi2_handle_message },
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

        const gi2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (CASES[i].func && strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            /* working copy: the port mutates args.event.text in place; the
             * echoed "in" stays the pristine fixture. */
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            json_t *work = (args && args->type == JSON_OBJECT) ? json_copy(args) : json_object();
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            gi2_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, work);
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
            json_set(out, "state", ctx.state);
            json_t *ev_out = json_obj_get(work, "event");
            json_set(out, "event", ev_out ? json_copy(ev_out) : json_null());
            json_free(ctx.cursors);
            json_free(work);
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
    return 0;
}