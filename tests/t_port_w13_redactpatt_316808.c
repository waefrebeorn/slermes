#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_redactpatt_316808.h"

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
        if (!strcmp(func, "skip_class")) {
            out = json_number((double)w13_redactpatt_316808_skip_char_class(
                      json_get_str(in, "pattern", ""), (int)json_get_num(in, "i", 0)));
        } else if (!strcmp(func, "unbounded")) {
            out = json_bool(w13_redactpatt_316808_unbounded_quantifier_follows(
                      json_get_str(in, "pattern", ""), (int)json_get_num(in, "j", 0)) ? true : false);
        } else if (!strcmp(func, "structure")) {
            out = w13_redactpatt_316808_pattern_structure(json_get_str(in, "pattern", ""));
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
