#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_gatewaycom_6e4e92.h"

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
        if (!strcmp(func, "deferred_pick_text")) {
            out = w15_gatewaycom_6e4e92_deferred_pick_text(json_obj_get(in, "result"));
        } else if (!strcmp(func, "refused_pick_text")) {
            out = w15_gatewaycom_6e4e92_refused_pick_text(json_obj_get(in, "update"));
        } else if (!strcmp(func, "summary_text")) {
            out = w15_gatewaycom_6e4e92_summary_text(json_obj_get(in, "result"));
        } else if (!strcmp(func, "model_text")) {
            out = w15_gatewaycom_6e4e92_model_text(json_get_str(in, "label", ""),
                                                   json_obj_get(in, "fields"));
        } else if (!strcmp(func, "tools_text")) {
            out = w15_gatewaycom_6e4e92_tools_text(json_obj_get(in, "info"));
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
