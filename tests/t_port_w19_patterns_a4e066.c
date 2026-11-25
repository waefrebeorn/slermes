#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_patterns_a4e066.h"

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
        const char *path = json_get_str(in, "path", "");
        json_t *out = NULL;
        if (!strcmp(func, "js_only")) {
            out = json_bool(w19_patterns_a4e066_js_only(path) ? true : false);
        } else if (!strcmp(func, "py_only")) {
            out = json_bool(w19_patterns_a4e066_py_only(path) ? true : false);
        } else if (!strcmp(func, "not_docs")) {
            out = json_bool(w19_patterns_a4e066_not_docs(path) ? true : false);
        } else if (!strcmp(func, "count")) {
            out = json_number((double)w19_patterns_a4e066_count());
        } else if (!strcmp(func, "names")) {
            out = w19_patterns_a4e066_names();
        } else if (!strcmp(func, "pattern")) {
            out = w19_patterns_a4e066_pattern(json_get_str(in, "name", ""), json_obj_get(in, "probe_paths"));
        } else if (!strcmp(func, "rule")) {
            out = w19_patterns_a4e066_rule(json_get_str(in, "name", ""), json_get_str(in, "reminder", ""),
                                           json_obj_get(in, "triggers"));
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
