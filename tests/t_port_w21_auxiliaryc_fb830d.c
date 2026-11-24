/* JSON-lines harness for the w21_auxiliaryc_fb830d gate
 * (agent/auxiliary_client_registry.py).
 *
 * funcs: arms | arm_for {"auth_type"} | log_once {"keys","seen"} |
 *        minimax {"probe","state","req_model","aux_model","creds","creds_error",
 *                 "build_import_error","seen"}
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_auxiliaryc_fb830d.h"

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
        if (!strcmp(func, "arms")) {
            out = w21_auxiliaryc_fb830d_registry_arms();
        } else if (!strcmp(func, "arm_for")) {
            out = w21_auxiliaryc_fb830d_arm_for(json_get_str(in, "auth_type", ""));
        } else if (!strcmp(func, "log_once")) {
            out = w21_auxiliaryc_fb830d_log_once(json_obj_get(in, "keys"),
                                                 json_obj_get(in, "seen"));
        } else if (!strcmp(func, "minimax")) {
            out = w21_auxiliaryc_fb830d_minimax_resolve(in);
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
