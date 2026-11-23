/* t_port_tenv.c — tenv gate harness (tui_gateway/_env.py). */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_tenv.h"

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
        const char *name = json_get_str(in, "name", "");

        /* Apply the fixture's env value BEFORE calling the function. */
        json_t *val = json_obj_get(in, "value");
        if (val && val->type == JSON_STRING) {
            setenv(name, val->str_val, 1);
        } else if (val && val->type == JSON_NULL) {
            unsetenv(name);
        } else if (val) {
            char *s = json_dumps(val, 0);
            setenv(name, s ? s : "", 1);
            free(s);
        } else {
            unsetenv(name);
        }

        json_t *out = NULL;
        if (!strcmp(func, "env_int")) {
            int64_t def = (int64_t)strtoll(json_get_str(in, "default", "0"), NULL, 10);
            out = json_number((double)tenv_env_int(name, def));
        } else if (!strcmp(func, "env_float")) {
            double def = json_get_num(in, "default", 0.0);
            out = json_number(tenv_env_float(name, def));
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
