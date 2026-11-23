#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_modelspric_4eacb7.h"

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
        char *s = NULL;
        if (!strcmp(func, "strip_v1")) {
            s = w12_modelspric_4eacb7_strip_v1(json_get_str(in, "url", ""));
            out = json_string(s ? s : "");
        } else if (!strcmp(func, "format_price")) {
            s = w12_modelspric_4eacb7_format_price_per_mtok(json_get_str(in, "s", ""));
            out = json_string(s ? s : "");
        } else if (!strcmp(func, "price_float")) {
            out = w12_modelspric_4eacb7_price_float(json_obj_get(in, "raw"),
                     json_get_bool(in, "positive", false) ? 1 : 0);
        } else if (!strcmp(func, "sale_pct")) {
            out = w12_modelspric_4eacb7_sale_pct(json_obj_get(in, "current"), json_obj_get(in, "original"));
        } else if (!strcmp(func, "sale_discount")) {
            out = w12_modelspric_4eacb7_compute_sale_discount(
                    json_obj_get(in, "prompt"), json_obj_get(in, "completion"), json_obj_get(in, "original"));
        } else if (!strcmp(func, "pricing_entry")) {
            out = w12_modelspric_4eacb7_pricing_entry(json_obj_get(in, "pricing"),
                    json_get_str(in, "prompt_key", "prompt"), json_get_str(in, "completion_key", "completion"));
        } else if (!strcmp(func, "per_token")) {
            s = w12_modelspric_4eacb7_per_token(json_obj_get(in, "per_mtok"));
            out = json_string(s ? s : "");
        } else if (!strcmp(func, "catalog_items")) {
            out = w12_modelspric_4eacb7_catalog_items(json_obj_get(in, "payload"));
        } else if (!strcmp(func, "restrict")) {
            out = w12_modelspric_4eacb7_restrict_to_nous_policy(
                    json_obj_get(in, "model_ids"), json_obj_get(in, "allowed"),
                    json_get_bool(in, "rescue_empty", false) ? 1 : 0);
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
        free(s);
        json_free(out);
        json_free(in);
    }
    return 0;
}
