#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_methodsses_0a2164.h"

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
        if (!strcmp(func, "str_param")) {
            out = w15_methodsses_0a2164_str_param(json_obj_get(in, "params"),
                    json_get_str(in, "key", ""), json_get_str(in, "default", ""));
        } else if (!strcmp(func, "int_param")) {
            out = w15_methodsses_0a2164_int_param(json_obj_get(in, "params"),
                    json_get_str(in, "key", ""), (long long)json_get_num(in, "default", 0));
        } else if (!strcmp(func, "denied")) {
            out = w15_methodsses_0a2164_denied_source(json_obj_get(in, "row"));
        } else if (!strcmp(func, "auto_denied")) {
            out = w15_methodsses_0a2164_auto_resume_denied_source(json_obj_get(in, "row"));
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
