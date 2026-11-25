/* t_port_w8_sessionshel_fe6651.c -- JSON-lines harness for src/gateway/session_shell.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_sessionshel_fe6651.h"

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
        if (!strcmp(func, "timeout_seconds")) {
            out = w8_sessionshel_fe6651_timeout_seconds();
        } else if (!strcmp(func, "local_backends")) {
            out = w8_sessionshel_fe6651_local_backends();
        } else if (!strcmp(func, "handler_names")) {
            out = w8_sessionshel_fe6651_handler_names();
        } else if (!strcmp(func, "params_valid")) {
            out = w8_sessionshel_fe6651_params_valid(json_obj_get(in, "params"),
                json_obj_get(in, "session_id"), json_obj_get(in, "command"));
        } else if (!strcmp(func, "backend_is_local")) {
            out = w8_sessionshel_fe6651_backend_is_local(json_obj_get(in, "terminal_env"));
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
