#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w17_webserver_7f8b25.h"

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
        const char *ui_surface = json_get_str(in, "ui_surface", NULL);
        bool has_runner = json_get_bool(in, "gateway_runner", false);
        bool auth_required = json_get_bool(in, "auth_required", false);
        json_t *out = NULL;

        if (!strcmp(func, "surface")) {
            char *s = w17_webserver_7f8b25_surface(ui_surface, has_runner);
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "policy")) {
            out = w17_webserver_7f8b25_policy(ui_surface, has_runner);
        } else if (!strcmp(func, "private_launch")) {
            out = json_bool(w17_webserver_7f8b25_private_launch(ui_surface, has_runner, auth_required));
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
