#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_webappsign_185f5a.h"

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
        if (!strcmp(func, "binding")) {
            char *v = w12_webappsign_185f5a_binding(json_get_str(in, "token", ""));
            out = json_string(v ? v : "");
            free(v);
        } else if (!strcmp(func, "digest")) {
            char *v = w12_webappsign_185f5a_digest(json_get_str(in, "token", ""));
            out = json_string(v ? v : "");
            free(v);
        } else if (!strcmp(func, "applies")) {
            const char *surface = json_obj_get(in, "ui_surface")
                ? json_get_str(in, "ui_surface", "") : NULL;
            out = json_bool(w12_webappsign_185f5a_applies(
                surface, json_get_bool(in, "auth_required", 0)));
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
