/* t_port_tui_methods_misc_flow.c — harness for the TUI slash-exec misc
 * method FLOW port (src/gateway/tui_methods_misc_flow.c).
 *
 * The task named tui_gateway/methods_misc.py, which does not exist in the
 * quarry; the real defining module is tui_gateway/methods_slash.py (the
 * slash.exec live-output + side-effect-mirroring helpers, fully unclaimed).
 * See the port header for the resolution and the seam contract.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated state mirror ("state": agent.service_tier).
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
#include "gateway/tui_methods_misc_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*tmm_fn_t)(tmm_ctx_t *, const json_t *);
typedef struct { const char *func; tmm_fn_t fn; } tmm_case_t;

static const tmm_case_t CASES[] = {
    { "_format_live_review_output", tmm_format_live_review_output },
    { "_format_live_refine_output", tmm_format_live_refine_output },
    { "_usage_route", tmm_usage_route },
    { "_usage_on_nous_route", tmm_usage_on_nous_route },
    { "_usage_nous_fields", tmm_usage_nous_fields },
    { "_usage_account_blocks", tmm_usage_account_blocks },
    { "_format_live_usage_output", tmm_format_live_usage_output },
    { "_format_live_usage_tokens", tmm_format_live_usage_tokens },
    { "_live_session_messages", tmm_live_session_messages },
    { "_format_live_history_output", tmm_format_live_history_output },
    { "_format_live_context_output", tmm_format_live_context_output },
    { "_format_live_tools_output", tmm_format_live_tools_output },
    { "_format_live_help_output", tmm_format_live_help_output },
    { "_format_live_model_output", tmm_format_live_model_output },
    { "_format_live_status_output", tmm_format_live_status_output },
    { "_live_slash_command_output", tmm_live_slash_command_output },
    { "_compress_live_with_feedback", tmm_compress_live_with_feedback },
    { "_mirror_approvals", tmm_mirror_approvals },
    { "_mirror_personality", tmm_mirror_personality },
    { "_mirror_fast", tmm_mirror_fast },
    { "_mirror_reload_mcp", tmm_mirror_reload_mcp },
    { "_mirror_stop", tmm_mirror_stop },
    { "_mirror_autoprune", tmm_mirror_autoprune },
    { "_compute_host_slash", tmm_compute_host_slash },
    { "_mirror_slash_side_effects", tmm_mirror_slash_side_effects },
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

        const tmm_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (CASES[i].func && strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            tmm_ctx_t ctx;
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