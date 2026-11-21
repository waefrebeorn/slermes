/* t_port_slack_adapter_flow9.c -- harness for the Slack plugin-adapter FLOW
 * port, wave 9 (src/gateway/platforms/slack_adapter_flow9.c).
 *
 * Reads JSON lines from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON lines: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret") and the state echo ("state"). The echoed "in" is the ORIGINAL
 * input (the oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__":true,"ret":<value>,"raise":{...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/slack_adapter_flow9.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*sa9_fn_t)(sa9_ctx_t *, const json_t *);
typedef struct { const char *func; sa9_fn_t fn; } sa9_case_t;

static const sa9_case_t CASES[] = {
    { "_date", sa9_date },
    { "_drop_bot_sender", sa9_drop_bot_sender },
    { "_early_reject_unauthorized", sa9_early_reject_unauthorized },
    { "_ensure_dm_conversation", sa9_ensure_dm_conversation },
    { "_ensure_socket_watchdog", sa9_ensure_socket_watchdog },
    { "_extract_additional_text_from_slack_blocks", sa9_extract_additional_text_from_slack_blocks },
    { "_extract_text_from_slack_attachments", sa9_extract_text_from_slack_attachments },
    { "_fatal_missing_env", sa9_fatal_missing_env },
    { "_fetch_thread_context", sa9_fetch_thread_context },
    { "_fetch_thread_parent_text", sa9_fetch_thread_parent_text },
    { "_finalize_interactive_message", sa9_finalize_interactive_message },
    { "_format_thread_context", sa9_format_thread_context },
    { "_get_thread_watermark", sa9_get_thread_watermark },
    { "_handle_app_context_changed", sa9_handle_app_context_changed },
    { "_handle_app_home_opened", sa9_handle_app_home_opened },
    { "_runner_auth_fn", sa9_runner_auth_fn },
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

        const sa9_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set(out, "error", json_string("unknown func"));
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            sa9_ctx_t ctx;
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
        json_set(echo, "func", json_string(func));
        json_set(echo, "in", in ? json_copy(in) : json_null());
        json_set(echo, "out", out);
        char *s = json_serialize(echo);
        if (s) { printf("%s\n", s); fflush(stdout); free(s); }
        json_free(echo);
        json_free(rec);
    }
    json_free(empty_args);
    return 0;
}
