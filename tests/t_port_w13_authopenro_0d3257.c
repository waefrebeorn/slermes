/* t_port_w13_authopenro_0d3257.c — oracle harness for
 * hermes_cli/auth_openrouter.py (gate w13_authopenro_0d3257). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_authopenro_0d3257.h"

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

        if (!strcmp(func, "exchange_code")) {
            /* int64 status carried as a decimal string (libjson numbers are double). */
            long long status = strtoll(json_get_str(in, "status", "200"), NULL, 10);
            out = w13_authopenro_0d3257_exchange_code(
                json_get_str(in, "code", "CODE"),
                json_get_str(in, "verifier", "VER"),
                status,
                json_get_str(in, "text", ""),
                json_obj_get(in, "payload"),
                json_get_bool(in, "json_error", false) ? 1 : 0,
                json_get_str(in, "post_error", ""));
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
