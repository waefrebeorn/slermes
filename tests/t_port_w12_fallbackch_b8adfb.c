#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_fallbackch_b8adfb.h"

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
        if (!strcmp(func, "canonical")) {
            out = w12_fallbackch_b8adfb_canonical_model_id(json_get_str(in, "model", ""));
        } else if (!strcmp(func, "answered")) {
            out = w12_fallbackch_b8adfb_answered(json_get_str(in, "requested", ""),
                                                 json_get_str(in, "returned", ""));
        } else if (!strcmp(func, "status")) {
            json_t *iv = json_obj_get(in, "inference");
            int inf = 0; /* None */
            if (iv && iv->type == JSON_BOOL) inf = iv->bool_val ? 1 : 2;
            out = w12_fallbackch_b8adfb_check_status(
                    json_get_bool(in, "configured", 0) ? 1 : 0,
                    json_get_bool(in, "auth_ready", 0) ? 1 : 0,
                    inf, json_get_str(in, "error_class", ""));
        } else if (!strcmp(func, "secrets")) {
            out = w12_fallbackch_b8adfb_secret_values(json_obj_get(in, "candidates"));
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
