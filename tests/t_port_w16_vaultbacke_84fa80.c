/* t_port_w16_vaultbacke_84fa80.c — JSON-lines harness for vault_backend_registry.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_vaultbacke_84fa80.h"

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
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "namespace_error")) {
            out = w16_vaultbacke_84fa80_namespace_error(json_get_str(in, "name", ""),
                    json_obj_get(in, "builtin_names"), json_obj_get(in, "builtin_prefixes"));
        } else if (!strcmp(func, "static_name")) {
            out = w16_vaultbacke_84fa80_static_name(json_obj_get(in, "provider"));
        } else if (!strcmp(func, "enabled_plugin_backends")) {
            out = w16_vaultbacke_84fa80_enabled_plugin_backends(json_obj_get(in, "registry"),
                    json_obj_get(in, "cfg"), json_obj_get(in, "builtin_names"),
                    json_obj_get(in, "builtin_prefixes"));
        } else {
            out = json_null();
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); ++i) {
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
