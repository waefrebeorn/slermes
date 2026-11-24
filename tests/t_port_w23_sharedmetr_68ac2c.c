/* JSON-lines harness for the w23_sharedmetr_68ac2c gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_sharedmetr_68ac2c.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "metric_name")) {
            out = json_string(w23_sharedmetr_68ac2c_metric());
        } else if (!strcmp(func, "truthy")) {
            out = json_bool(w23_sharedmetr_68ac2c_truthy(json_obj_get(in, "value")) != 0);
        } else if (!strcmp(func, "prepare_ahead_state")) {
            out = json_string(w23_sharedmetr_68ac2c_prepare_ahead_state(json_obj_get(in, "config")));
        } else if (!strcmp(func, "wait_bucket")) {
            out = json_string(w23_sharedmetr_68ac2c_wait_bucket(json_obj_get(in, "seconds")));
        } else if (!strcmp(func, "fields")) {
            out = w23_sharedmetr_68ac2c_fields(in);
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
