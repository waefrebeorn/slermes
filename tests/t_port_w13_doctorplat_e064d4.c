#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_doctorplat_e064d4.h"

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
        int nix = json_get_bool(in, "nix", false) ? 1 : 0;
        if (!strcmp(func, "install_cmd")) {
            out = w13_doctorplat_e064d4_system_package_install_cmd(
                json_get_str(in, "pkg", ""), json_get_str(in, "platform", ""),
                json_get_bool(in, "termux", false) ? 1 : 0);
        } else if (!strcmp(func, "repair_hint")) {
            out = w13_doctorplat_e064d4_python_repair_hint(
                json_get_str(in, "method", ""), json_get_str(in, "cmd", ""), nix);
        } else if (!strcmp(func, "sqlite_hint")) {
            out = w13_doctorplat_e064d4_sqlite_upgrade_hint(
                json_get_str(in, "method", ""), json_get_str(in, "cmd", ""), nix);
        } else if (!strcmp(func, "tcc")) {
            out = w13_doctorplat_e064d4_tcc_grant_check(json_get_str(in, "dr", ""));
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
