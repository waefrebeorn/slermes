#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_modelsdete_a35568.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *jstr(json_t *v){ return (v && v->type==JSON_STRING) ? v->str_val : NULL; }
static int jint(json_t *v, int dflt){ return (v && v->type==JSON_NUMBER) ? (int)v->num_val : dflt; }
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "catalog_match")) {
            out = w10_modelsdete_a35568_current_provider_catalog_match(
                jstr(json_obj_get(in,"model_name")), jstr(json_obj_get(in,"current_provider")),
                jstr(json_obj_get(in,"normalized_provider")),
                jint(json_obj_get(in,"catalog_error"),0), json_obj_get(in,"catalog"));
        } else if (!strcmp(func, "owns_vendor")) {
            int r = w10_modelsdete_a35568_current_provider_owns_vendor(
                jstr(json_obj_get(in,"model_name")), jstr(json_obj_get(in,"current_provider")),
                jstr(json_obj_get(in,"normalized_provider")),
                jint(json_obj_get(in,"is_aggregator"),0),
                jstr(json_obj_get(in,"vendor_of_model")), json_obj_get(in,"native_vendors"));
            out = json_bool(r ? 1 : 0);
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
