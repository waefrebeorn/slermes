#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_operations_2c07e3.h"

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
        const char *root = json_get_str(in, "root", "");
        json_t *out = json_object();
        if (!strcmp(func, "environment_root")) {
            char *p = NULL;
            if (w5_operations_2c07e3_environment_root(name, root, &p)) {
                json_set(out, "path", json_string(p ? p : ""));
                free(p);
            } else {
                json_set(out, "error", json_string("environment name must be a simple package name"));
            }
        } else {
            json_set(out, "error", json_string("UNKNOWN_FUNC"));
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
