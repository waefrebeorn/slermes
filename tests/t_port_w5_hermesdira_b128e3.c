#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_hermesdira_b128e3.h"

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
        const char *path = json_get_str(in, "path", "");
        int windows = json_get_bool(in, "windows", 0) ? 1 : 0;
        json_t *out = NULL;

        if (!strcmp(func, "strip")) {
            char *s = hermesdira_strip_win32_prefix(path);
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "remedy")) {
            char *s = hermesdira_unusable_dir_remedy(path, windows);
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "constants")) {
            out = hermesdira_constants();
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
