#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_auth_845ce1.h"

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
        if (!strcmp(func, "terminal_id")) {
            out = json_string(w5_auth_845ce1_terminal_setup_auth_method_id());
        } else if (!strcmp(func, "detect_provider")) {
            s = w5_auth_845ce1_detect_provider_ex(json_obj_get(in, "runtime"),
                    json_get_bool(in, "api_key_callable", false) ? 1 : 0,
                    json_get_bool(in, "raise", false) ? 1 : 0);
            if (s) { out = json_string(s); free(s); }
            else out = json_null();
        } else if (!strcmp(func, "build_auth_methods")) {
            const char *provider = json_has(in, "provider") ? json_get_str(in, "provider", "") : NULL;
            out = w5_auth_845ce1_build_auth_methods_ex(provider);
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
