#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_imports_8a5933.h"

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
        const char *field = json_get_str(in, "field", "f");
        json_t *out = NULL;
        if (!strcmp(func, "text_or_none")) {
            out = w8_imports_8a5933_text_or_none(json_obj_get(in, "value"), field);
        } else if (!strcmp(func, "int_or_none")) {
            out = w8_imports_8a5933_int_or_none(json_obj_get(in, "value"), field);
        } else if (!strcmp(func, "non_finite_path")) {
            char *p = w8_imports_8a5933_non_finite_path(json_obj_get(in, "data"), json_get_str(in, "path", "root"));
            out = p ? json_string(p) : json_null();
            free(p);
        } else if (!strcmp(func, "bounds")) {
            out = w8_imports_8a5933_bounds();
        } else if (!strcmp(func, "field_tables")) {
            out = w8_imports_8a5933_field_tables();
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