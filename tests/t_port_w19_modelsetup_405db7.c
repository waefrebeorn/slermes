#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_modelsetup_405db7.h"

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
        if (!strcmp(func, "geo_prefix")) {
            const char *region_name = json_get_str(in, "region_name", "");
            out = json_string(w19_modelsetup_405db7_region_geo_prefix(region_name));
        } else if (!strcmp(func, "routable")) {
            const char *model_id = json_get_str(in, "model_id", "");
            const char *region_name = json_get_str(in, "region_name", "");
            out = json_bool(w19_modelsetup_405db7_model_routable(model_id, region_name) ? 1 : 0);
        } else if (!strcmp(func, "text_model_ids")) {
            json_t *models = json_obj_get(in, "models");
            const char *region = json_obj_get(in, "region") ? json_get_str(in, "region", "") : NULL;
            out = w19_modelsetup_405db7_text_model_ids(models, region);
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
