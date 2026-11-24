/* JSON-lines harness for the w22_finder_213faa gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_finder_213faa.h"
#include "hermes_agent/compat/finder.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* A string answer as JSON: "" stays "", NULL becomes null. */
static json_t *opt_str(char *s) {
    json_t *v;
    if (!s) return NULL;
    v = json_string(s);
    return v;
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "current_name")) {
            char *s = w22_finder_213faa_current_name(json_get_str(in, "name", ""),
                                                     json_obj_get(in, "moves"));
            out = opt_str(s);
            free(s);
        } else if (!strcmp(func, "child_names")) {
            out = w22_finder_213faa_child_names(json_obj_get(in, "moves"));
        } else if (!strcmp(func, "strict_error")) {
            char *s = w22_finder_213faa_strict_error(json_get_str(in, "name", ""),
                                                     json_obj_get(in, "moves"));
            out = opt_str(s);
            free(s);
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
