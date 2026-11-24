#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_telegramhe_c40f34.h"

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
        if (!strcmp(func, "insert_by_arrival")) {
            out = w19_telegramhe_c40f34_insert_by_arrival(json_obj_get(in, "held_stamps"),
                                                          json_obj_get(in, "seq"));
        } else if (!strcmp(func, "note_arrival")) {
            out = w19_telegramhe_c40f34_note_arrival(json_obj_get(in, "existing_stamp"),
                                                     (int)json_get_num(in, "counter", 0));
        } else if (!strcmp(func, "hold_event")) {
            out = w19_telegramhe_c40f34_hold_event(json_obj_get(in, "queue"),
                                                   json_obj_get(in, "event"),
                                                   json_get_bool(in, "fatal", false),
                                                   json_get_bool(in, "drop_delayed", false),
                                                   (int)json_get_num(in, "max_n", 64),
                                                   json_get_bool(in, "schedule", true),
                                                   json_obj_get(in, "successor_queue"));
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
