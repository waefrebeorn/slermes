#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_environments_407599.h"

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
        char *s = NULL;
        if (!strcmp(func, "venv_bin_dir")) {
            s = w5_environments_407599_venv_bin_dir(json_get_str(in, "venv", ""),
                                                     (int)json_get_num(in, "windows", -1));
        } else if (!strcmp(func, "venv_python")) {
            s = w5_environments_407599_venv_python(json_get_str(in, "venv", ""),
                                                    (int)json_get_num(in, "windows", -1));
        } else if (!strcmp(func, "fish_quote")) {
            s = w5_environments_407599_fish_quote(json_get_str(in, "value", ""));
        } else if (!strcmp(func, "shell_exports")) {
            s = w5_environments_407599_shell_exports(json_obj_get(in, "env"),
                                                     json_get_str(in, "dialect", "sh"));
        } else if (!strcmp(func, "install_key")) {
            s = w5_environments_407599_install_key_ex(json_get_str(in, "canonical", ""));
        }
        if (s) { out = json_string(s); free(s); }
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
