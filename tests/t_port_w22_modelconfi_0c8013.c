/* JSON-lines harness for the w22_modelconfi_0c8013 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_modelconfi_0c8013.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "update_config")) {
            out = w22_modelconfi_0c8013_update_config(
                json_get_str(in, "row_config", NULL),
                json_get_str(in, "key", NULL),
                json_obj_get(in, "value"));
        } else if (!strcmp(func, "json_extract")) {
            char *s = w22_modelconfi_0c8013_json_extract(
                json_get_str(in, "expression", NULL),
                json_get_str(in, "path", NULL));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "select_ids_sql")) {
            char *s = w22_modelconfi_0c8013_select_ids_sql(json_get_str(in, "key", NULL));
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
