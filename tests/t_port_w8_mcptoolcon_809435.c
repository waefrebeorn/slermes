#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_mcptoolcon_809435.h"

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
        json_t *out = NULL;
        if (!strcmp(func, "pathext_suffixes")) {
            out = w8_mcptoolcon_809435_pathext_suffixes(
                json_obj_get(in, "env"), json_get_bool(in, "windows", false) ? 1 : 0);
        } else if (!strcmp(func, "warn_hidden_whitespace")) {
            out = w8_mcptoolcon_809435_warn_hidden_whitespace(
                json_get_str(in, "name", ""), json_obj_get(in, "config"));
        } else if (!strcmp(func, "require_rendered_remote")) {
            out = w8_mcptoolcon_809435_require_rendered_remote(
                json_get_str(in, "name", ""), json_obj_get(in, "cfg"));
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
