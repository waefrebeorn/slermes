/* t_port_w14_sessionact_06a3c5.c — oracle harness for
 * gateway/session_active_list.py (gate w14_sessionact_06a3c5). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_sessionact_06a3c5.h"

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

        if (!strcmp(func, "status")) {
            out = w14_sessionact_06a3c5_status(
                json_get_str(in, "execution_state", ""),
                json_get_bool(in, "asked", false) ? 1 : 0,
                json_obj_get(in, "pending"));
        } else if (!strcmp(func, "active_list")) {
            out = w14_sessionact_06a3c5_active_list(
                json_obj_get(in, "params"), json_obj_get(in, "capabilities"),
                json_obj_get(in, "entries"));
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
