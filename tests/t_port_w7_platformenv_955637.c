#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_platformenv_955637.h"

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
        if (!strcmp(func, "csv_list")) {
            out = w7_platformenv_955637_csv_list(json_get_str(in, "value", ""));
        } else if (!strcmp(func, "strip_slash")) {
            out = w7_platformenv_955637_strip_slash(json_get_str(in, "value", ""));
        } else if (!strcmp(func, "strip_lower")) {
            out = w7_platformenv_955637_strip_lower(json_get_str(in, "value", ""));
        } else if (!strcmp(func, "truthy")) {
            out = w7_platformenv_955637_truthy_token(json_get_str(in, "value", ""));
        } else if (!strcmp(func, "int_or")) {
            out = w7_platformenv_955637_int_or(json_get_str(in, "value", ""), json_get_str(in, "default", "0"));
        } else if (!strcmp(func, "mention")) {
            out = w7_platformenv_955637_mention_patterns(json_get_str(in, "value", ""));
        } else if (!strcmp(func, "env_first")) {
            out = w7_platformenv_955637_env_first(json_obj_get(in, "names"), json_obj_get(in, "values"));
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
