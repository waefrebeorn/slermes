#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_anonchalle_ed340f.h"

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
        if (!strcmp(func, "server_message")) {
            out = w12_anonchalle_ed340f_server_message(json_obj_get(in, "value"));
        } else if (!strcmp(func, "same_origin")) {
            out = json_bool(w12_anonchalle_ed340f_same_origin(
                     json_get_str(in, "url", ""), json_get_str(in, "portal", "")) ? true : false);
        } else if (!strcmp(func, "number")) {
            out = w12_anonchalle_ed340f_number(json_obj_get(in, "value"),
                                               json_get_num(in, "default", 0.0));
        } else if (!strcmp(func, "parse")) {
            out = w12_anonchalle_ed340f_parse(json_obj_get(in, "payload"),
                                              json_get_str(in, "portal", ""));
        } else if (!strcmp(func, "constants")) {
            out = w12_anonchalle_ed340f_constants();
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
