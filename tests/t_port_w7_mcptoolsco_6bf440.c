/*
 * t_port_w7_mcptoolsco_6bf440.c — Oracle harness for tools/mcp_tool_scope.py.
 * Reads JSONL fixture lines, emits {"func":...,"out":...,<echoed input keys>}.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_mcptoolsco_6bf440.h"

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
        json_t *out = NULL;
        const char *func;
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "key_name")) {
            out = w7_mcptoolsco_6bf440_key_name(json_obj_get(in, "key"));
        } else if (!strcmp(func, "key_scope")) {
            out = w7_mcptoolsco_6bf440_key_scope(json_obj_get(in, "key"));
        } else if (!strcmp(func, "key_visible")) {
            out = w7_mcptoolsco_6bf440_key_visible_in_scope(
                json_obj_get(in, "key"), json_get_str(in, "scope", NULL), json_obj_get(in, "adopted"));
        } else if (!strcmp(func, "server_key")) {
            out = w7_mcptoolsco_6bf440_server_key(json_get_str(in, "name", ""),
                json_obj_get(in, "scope"), json_get_bool(in, "current", true) ? 1 : 0,
                json_obj_get(in, "current_scope"));
        } else if (!strcmp(func, "resolve")) {
            out = w7_mcptoolsco_6bf440_resolve_server_key(json_get_str(in, "name", ""),
                json_obj_get(in, "scope"), json_get_bool(in, "current", true) ? 1 : 0,
                json_obj_get(in, "current_scope"), json_obj_get(in, "servers"),
                json_obj_get(in, "lazy"), json_obj_get(in, "shared"));
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
        json_free(out);
        json_free(in);
    }
    return 0;
}
