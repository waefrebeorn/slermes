/* JSON-lines harness for the w30_messages_29cf8f_p3 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w30_messages_29cf8f_p3.h"

typedef json_t *(*msg_fn)(const json_t *);

static const struct {
    const char *name;
    msg_fn fn;
} TABLE[] = {
    {"sql_latest_dialogue", w30_messages_29cf8f_p3_sql_latest_dialogue},
    {"sql_message_storage_state", w30_messages_29cf8f_p3_sql_message_storage_state},
    {"sql_terminal_payloads", w30_messages_29cf8f_p3_sql_terminal_payloads},
    {"sql_blocked_payloads", w30_messages_29cf8f_p3_sql_blocked_payloads},
    {"sql_has_any_message", w30_messages_29cf8f_p3_sql_has_any_message},
    {"sql_spillover_reference", w30_messages_29cf8f_p3_sql_spillover_reference},
    {"sql_filter", w30_messages_29cf8f_p3_sql_filter},
    {"timeline_page", w30_messages_29cf8f_p3_timeline_page},
    {"prompt_page", w30_messages_29cf8f_p3_prompt_page},
    {"anchored_window", w30_messages_29cf8f_p3_anchored_window},
    {"anchored_view", w30_messages_29cf8f_p3_anchored_view},
    {"pr_url_hits", w30_messages_29cf8f_p3_pr_url_hits},
    {"handoff_map", w30_messages_29cf8f_p3_handoff_map},
    {"watermark", w30_messages_29cf8f_p3_watermark},
};

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        for (size_t i = 0; i < sizeof(TABLE) / sizeof(TABLE[0]); i++) {
            if (!strcmp(func, TABLE[i].name)) { out = TABLE[i].fn(in); break; }
        }
        if (!out) out = json_string("UNKNOWN_FUNC");
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
