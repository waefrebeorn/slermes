/* t_port_w19_telegramme_82fdeb.c - harness for src/plugins/platforms/telegram/telegram_media_flood.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_telegramme_82fdeb.h"

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
        if (!strcmp(func, "flood_wait_seconds")) {
            json_t *ra = json_obj_get(in, "retry_after");
            int has = (ra && ra->type != JSON_NULL) ? 1 : 0;
            out = w19_telegramme_82fdeb_flood_wait_seconds(has, ra, json_get_str(in, "text", ""));
        } else if (!strcmp(func, "known_window_message")) {
            out = json_string(w19_telegramme_82fdeb_known_window_message(json_get_num(in, "wait", 0.0)));
        } else if (!strcmp(func, "recover_action")) {
            out = w19_telegramme_82fdeb_recover_action(json_get_bool(in, "wait_present", false),
                                                       json_get_num(in, "wait", 0.0),
                                                       (int)json_get_num(in, "stage", 0.0),
                                                       json_get_bool(in, "known_window", false) ? 1 : 0,
                                                       json_get_num(in, "inline_cap", 5.0));
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
