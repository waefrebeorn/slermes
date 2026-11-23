#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_init_51a22b.h"

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
        if (!strcmp(func, "doc")) {
            out = json_string(w7_init_51a22b_doc());
        } else if (!strcmp(func, "release_date")) {
            out = json_string(w7_init_51a22b_release_date());
        } else if (!strcmp(func, "getattr")) {
            out = w7_init_51a22b_getattr(json_get_str(in, "name", ""), json_obj_get(in, "base_version"));
        } else if (!strcmp(func, "public_names")) {
            out = w7_init_51a22b_public_names();
        } else if (!strcmp(func, "has_symbol")) {
            out = json_bool(w7_init_51a22b_has_symbol(json_get_str(in, "name", "")) != 0);
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
