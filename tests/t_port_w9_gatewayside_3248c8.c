#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_gatewayside_3248c8.h"

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
        if (!strcmp(func, "is_side_event")) {
            out = json_bool(w9_gatewayside_3248c8_is_side_event(json_get_str(in, "name", "")) ? true : false);
        } else if (!strcmp(func, "connection_for")) {
            out = json_bool(w9_gatewayside_3248c8_connection_for(
                      json_get_bool(in, "has_store", false) ? 1 : 0,
                      json_get_bool(in, "session_open", false) ? 1 : 0,
                      json_get_bool(in, "final", false) ? 1 : 0) ? true : false);
        } else if (!strcmp(func, "start")) {
            out = w9_gatewayside_3248c8_start(json_obj_get(in, "result"), json_get_str(in, "verb", "bg"));
        } else if (!strcmp(func, "answer")) {
            out = w9_gatewayside_3248c8_answer(json_obj_get(in, "payload"));
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
