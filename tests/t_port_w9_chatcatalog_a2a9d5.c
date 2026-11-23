#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_chatcatalog_a2a9d5.h"

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
        if (!strcmp(func, "model_id_is_generation"))
            out = json_bool(w9_chatcatalog_a2a9d5_model_id_is_generation(json_obj_get(in, "model_id")) ? true : false);
        else if (!strcmp(func, "tag_list_is_generation"))
            out = json_bool(w9_chatcatalog_a2a9d5_tag_list_is_generation(json_obj_get(in, "tags")) ? true : false);
        else if (!strcmp(func, "output_modalities_are_generation"))
            out = json_bool(w9_chatcatalog_a2a9d5_output_modalities_are_generation(json_obj_get(in, "outputs")) ? true : false);
        else if (!strcmp(func, "catalog_item_is_generation"))
            out = json_bool(w9_chatcatalog_a2a9d5_catalog_item_is_generation(json_obj_get(in, "item")) ? true : false);
        else
            out = json_string("UNKNOWN_FUNC");
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
