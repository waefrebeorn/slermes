#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_providerbas_bb26d7.h"

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
        const char *name = json_get_str(in, "name", "");
        json_t *out = NULL;
        if (!strcmp(func, "display_name")) {
            char *s = w6_providerbas_bb26d7_display_name(name);
            out = s ? json_string(s) : json_null(); free(s);
        } else if (!strcmp(func, "setup_schema")) {
            out = w6_providerbas_bb26d7_setup_schema(name);
        } else if (!strcmp(func, "catalog_display_name")) {
            char *s = w6_providerbas_bb26d7_catalog_display_name(name);
            out = s ? json_string(s) : json_null(); free(s);
        } else if (!strcmp(func, "catalog_is_available")) {
            out = json_bool(w6_providerbas_bb26d7_catalog_is_available() ? true : false);
        } else if (!strcmp(func, "catalog_list_models")) {
            out = w6_providerbas_bb26d7_catalog_list_models();
        } else if (!strcmp(func, "catalog_default_model")) {
            out = w6_providerbas_bb26d7_catalog_default_model(json_obj_get(in, "models"));
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
        json_free(out);
        json_free(in);
    }
    return 0;
}
