#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_providerreg_49f364.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *owned_string_or_null(char *s) {
    if (!s) return json_null();
    json_t *v = json_string(s);
    free(s);
    return v;
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
        if (!strcmp(func, "lower_key")) {
            out = owned_string_or_null(w9_providerreg_49f364_lower_key(json_get_str(in, "name", "")));
        } else if (!strcmp(func, "exact_key")) {
            out = owned_string_or_null(w9_providerreg_49f364_exact_key(json_get_str(in, "name", "")));
        } else if (!strcmp(func, "configured")) {
            json_t *rawv = json_obj_get(in, "raw");
            const char *raw = (rawv && rawv->type == JSON_STRING) ? rawv->str_val : NULL;
            out = owned_string_or_null(w9_providerreg_49f364_configured_ex(raw));
        } else if (!strcmp(func, "is_available_safe")) {
            out = json_bool(w9_providerreg_49f364_is_available_safe(
                        json_get_bool(in, "available", false),
                        json_get_bool(in, "raises", false)));
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
