/* t_port_w16_vaultlogin_58f247.c — JSON-lines harness for vault_login_classifier.c */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_vaultlogin_58f247.h"

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
        if (!strcmp(func, "classify_login")) {
            out = w16_vaultlogin_58f247_classify_login(json_obj_get(in, "control"));
        } else if (!strcmp(func, "classify_checkout")) {
            out = w16_vaultlogin_58f247_classify_checkout(json_obj_get(in, "control"));
        } else if (!strcmp(func, "classify_otp")) {
            out = w16_vaultlogin_58f247_classify_otp(json_obj_get(in, "controls"));
        } else if (!strcmp(func, "select_password")) {
            out = w16_vaultlogin_58f247_select_password(json_obj_get(in, "controls"),
                                                        json_get_str(in, "password", ""));
        } else if (!strcmp(func, "select_checkout")) {
            out = w16_vaultlogin_58f247_select_checkout(json_obj_get(in, "controls"),
                    json_obj_get(in, "secret"), json_obj_get(in, "map"));
        } else if (!strcmp(func, "build_otp_fills")) {
            out = w16_vaultlogin_58f247_build_otp_fills(json_obj_get(in, "controls"),
                                                        json_get_str(in, "code", ""));
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
