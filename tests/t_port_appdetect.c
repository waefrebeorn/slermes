#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_appdetect.h"

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
        json_t *out = NULL;

        if (!strcmp(func, "lower")) {
            char *s = appdetect_lower_preserving_flags(json_get_str(in, "command", ""));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "splice")) {
            char *s = appdetect_splice(json_get_str(in, "command", ""),
                                       json_obj_get(in, "edits"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "eval")) {
            char *s = appdetect_eval_payload(json_obj_get(in, "args"));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "family")) {
            const char *s = appdetect_interpreter_family(json_get_str(in, "executable", ""));
            out = s ? json_string(s) : json_null();
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
