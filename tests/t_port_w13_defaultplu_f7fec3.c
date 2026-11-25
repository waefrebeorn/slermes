/* t_port_w13_defaultplu_f7fec3.c — oracle harness for hermes_cli/default_plugins.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_defaultplu_f7fec3.h"

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
        if (!strcmp(func, "attempt")) {
            out = w13_defaultplu_f7fec3_attempt(json_obj_get(in, "name"));
        } else if (!strcmp(func, "fetch_policy")) {
            out = w13_defaultplu_f7fec3_fetch_policy(json_obj_get(in, "env"),
                    json_obj_get(in, "security"), json_get_bool(in, "readable", 1));
        } else if (!strcmp(func, "fetch_allowed")) {
            out = w13_defaultplu_f7fec3_fetch_allowed(json_obj_get(in, "env"),
                    json_obj_get(in, "security"), json_get_bool(in, "readable", 1));
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
