#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w17_setupagent_0fd570.h"

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

        if (!strcmp(func, "constants")) {
            out = w17_setupagent_0fd570_constants();
        } else if (!strcmp(func, "argv")) {
            out = w17_setupagent_0fd570_import_argv(
                json_get_str(in, "executable", ""), json_get_str(in, "source", ""),
                json_obj_get(in, "extra_args"), json_obj_get(in, "selector"),
                json_get_bool(in, "from_setup", false));
        } else if (!strcmp(func, "hint")) {
            char *s = w17_setupagent_0fd570_migrate_hint(
                json_obj_get(in, "found"), json_get_bool(in, "show_found", false));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "context")) {
            char *s = w17_setupagent_0fd570_found_context(
                json_get_str(in, "user_home", ""), json_obj_get(in, "found"));
            out = s ? json_string(s) : json_null();
            free(s);
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
