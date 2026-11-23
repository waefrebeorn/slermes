#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_schedulerb_e4b4e1.h"

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
        if (!strcmp(func, "classify")) {
            out = w13_schedulerb_e4b4e1_classify(
                json_obj_get(in, "delivery_error"), json_obj_get(in, "should_deliver"),
                json_obj_get(in, "unresolved_origin"), json_obj_get(in, "normalized_deliver"),
                json_obj_get(in, "incident_acked"), json_obj_get(in, "success"),
                json_obj_get(in, "delivery_queued"), json_obj_get(in, "notification_suppressed"));
        } else if (!strcmp(func, "fail_empty")) {
            out = w13_schedulerb_e4b4e1_fail_empty(json_obj_get(in, "success"),
                    json_obj_get(in, "final_response"), json_obj_get(in, "error"));
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
