/* JSON-lines harness for the w21_methodstoo_16216e gate
 * (tui_gateway/methods_tools_plugin_servers.py).
 *
 * funcs:
 *   identity  {"cfg": {...}|null, "env": {VAR: value}}
 *   portable  {"state": {...}}
 *   ownership {"state": {...}}
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_methodstoo_16216e.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "identity")) {
            out = w21_methodstoo_16216e_server_identity(json_obj_get(in, "cfg"),
                                                       json_obj_get(in, "env"));
        } else if (!strcmp(func, "portable")) {
            out = w21_methodstoo_16216e_portable_server_tools(json_obj_get(in, "state"));
        } else if (!strcmp(func, "ownership")) {
            out = w21_methodstoo_16216e_plugin_tool_ownership(json_obj_get(in, "state"));
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
