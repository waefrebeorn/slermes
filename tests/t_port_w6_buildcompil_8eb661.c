/* t_port_w6_buildcompil_8eb661.c — JSON-lines harness for pm/build_compilers.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_buildcompil_8eb661.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        char *r = NULL;

        if (!strcmp(func, "missing_compiler_remedy")) {
            const struct json_t *v = json_obj_get(in, "output");
            const char *text = (v && v->type == JSON_STRING) ? v->str_val : "";
            r = w6_buildcompil_8eb661_missing_compiler_remedy(text);
            out = json_string(r ? r : "");
        } else if (!strcmp(func, "linux_defaults")) {
            out = w6_buildcompil_8eb661_linux_defaults(json_obj_get(in, "recorded"),
                      json_obj_get(in, "env"), json_obj_get(in, "available"));
        } else if (!strcmp(func, "pinned_build_defaults")) {
            out = w6_buildcompil_8eb661_pinned_build_defaults(json_get_str(in, "platform", ""),
                      json_obj_get(in, "recorded"), json_obj_get(in, "env"),
                      json_obj_get(in, "available"));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        free(r);

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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
