/* t_port_slack_adapter_flow4.c -- harness for the Slack plugin-adapter FLOW
 * port, wave 4 (src/gateway/platforms/slack_adapter_flow4.c).
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
#include "gateway/platforms/slack_adapter_flow4.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*sa4_fn_t)(sa4_ctx_t *, const json_t *);
typedef struct { const char *func; sa4_fn_t fn; } sa4_case_t;

static const sa4_case_t CASES[] = {
    { "_sdk_supports_agent_sessions",  sa4_sdk_supports_agent_sessions },
    { "_interaction_fields",           sa4_interaction_fields },
    { "_section_text",                 sa4_section_text },
    { "_parse_users_info",             sa4_parse_users_info },
    { "_slack_file_kind",              sa4_slack_file_kind },
    { "_cron_continuable_surface",     sa4_cron_continuable_surface },
    { "_slack_allow_bots",             sa4_slack_allow_bots },
    { "_reactions_enabled",            sa4_reactions_enabled },
    { "_is_retryable_upload_error",    sa4_is_retryable_upload_error },
    /* Coherent block-building / event-field-extraction remainder (16). */
    { "_button",                        sa4_button },
    { "_markdown_block_payload",        sa4_markdown_block_payload },
    { "_feedback_block",                sa4_feedback_block },
    { "_append_feedback_block",         sa4_append_feedback_block },
    { "_build_identity_prompt",         sa4_build_identity_prompt },
    { "_event_team_id",                 sa4_event_team_id },
    { "_context_channel_id",            sa4_context_channel_id },
    { "_first_file_share",              sa4_first_file_share },
    { "_is_foreign_author",             sa4_is_foreign_author },
    { "_agent_view_event_fields",       sa4_agent_view_event_fields },
    { "_extract_assistant_thread_metadata", sa4_extract_assistant_thread_metadata },
    { "_event_declares_bot_sender",     sa4_event_declares_bot_sender },
    { "_resolve_thread_ts",             sa4_resolve_thread_ts },
    { "_session_thread_ts",             sa4_session_thread_ts },
    { "_normalize_changed_message",     sa4_normalize_changed_message },
    { "_assistant_thread_title_enabled", sa4_assistant_thread_title_enabled },
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

        const sa4_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set(out, "error", json_string("unknown func"));
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            sa4_ctx_t ctx;
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
