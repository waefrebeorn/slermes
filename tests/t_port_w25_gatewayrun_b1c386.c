/* JSON-lines harness for the w25_gatewayrun_b1c386 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_gatewayrun_b1c386.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
    if (v) json_free(v);
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
        if (!strcmp(func, "normalize_name")) {
            out = w25_gatewayrun_b1c386_normalize_name(json_obj_get(in, "name"));
        } else if (!strcmp(func, "settled_flag")) {
            out = w25_gatewayrun_b1c386_settled_flag(json_obj_get(in, "value"));
        } else if (!strcmp(func, "serves_home")) {
            out = w25_gatewayrun_b1c386_serves_home(json_obj_get(in, "named"),
                                                    json_obj_get(in, "verdict"),
                                                    json_obj_get(in, "served"),
                                                    json_obj_get(in, "home_name"));
        } else if (!strcmp(func, "implied_serves")) {
            out = w25_gatewayrun_b1c386_implied_serves(json_obj_get(in, "named"),
                                                       json_obj_get(in, "verdict"));
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
        json_free(in);
    }
    return 0;
}
