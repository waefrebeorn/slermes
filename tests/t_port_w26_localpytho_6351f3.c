/* JSON-lines harness for the w26_localpytho_6351f3 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_localpytho_6351f3.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *opt_str(const json_t *in, const char *key, const char *def) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_STRING) ? v->str_val : def;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "same_path")) {
            out = w26_localpytho_6351f3_same_path(opt_str(in, "left", NULL),
                                                  opt_str(in, "right", NULL));
        } else if (!strcmp(func, "strip_pythonpath")) {
            out = w26_localpytho_6351f3_strip_pythonpath(json_obj_get(in, "env"),
                                                         json_obj_get(in, "owned_paths"),
                                                         opt_str(in, "sep", ";"));
        } else if (!strcmp(func, "strip_all")) {
            out = w26_localpytho_6351f3_strip_all(json_obj_get(in, "env"),
                                                  json_obj_get(in, "owned_paths"),
                                                  opt_str(in, "sep", ";"));
        } else if (!strcmp(func, "constants")) {
            out = w26_localpytho_6351f3_constants();
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
