/* t_port_w11_modelcatal_563c9e.c - harness for acp_adapter/model_catalog.py port. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_modelcatal_563c9e.h"

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
        char *text = NULL;

        if (!strcmp(func, "encode")) {
            text = w11_modelcatal_563c9e_encode_model_choice(
                json_get_str(in, "provider", ""), json_get_str(in, "model", ""));
            out = json_string(text ? text : "");
        } else if (!strcmp(func, "semantic")) {
            text = w11_modelcatal_563c9e_semantic_provider(
                json_get_str(in, "provider_id", ""), json_get_str(in, "normalized", ""));
            out = json_string(text ? text : "");
        } else if (!strcmp(func, "empty_applies")) {
            out = json_bool(w11_modelcatal_563c9e_empty_catalog_applies(
                json_get_str(in, "provider_id", ""),
                json_obj_get(in, "candidates"), json_obj_get(in, "norm_map")));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        free(text);

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
