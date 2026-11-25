/* Port gate harness for tools/arg_validation.py (gate w7_argvalidati_b61680). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_argvalidati_b61680.h"

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
        const char *key = json_get_str(in, "key", "");
        const char *name = json_get_str(in, "name", "");
        const char *valid = json_get_str(in, "valid", "");
        const char *result = json_get_str(in, "result", "");
        json_t *out = NULL;
        if (!strcmp(func, "with_choices")) {
            out = argval_with_choices(key, json_obj_get(in, "properties"));
        } else if (!strcmp(func, "missing_unknown")) {
            out = argval_missing_unknown(json_obj_get(in, "params"), json_obj_get(in, "args"),
                                         json_obj_get(in, "compat_args"),
                                         json_obj_get(in, "self_checked_args"));
        } else if (!strcmp(func, "flag_unknown_args")) {
            out = argval_flag_unknown_args(result, name, json_obj_get(in, "unknown"), valid);
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
