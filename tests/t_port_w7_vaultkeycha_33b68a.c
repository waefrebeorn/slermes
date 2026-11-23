/* Port gate harness for agent/vault_keychain.py (gate w7_vaultkeycha_33b68a). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_vaultkeycha_33b68a.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "is_os_keychain")) {
            out = json_bool(vaultkeychain_is_os_keychain(json_obj_get(in, "backend")) ? 1 : 0);
        } else if (!strcmp(func, "backend_label")) {
            out = vaultkeychain_backend_label(in);
        } else if (!strcmp(func, "new_ref")) {
            out = vaultkeychain_new_ref(json_get_str(in, "vault_dir", ""),
                                        json_get_str(in, "account_hex", ""));
        } else if (!strcmp(func, "owns")) {
            out = json_bool(vaultkeychain_owns(json_obj_get(in, "ref"),
                                               json_get_str(in, "vault_dir", "")) ? 1 : 0);
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
