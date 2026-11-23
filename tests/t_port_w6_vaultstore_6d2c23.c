/*
 * t_port_w6_vaultstore_6d2c23.c — Oracle harness for agent/vault_store.py.
 * Reads JSONL fixture lines, emits {"func":...,"out":...,<echoed input keys>}.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_vaultstore_6d2c23.h"

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
        json_t *out = NULL;
        char *s = NULL;
        if (!strcmp(func, "vault_kinds")) {
            out = w6_vaultstore_6d2c23_vault_kinds();
        } else if (!strcmp(func, "login_identifier_types")) {
            out = w6_vaultstore_6d2c23_login_identifier_types();
        } else if (!strcmp(func, "fields")) {
            out = w6_vaultstore_6d2c23_fields(json_get_str(in, "kind", NULL));
        } else if (!strcmp(func, "required_fields")) {
            out = w6_vaultstore_6d2c23_required_fields(json_get_str(in, "kind", NULL));
        } else if (!strcmp(func, "normalize_origin")) {
            s = w6_vaultstore_6d2c23_normalize_origin(json_get_str(in, "value", NULL));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "normalize_otp")) {
            s = w6_vaultstore_6d2c23_normalize_otp_secret(json_get_str(in, "value", NULL));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "scrub")) {
            s = w6_vaultstore_6d2c23_scrub_secret_from_text(
                json_get_str(in, "text", ""), json_obj_get(in, "secret"));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "meta")) {
            out = w6_vaultstore_6d2c23_meta_dict(json_obj_get(in, "rec"));
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
