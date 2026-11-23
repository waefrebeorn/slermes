#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_unreachable_01eb9c.h"

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
        if (!strcmp(func, "retry_enabled")) {
            out = unreachable_retry_enabled(json_obj_get(in, "cfg"));
        } else if (!strcmp(func, "is_retry_fire")) {
            out = unreachable_is_retry_fire(json_obj_get(in, "job"),
                                             json_get_str(in, "next_run", NULL));
        } else if (!strcmp(func, "clear_state")) {
            out = unreachable_clear_state(json_obj_get(in, "job"));
        } else if (!strcmp(func, "ladder_applies")) {
            out = unreachable_ladder_applies(json_obj_get(in, "job"),
                                              json_get_bool(in, "recurring", false),
                                              json_obj_get(in, "cfg"));
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
