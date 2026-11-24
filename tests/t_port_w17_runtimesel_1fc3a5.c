/* JSON-lines harness for the w17_runtimesel_1fc3a5 gate. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_runtimesel_1fc3a5.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "version_spec")) {
            int v = w17_runtimesel_1fc3a5_version_spec_matches(
                json_get_str(in, "spec", ""),
                json_get_str(in, "version", ""),
                json_get_str(in, "uv_root_name", ""));
            out = json_bool(v ? 1 : 0);
        } else if (!strcmp(func, "overlaps")) {
            int v = w17_runtimesel_1fc3a5_overlaps(
                json_get_str(in, "a", ""), json_get_str(in, "b", ""));
            out = json_bool(v ? 1 : 0);
        } else if (!strcmp(func, "strip_prefixes")) {
            out = w17_runtimesel_1fc3a5_strip_prefixes(json_obj_get(in, "words"));
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
