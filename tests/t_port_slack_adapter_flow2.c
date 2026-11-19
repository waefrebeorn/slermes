/* t_port_slack_adapter_flow2.c — harness for the Slack plugin-adapter FLOW
 * port, wave 2 (src/gateway/platforms/slack_adapter_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated state ("state"). The echoed "in" is the
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
#include "gateway/platforms/slack_adapter_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*sa2_fn_t)(sa2_ctx_t *, const json_t *);
typedef struct { const char *func; sa2_fn_t fn; } sa2_case_t;

static const sa2_case_t CASES[] = {
    { "_slack_error_is", sa2_slack_error_is },
    { "_is_transient_transport_error", sa2_is_transient_transport_error },
    { "_collect_slack_table_cell_text", sa2_collect_slack_table_cell_text },
    { "_render_slack_table_block", sa2_render_slack_table_block },
    { "_extract_text_from_slack_blocks", sa2_extract_text_from_slack_blocks },
    { "_extract_urls_from_slack_blocks", sa2_extract_urls_from_slack_blocks },
    { "_apply_slack_proxy", sa2_apply_slack_proxy },
    { "_slack_timestamp_sort_key", sa2_slack_timestamp_sort_key },
    { "_trim_oldest_dict_entries", sa2_trim_oldest_dict_entries },
    { "_discard_oldest_by_thread_ts", sa2_discard_oldest_by_thread_ts },
    { "_metadata_team_id", sa2_metadata_team_id },
    { "_workspace_event_id", sa2_workspace_event_id },
    { "_workspace_message_marker", sa2_workspace_message_marker },
    { "_truthy_config", sa2_truthy_config },
    { "_task_update_chunk", sa2_task_update_chunk },
    { "_stream_relation", sa2_stream_relation },
    { "_lazy_attr", sa2_lazy_attr },
    { "_remember_channel_team", sa2_remember_channel_team },
    { "_pop_slash_context", sa2_pop_slash_context },
    { "_purge_stale_slash_contexts", sa2_purge_stale_slash_contexts },
    { "scope_id_for_chat", sa2_scope_id_for_chat },
    { "_get_client", sa2_get_client },
    { "_is_ignored_channel", sa2_is_ignored_channel },
    { "native_task_cards_enabled", sa2_native_task_cards_enabled },
    { "_describe_slack_api_error", sa2_describe_slack_api_error },
    { "_warn_if_not_bot_token", sa2_warn_if_not_bot_token },
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

        const sa2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set(out, "error", json_string("unknown func"));
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            sa2_ctx_t ctx;
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
        if (s) { printf("%s\n", s); free(s); }
        json_free(echo);
        json_free(rec);
    }
    json_free(empty_args);
    return 0;
}
