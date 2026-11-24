/* JSON-lines harness for the w23_gatewaywin_830338 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_gatewaywin_830338.h"

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
        if (!strcmp(func, "normalize_path")) {
            char *s = w23_gatewaywin_830338_normalize_path(json_get_str(in, "value", ""));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "join_path")) {
            char *s = w23_gatewaywin_830338_join_path(json_get_str(in, "base", ""),
                                                      json_get_str(in, "part", ""));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "targets_this_home")) {
            out = json_bool(w23_gatewaywin_830338_targets_this_home(
                json_get_str(in, "text", ""), json_get_str(in, "hermes_home", "")) != 0);
        } else if (!strcmp(func, "artifacts")) {
            out = w23_gatewaywin_830338_artifacts(json_obj_get(in, "world"));
        } else if (!strcmp(func, "warn")) {
            out = w23_gatewaywin_830338_warn(json_obj_get(in, "world"));
        } else if (!strcmp(func, "remove")) {
            out = w23_gatewaywin_830338_remove(json_obj_get(in, "world"));
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
