#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_webserver_1a1654.h"

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
        if (!strcmp(func, "strlist")) {
            out = w16_webserver_1a1654_string_list(json_obj_get(in, "value"));
        } else if (!strcmp(func, "kind")) {
            out = w16_webserver_1a1654_schema_field_kind(
                json_obj_get(in, "raw"), json_obj_get(in, "choices"));
        } else if (!strcmp(func, "coerce")) {
            out = w16_webserver_1a1654_coerce_bool(
                json_obj_get(in, "value"), json_obj_get(in, "default"));
        } else if (!strcmp(func, "normschema")) {
            out = w16_webserver_1a1654_normalize_schema(json_obj_get(in, "raw_schema"));
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
