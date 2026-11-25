#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_venvsync_d6306e.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "is_read_only_gateway_status")) {
            out = json_bool(w7_venvsync_d6306e_is_read_only_gateway_status(
                      json_obj_get(in, "command")) ? true : false);
        } else if (!strcmp(func, "supervised_child")) {
            out = json_bool(w7_venvsync_d6306e_supervised_child(
                      json_get_str(in, "supervised", ""),
                      json_get_str(in, "s6", "")) ? true : false);
        } else if (!strcmp(func, "completion_retry_constants")) {
            out = w7_venvsync_d6306e_completion_retry_constants();
        } else if (!strcmp(func, "string_set")) {
            out = w7_venvsync_d6306e_string_set(json_get_str(in, "which", ""));
        } else if (!strcmp(func, "relaunch_command")) {
            out = w7_venvsync_d6306e_relaunch_command(
                json_get_str(in, "python", ""), json_get_str(in, "root", ""),
                json_obj_get(in, "argv"), json_obj_get(in, "original"),
                json_get_str(in, "module", ""));
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
