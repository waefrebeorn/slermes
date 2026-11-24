/* t_port_w16_credential_33d75e.c — JSON-lines harness for credential_pool_plugin.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_credential_33d75e.h"

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
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "is_terminal_plugin_refresh_error")) {
            out = w16_credential_33d75e_is_terminal_plugin_refresh_error(json_obj_get(in, "exc"));
        } else if (!strcmp(func, "plugin_row_is_expiring")) {
            out = w16_credential_33d75e_plugin_row_is_expiring(json_get_str(in, "expires_at_ms", ""),
                                                              json_get_str(in, "now_ms", "0"));
        } else if (!strcmp(func, "apply_plugin_refresh_result")) {
            out = w16_credential_33d75e_apply_plugin_refresh_result(json_obj_get(in, "entry"),
                                                                   json_obj_get(in, "result"));
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
        json_free(out);
        json_free(in);
    }
    return 0;
}
