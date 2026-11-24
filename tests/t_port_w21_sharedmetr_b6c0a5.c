/* JSON-lines harness for the w21_sharedmetr_b6c0a5 gate
 * (hermes_cli/observability/shared_metrics_drift.py). One request object per
 * stdin line; one response line carrying the computed "out" plus every input
 * key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_sharedmetr_b6c0a5.h"
#include "hermes_cli/observability/shared_metrics_drift.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "shape_known")) {
            out = w21_sharedmetr_b6c0a5_shape_known(json_obj_get(in, "metric"),
                                                    json_obj_get(in, "dimensions"));
        } else if (!strcmp(func, "conform")) {
            out = w21_sharedmetr_b6c0a5_conform(json_obj_get(in, "metric"),
                                                json_obj_get(in, "dimensions"),
                                                json_obj_get(in, "catalog"));
        } else if (!strcmp(func, "metric_identifier")) {
            json_t *v = json_obj_get(in, "value");
            json_t *m = json_obj_get(in, "max_length");
            int max_length = (m && m->type == JSON_NUMBER) ? (int)m->num_val : 0;
            char *ident = w21_sharedmetr_b6c0a5_metric_identifier(
                (v && v->type == JSON_STRING) ? v->str_val : NULL, max_length);
            out = json_string(ident ? ident : "unknown");
            free(ident);
        } else if (!strcmp(func, "contract_shape")) {
            out = w21_sharedmetr_b6c0a5_contract_shape();
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
