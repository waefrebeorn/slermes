#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_telegramva_544569.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "match_card")) {
            out = w19_telegramva_544569_match_card(json_obj_get(in, "chat_id"),
                                                   json_obj_get(in, "user_id"),
                                                   json_obj_get(in, "replied_to"),
                                                   json_obj_get(in, "chat_type"),
                                                   json_get_num(in, "now", 0.0),
                                                   json_obj_get(in, "replies"),
                                                   json_obj_get(in, "retired"));
        } else if (!strcmp(func, "pop_reply")) {
            out = w19_telegramva_544569_pop_reply(json_obj_get(in, "replies"),
                                                  json_obj_get(in, "retired"),
                                                  (int)json_get_num(in, "pending", 0),
                                                  json_get_str(in, "chat_id", ""),
                                                  json_get_str(in, "message_id", ""));
        } else if (!strcmp(func, "prompt_text")) {
            out = w19_telegramva_544569_prompt_text(json_obj_get(in, "chat_id"),
                                                    json_obj_get(in, "user_id"),
                                                    json_obj_get(in, "site"),
                                                    json_obj_get(in, "hint"),
                                                    json_obj_get(in, "title_template"),
                                                    json_obj_get(in, "body_template"),
                                                    json_obj_get(in, "label_template"));
        } else if (!strcmp(func, "filter_allows")) {
            out = w19_telegramva_544569_filter_allows((int)json_get_num(in, "pending", 0),
                                                      json_obj_get(in, "message_text"));
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
