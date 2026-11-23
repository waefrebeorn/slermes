#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_schedulerg_cccc6c.h"

static void emit_key(const char *key, struct json_t *v) {
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
        struct json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        const char *env_value = json_has(in, "env_value") ? json_get_str(in, "env_value", "") : NULL;
        const char *state = json_get_str(in, "state", "absent");
        const char *reason_code = json_has(in, "reason_code") ? json_get_str(in, "reason_code", "") : NULL;
        struct json_t *out = NULL;
        if (!strcmp(func, "gateway_unavailable")) {
            const char *p = w14_schedulerg_cccc6c_gateway_unavailable(env_value, state, reason_code);
            out = p ? json_string(p) : json_null();
        } else if (!strcmp(func, "refuse_jobs")) {
            out = w14_schedulerg_cccc6c_refuse_jobs(json_obj_get(in, "due_jobs"),
                                                    env_value, state, reason_code);
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
