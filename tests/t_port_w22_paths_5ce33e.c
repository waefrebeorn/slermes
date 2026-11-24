/* JSON-lines harness for the w22_paths_5ce33e gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_paths_5ce33e.h"

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
        const char *func, *path, *name, *pattern;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        path = json_get_str(in, "path", "");
        name = json_get_str(in, "name", "");
        pattern = json_get_str(in, "pattern", "");
        if (!strcmp(func, "directory")) {
            out = w22_paths_5ce33e_directory(json_obj_get(in, "table"), path);
        } else if (!strcmp(func, "current")) {
            out = w22_paths_5ce33e_current(json_obj_get(in, "table"), path);
        } else if (!strcmp(func, "original")) {
            out = w22_paths_5ce33e_original(json_obj_get(in, "table"), path);
        } else if (!strcmp(func, "module")) {
            out = w22_paths_5ce33e_module(json_obj_get(in, "table"), name);
        } else if (!strcmp(func, "original_module")) {
            out = w22_paths_5ce33e_original_module(json_obj_get(in, "table"), name);
        } else if (!strcmp(func, "current_globs")) {
            out = w22_paths_5ce33e_current_globs(json_obj_get(in, "table"), pattern);
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
