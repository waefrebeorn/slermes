/* JSON-lines harness for the w21_runturnrun_ac2ad3 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_runturnrun_ac2ad3.h"
#include "gateway/run_turn_runner_approval_transport.c"

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
        if (!strcmp(func, "surfaces")) {
            out = w21_runturnrun_ac2ad3_surfaces();
        } else if (!strcmp(func, "platform_value")) {
            char *s = w21_runturnrun_ac2ad3_platform_value(json_obj_get(in, "source"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "transport")) {
            out = w21_runturnrun_ac2ad3_transport(json_obj_get(in, "source"),
                                                  json_get_bool(in, "local_viewer", false));
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
