#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_argcoerce.h"

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
        if (!strcmp(func, "number")) {
            out = argcoerce_coerce_number(json_get_str(in, "value", ""),
                                          json_get_bool(in, "integer_only", false) ? 1 : 0);
        } else if (!strcmp(func, "boolean")) {
            out = argcoerce_coerce_boolean(json_get_str(in, "value", ""));
        } else if (!strcmp(func, "allows_null")) {
            out = json_bool(argcoerce_schema_allows_null(json_obj_get(in, "schema")) ? true : false);
        } else if (!strcmp(func, "accepts_kind")) {
            out = json_bool(argcoerce_schema_accepts_kind(json_obj_get(in, "schema"),
                             json_get_str(in, "kind", "array"), json_obj_get(in, "root")) ? true : false);
        } else if (!strcmp(func, "coerce_value")) {
            out = argcoerce_coerce_value(json_get_str(in, "value", ""),
                                         json_obj_get(in, "expected_type"), json_obj_get(in, "schema"));
        } else if (!strcmp(func, "normalize")) {
            out = argcoerce_normalize_json_strings(json_obj_get(in, "value"),
                        json_obj_get(in, "schema"), json_obj_get(in, "root"));
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
