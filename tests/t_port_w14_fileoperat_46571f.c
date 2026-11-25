#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_fileoperat_46571f.h"

static void emit_key(const char *key, struct json_t *v) {
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
        struct json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        struct json_t *out = NULL;
        if (!strcmp(func, "looks_like_linter_unusable")) {
            out = json_bool(w14_fileoperat_46571f_looks_like_linter_unusable(
                      json_get_str(in, "base_cmd", ""),
                      json_get_str(in, "output", "")) ? true : false);
        } else if (!strcmp(func, "check_lint_delta")) {
            const struct json_t *pre = json_obj_get(in, "pre");
            out = w14_fileoperat_46571f_check_lint_delta(
                      pre, json_obj_get(in, "post"), pre ? 1 : 0);
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
