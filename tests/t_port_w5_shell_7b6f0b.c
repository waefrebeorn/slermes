#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_shell_7b6f0b.h"

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

        if (!strcmp(func, "candidates")) {
            json_t *envj = json_obj_get(in, "env");
            size_t ne = json_object_size(envj);
            w5_shell_7b6f0b_env_t *env = NULL;
            if (ne) env = (w5_shell_7b6f0b_env_t *)calloc(ne, sizeof(*env));
            for (size_t i = 0; i < ne; i++) {
                json_t *v = json_object_get_at(envj, i);
                env[i].key = json_object_get_key_at(envj, i);
                env[i].value = (v && v->type == JSON_STRING) ? v->str_val : "";
            }
            size_t n = 0;
            char **arr = w5_shell_7b6f0b_windows_bash_candidates(
                json_get_str(in, "on_path", ""), env, ne, &n);
            out = json_array();
            for (size_t i = 0; i < n; i++) json_append(out, json_string(arr[i]));
            w5_shell_7b6f0b_strings_free(arr, n);
            free(env);
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
