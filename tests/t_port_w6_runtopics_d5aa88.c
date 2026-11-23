/* t_port_w6_runtopics_d5aa88.c — JSON-lines harness for gateway/run_topics.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_runtopics_d5aa88.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static int jbool(json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_BOOL && v->bool_val) ? 1 : 0;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        const char *title = json_get_str(in, "title", "");
        const char *profile = json_get_str(in, "profile", "");
        const char *chat_id = json_get_str(in, "chat_id", "");
        const char *thread_id = json_get_str(in, "thread_id", "");
        const char *platform = json_get_str(in, "platform", "");
        const char *chat_type = json_get_str(in, "chat_type", "");
        const char *initial = json_get_str(in, "auto_thread_initial_name", "");
        json_t *out = NULL;

        if (!strcmp(func, "collapse_title")) {
            char *r = w6_runtopics_d5aa88_collapse_title(title);
            out = json_string(r ? r : ""); free(r);
        } else if (!strcmp(func, "sanitize_telegram_topic_title")) {
            char *r = w6_runtopics_d5aa88_sanitize_telegram_topic_title(title);
            out = json_string(r ? r : ""); free(r);
        } else if (!strcmp(func, "sanitize_discord_thread_title")) {
            char *r = w6_runtopics_d5aa88_sanitize_discord_thread_title(title);
            out = json_string(r ? r : ""); free(r);
        } else if (!strcmp(func, "telegram_topic_profile_name")) {
            char *r = w6_runtopics_d5aa88_telegram_topic_profile_name(profile);
            out = json_string(r ? r : ""); free(r);
        } else if (!strcmp(func, "telegram_topic_key")) {
            out = w6_runtopics_d5aa88_telegram_topic_key(chat_id, thread_id);
        } else if (!strcmp(func, "telegram_topic_cooldown_key")) {
            char *r = w6_runtopics_d5aa88_telegram_topic_cooldown_key(profile, chat_id);
            out = r ? json_string(r) : json_null(); free(r);
        } else if (!strcmp(func, "is_telegram_dm")) {
            out = json_bool(w6_runtopics_d5aa88_is_telegram_dm(platform, chat_type));
        } else if (!strcmp(func, "is_discord_auto_thread_lane")) {
            out = json_bool(w6_runtopics_d5aa88_is_discord_auto_thread_lane(platform, chat_type,
                      jbool(in, "auto_thread_created"), thread_id, initial));
        } else if (!strcmp(func, "is_relay_discord_channel_lane")) {
            out = json_bool(w6_runtopics_d5aa88_is_relay_discord_channel_lane(platform, chat_id,
                      thread_id, chat_type, jbool(in, "delivered_via_upstream_relay")));
        } else if (!strcmp(func, "topic_restore_steps")) {
            out = w6_runtopics_d5aa88_topic_restore_steps();
        } else {
            out = json_string("UNKNOWN_FUNC");
        }

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
