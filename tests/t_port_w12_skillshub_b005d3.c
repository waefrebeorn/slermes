#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_skillshub_b005d3.h"

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
        if (!strcmp(func, "strip_prefix")) {
            out = w12_skillshub_b005d3_strip_prefix(json_obj_get(in,"value"), json_obj_get(in,"prefixes"));
        } else if (!strcmp(func, "clean_rel_parts")) {
            out = w12_skillshub_b005d3_clean_rel_parts(json_obj_get(in,"path"));
        } else if (!strcmp(func, "entry_provider")) {
            out = w12_skillshub_b005d3_entry_provider(json_obj_get(in,"entry"));
        } else if (!strcmp(func, "optional_rel")) {
            out = w12_skillshub_b005d3_optional_rel(json_obj_get(in,"identifier"));
        } else if (!strcmp(func, "upstream_pointer")) {
            out = w12_skillshub_b005d3_upstream_pointer(json_obj_get(in,"fm"));
        } else if (!strcmp(func, "to_meta")) {
            out = w12_skillshub_b005d3_to_meta(json_obj_get(in,"entry"));
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
