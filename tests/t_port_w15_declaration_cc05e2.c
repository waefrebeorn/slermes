#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_declaration_cc05e2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static const char *jstrd(const json_t *in, const char *k, const char *d) {
    const char *s = jstr(json_obj_get(in, k));
    return s ? s : d;
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
        if (!strcmp(func, "gpu_label")) {
            char *s = w15_declaration_cc05e2_gpu_label(jstr(json_obj_get(in, "gpu")));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "location_is_rooted")) {
            out = json_bool(w15_declaration_cc05e2_location_is_rooted(
                jstrd(in, "location", ""), jstrd(in, "osf", "")) ? 1 : 0);
        } else if (!strcmp(func, "is_relative_name")) {
            out = json_bool(w15_declaration_cc05e2_is_relative_name(
                json_obj_get(in, "value"), json_get_bool(in, "allow_dirs", 0)) ? 1 : 0);
        } else if (!strcmp(func, "parse_requires")) {
            out = w15_declaration_cc05e2_parse_requires(json_obj_get(in, "raw"),
                                                        json_obj_get(in, "app_per_os"));
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
