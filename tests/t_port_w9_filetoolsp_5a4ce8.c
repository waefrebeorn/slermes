#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_filetoolsp_5a4ce8.h"

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
        if (!strcmp(func, "normalize_posix")) {
            char *s = w9_filetoolsp_5a4ce8_normalize_posix(json_get_str(in, "path", ""));
            out = s ? json_string(s) : json_null(); free(s);
        } else if (!strcmp(func, "ssh_escapes_home")) {
            out = json_bool(w9_filetoolsp_5a4ce8_ssh_escapes_home(json_get_str(in, "path", "")) ? true : false);
        } else if (!strcmp(func, "sentinel")) {
            out = json_bool(w9_filetoolsp_5a4ce8_sentinel(json_get_str(in, "value", "")) ? true : false);
        } else if (!strcmp(func, "expand_tilde")) {
            char *s = w9_filetoolsp_5a4ce8_expand_tilde(json_get_str(in, "path", ""), json_get_str(in, "home", ""));
            out = s ? json_string(s) : json_null(); free(s);
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
