/* t_port_apppipe.c — oracle harness for tools/approval_pipe_launchers.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_apppipe.h"

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
        char *r = NULL;
        json_t *out = NULL;

        if (!strcmp(func, "launcher_re")) {
            const char *name = json_get_str(in, "name", "");
            const char *short_opts = json_get_str(in, "short", "");
            const char *ws = json_get_str(in, "ws", NULL); /* NULL -> default \s */
            const char *lo[64]; size_t nlo = 0;
            json_t *arr = json_obj_get(in, "long_opts");
            if (arr && arr->type == JSON_ARRAY)
                for (size_t i = 0; i < arr->c.count && nlo < 64; i++) {
                    json_t *it = arr->c.items[i];
                    if (it && it->type == JSON_STRING) lo[nlo++] = it->str_val;
                }
            r = apppipe_pipe_launcher_re(name, short_opts, lo, nlo, ws);
            out = json_string(r ? r : "");
        } else if (!strcmp(func, "path_prefix")) {
            out = json_string(apppipe_path_prefix_re());
        } else if (!strcmp(func, "wrappers")) {
            r = apppipe_wrappers_re();
            out = json_string(r ? r : "");
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        free(r);

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
