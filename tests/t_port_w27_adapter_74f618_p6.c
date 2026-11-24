/* JSON-lines harness for the w27_adapter_74f618_p6 gate. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_74f618_p6.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[1048576];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "task_update_chunk")) out = w27_p6_task_update_chunk(in);
        else if (!strcmp(func, "strip_stream_cursor")) out = w27_p6_strip_stream_cursor(in);
        else if (!strcmp(func, "stream_key")) out = w27_p6_stream_key(in);
        else if (!strcmp(func, "mrkdwn_insensitive")) out = w27_p6_mrkdwn_insensitive(in);
        else if (!strcmp(func, "stream_relation")) out = w27_p6_stream_relation(in);
        else if (!strcmp(func, "default_status_text")) out = w27_p6_default_status_text(in);
        else if (!strcmp(func, "cron_continuable_surface")) out = w27_p6_cron_continuable_surface(in);
        else if (!strcmp(func, "slack_allow_bots")) out = w27_p6_slack_allow_bots(in);
        else if (!strcmp(func, "api_human_users")) out = w27_p6_api_human_users(in);
        else if (!strcmp(func, "event_declares_bot_sender")) out = w27_p6_event_declares_bot_sender(in);
        else if (!strcmp(func, "format_tool_preview")) out = w27_p6_format_tool_preview(in);
        else if (!strcmp(func, "markdown_block_payload")) out = w27_p6_markdown_block_payload(in);
        else if (!strcmp(func, "append_feedback_block")) out = w27_p6_append_feedback_block(in);
        else out = json_string("UNKNOWN_FUNC");
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
