/* t_port_w13_secretsco_6e907b.c — oracle harness for
 * hermes_cli/_secrets_common.py (gate w13_secretsco_6e907b). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_secretsco_6e907b.h"

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
        char *s = NULL;

        if (!strcmp(func, "console_safe")) {
            s = w13_secretsco_6e907b_console_safe(json_get_str(in, "text", ""));
            out = json_string(s ? s : "");
        } else if (!strcmp(func, "yn")) {
            out = json_string(w13_secretsco_6e907b_yn(json_get_bool(in, "b", false)));
        } else if (!strcmp(func, "section_cfg")) {
            out = w13_secretsco_6e907b_section_cfg(json_obj_get(in, "cfg"),
                                                   json_get_str(in, "section", ""));
        } else if (!strcmp(func, "cfg_str")) {
            s = w13_secretsco_6e907b_cfg_str(json_obj_get(in, "cfg"),
                                             json_get_str(in, "key", ""));
            out = json_string(s ? s : "");
        } else if (!strcmp(func, "arg")) {
            out = w13_secretsco_6e907b_arg(json_get_str(in, "name", ""),
                                           json_get_str(in, "help", ""),
                                           json_obj_get(in, "kwargs"));
        } else if (!strcmp(func, "flag")) {
            out = w13_secretsco_6e907b_flag(json_get_str(in, "name", ""),
                                            json_get_str(in, "help", ""));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        free(s);

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
