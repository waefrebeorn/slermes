#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_i18nlayers_f42323.h"

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
        json_t *value = json_obj_get(in, "value");
        if (!strcmp(func, "normalize_language_id")) {
            char *s = w6_i18nlayers_f42323_normalize_language_id(value);
            out = json_string(s ? s : ""); free(s);
        } else if (!strcmp(func, "is_language_id")) {
            out = json_bool(w6_i18nlayers_f42323_is_language_id(value));
        } else if (!strcmp(func, "flatten")) {
            out = w6_i18nlayers_f42323_flatten(value);
        } else if (!strcmp(func, "non_text_leaves")) {
            out = w6_i18nlayers_f42323_non_text_leaves(value);
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
