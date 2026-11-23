#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_verification_1c81c6.h"

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
        json_t *args = json_obj_get(in, "in");
        json_t *out = NULL;
        if (!strcmp(func, "non_code")) {
            out = json_bool(w9_verification_1c81c6_is_non_code_path(
                json_get_str(args, "path", "")) ? true : false);
        } else if (!strcmp(func, "format_paths")) {
            out = w9_verification_1c81c6_format_changed_paths(json_obj_get(args, "paths"));
        } else if (!strcmp(func, "enabled")) {
            json_t *envv = json_obj_get(args, "env");
            const char *env = (envv && envv->type == JSON_STRING) ? envv->str_val : NULL;
            out = json_bool(w9_verification_1c81c6_verify_on_stop_enabled_ex(
                env, json_obj_get(args, "config"),
                json_get_bool(args, "messaging_surface", false) ? 1 : 0) ? true : false);
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
