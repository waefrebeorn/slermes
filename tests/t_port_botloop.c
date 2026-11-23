#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_botloop.h"

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
        if (!strcmp(func, "as_bool")) {
            out = json_bool(botloop_as_bool(json_obj_get(in, "raw"),
                                            json_get_bool(in, "default", false)));
        } else if (!strcmp(func, "as_positive")) {
            out = json_number(botloop_as_positive(json_obj_get(in, "raw"),
                                                  json_get_num(in, "default", 0.0)));
        } else if (!strcmp(func, "as_positive_int")) {
            out = json_number((double)botloop_as_positive_int(
                json_obj_get(in, "raw"), (long long)json_get_num(in, "default", 0.0)));
        } else if (!strcmp(func, "configured_bot_user")) {
            out = json_bool(botloop_configured_bot_user(json_obj_get(in, "extra"),
                                                        json_obj_get(in, "user_id")));
        } else if (!strcmp(func, "settings")) {
            out = botloop_settings_from_block(json_obj_get(in, "block"));
        } else if (!strcmp(func, "guard")) {
            out = botloop_guard(json_obj_get(in, "settings"), json_obj_get(in, "ops"));
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
