#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_gatewayrun_de650a.h"

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
        if (!strcmp(func, "constants")) {
            out = w13_gatewayrun_de650a_constants();
        } else if (!strcmp(func, "attachable")) {
            out = w13_gatewayrun_de650a_attachable(json_get_str(in, "state", ""),
                        json_get_bool(in, "has_endpoint", false) ? 1 : 0,
                        json_get_str(in, "reason_code", ""),
                        json_get_str(in, "restart_reason", "restart_wait"));
        } else if (!strcmp(func, "control_home")) {
            out = w13_gatewayrun_de650a_control_home_for(json_get_str(in, "home", ""),
                                                          json_get_str(in, "control_home", ""));
        } else if (!strcmp(func, "endpoint")) {
            out = w13_gatewayrun_de650a_endpoint(json_obj_get(in, "payload"),
                        json_get_str(in, "home", ""), json_get_str(in, "control_home", ""));
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
