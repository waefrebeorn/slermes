#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_sessions_9adadc.h"

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
        if (!strcmp(func, "spec")) {
            out = w19_sessions_9adadc_spec();
        } else if (!strcmp(func, "parent_args")) {
            out = w19_sessions_9adadc_parent_args();
        } else if (!strcmp(func, "sub_names")) {
            out = w19_sessions_9adadc_sub_names();
        } else if (!strcmp(func, "args")) {
            out = w19_sessions_9adadc_args(json_get_str(in, "sub", ""));
        } else if (!strcmp(func, "arg")) {
            out = w19_sessions_9adadc_arg(json_get_str(in, "sub", ""),
                                          json_get_str(in, "flag", ""));
        } else if (!strcmp(func, "arg_count")) {
            out = w19_sessions_9adadc_arg_count(json_get_str(in, "sub", ""));
        } else if (!strcmp(func, "dest")) {
            out = json_string(w19_sessions_9adadc_dest());
        } else if (!strcmp(func, "has_sub")) {
            out = json_bool(w19_sessions_9adadc_has_sub(json_get_str(in, "sub", "")) != 0);
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
