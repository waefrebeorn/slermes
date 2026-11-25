/*
 * t_port_w7_init_ec1211.c — Oracle harness for tui_gateway/__init__.py.
 * Reads JSONL fixture lines, emits {"func":...,"out":...,<echoed input keys>}.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_init_ec1211.h"

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
        json_t *out = NULL;
        const char *func;
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "public_names")) {
            out = w7_init_ec1211_public_names();
        } else if (!strcmp(func, "has_attr")) {
            out = w7_init_ec1211_has_attr(json_get_str(in, "name", ""));
        } else if (!strcmp(func, "module_doc")) {
            out = w7_init_ec1211_module_doc();
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        {
            size_t i;
            for (i = 0; i < json_object_size(in); i++) {
                const char *k = json_object_get_key_at(in, i);
                if (!k || !strcmp(k, "func")) continue;
                fputs(",", stdout);
                emit_key(k, json_copy(json_object_get_at(in, i)));
            }
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
