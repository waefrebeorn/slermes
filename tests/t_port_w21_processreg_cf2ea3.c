/* JSON-lines harness for the w21_processreg_cf2ea3 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_processreg_cf2ea3.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "in_env")) {
            json_t *sess = json_obj_get(in, "session");
            const char *key = json_get_str(in, "key", "");
            const char *tag = json_get_str(in, "env_tag", NULL);
            out = json_bool(w21_processreg_cf2ea3_in_env(sess, key, tag));
        } else if (!strcmp(func, "has_running")) {
            out = w21_processreg_cf2ea3_has_running(json_obj_get(in, "world"));
        } else if (!strcmp(func, "settle")) {
            out = w21_processreg_cf2ea3_settle(json_obj_get(in, "world"));
        } else if (!strcmp(func, "env_probe")) {
            out = w21_processreg_cf2ea3_env_probe();
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
            emit_key(k, json_object_get_at(in, i));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
