#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_update_6d82c1.h"

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

        if (!strcmp(func, "version_key")) {
            out = w5_update_6d82c1_version_key(json_get_str(in, "version", ""));
        } else if (!strcmp(func, "minor_of")) {
            out = w5_update_6d82c1_minor_of(json_get_str(in, "version", ""));
        } else if (!strcmp(func, "best_in_minor")) {
            out = w5_update_6d82c1_best_in_minor(
                json_obj_get(in, "versions"),
                (int)json_get_num(in, "major", 0),
                (int)json_get_num(in, "minor", 0));
        } else if (!strcmp(func, "resolve_best")) {
            json_t *locked_j = json_obj_get(in, "locked");
            int locked_present = locked_j && locked_j->type == JSON_STRING;
            out = w5_update_6d82c1_resolve_best(
                json_get_str(in, "name", ""),
                json_obj_get(in, "latest"),
                locked_present ? locked_j->str_val : "",
                locked_present,
                json_get_str(in, "style", "semver"));
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
