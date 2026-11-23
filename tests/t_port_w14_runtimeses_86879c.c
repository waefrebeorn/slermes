#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_runtimeses_86879c.h"

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
        if (!strcmp(func, "apply_row_annotations")) {
            out = w14_runtimeses_86879c_apply_row_annotations(
                json_obj_get(in, "messages"), json_obj_get(in, "annotations"));
        } else if (!strcmp(func, "session_number")) {
            out = w14_runtimeses_86879c_session_number(json_obj_get(in, "row"),
                json_get_str(in, "column", ""), json_get_str(in, "cast_kind", "int"),
                json_get_num(in, "zero", 0.0));
        } else if (!strcmp(func, "compression_cooldown")) {
            out = w14_runtimeses_86879c_compression_cooldown(
                json_obj_get(in, "row"), json_get_num(in, "now", 0.0));
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
