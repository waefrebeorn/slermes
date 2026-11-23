#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_userfacing_57dafa.h"

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
        if (!strcmp(func, "copies")) {
            out = w5_userfacing_57dafa_copies(json_obj_get(in, "dirs"),
                        json_get_bool(in, "applies", false) ? 1 : 0,
                        json_obj_get(in, "sealed"), json_obj_get(in, "copies_map"));
        } else if (!strcmp(func, "path")) {
            char *p = w5_userfacing_57dafa_user_facing_path(
                        json_get_str(in, "path", ""),
                        json_obj_get(in, "store"), json_obj_get(in, "ours"),
                        json_obj_get(in, "python_copies"), json_obj_get(in, "stub_flags"));
            out = p ? json_string(p) : json_null();
            free(p);
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
