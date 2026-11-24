/* JSON-lines harness for the w21_errors_5bcef1 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_errors_5bcef1.h"
#include "tools/connectors/gateway/errors.c"

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
        if (!strcmp(func, "parse")) {
            out = w21_errors_5bcef1_parse(json_obj_get(in, "status"),
                                          json_obj_get(in, "body"),
                                          json_obj_get(in, "headers"));
        } else if (!strcmp(func, "render")) {
            out = w21_errors_5bcef1_render(in);
        } else if (!strcmp(func, "default_message")) {
            char *s = w21_errors_5bcef1_default_message(json_get_str(in, "code", ""),
                                                        json_get_str(in, "connector", ""));
            out = json_string(s ? s : "");
            free(s);
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
