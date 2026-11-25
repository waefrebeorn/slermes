/* t_port_w18_updatelaun_89b41e.c — harness for src/hermes_cli/update_launch_files.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_updatelaun_89b41e.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "owned")) {
            out = json_bool(w18_updatelaun_89b41e_owned(
                        json_get_str(in, "kind", ""),
                        json_get_str(in, "foreign", NULL)) ? true : false);
        } else if (!strcmp(func, "refreshers")) {
            out = w18_updatelaun_89b41e_refreshers(
                        json_get_str(in, "platform", ""),
                        json_get_bool(in, "systemd", false) ? 1 : 0);
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        json_t *jf = json_string(func);
        emit_key("func", jf);
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
        json_free(out); json_free(jf); json_free(in);
    }
    return 0;
}
