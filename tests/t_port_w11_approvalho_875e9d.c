#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_approvalho_875e9d.h"

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
        int win32 = json_get_bool(in, "win32", false) ? 1 : 0;
        json_t *out = NULL;
        if (!strcmp(func, "msys_drive_spellings")) {
            out = w11_approvalho_875e9d_msys_drive_spellings(json_get_str(in, "path", ""));
        } else if (!strcmp(func, "path_below")) {
            out = w11_approvalho_875e9d_path_below(json_get_str(in, "root", ""),
                        json_get_str(in, "path", ""), win32);
        } else if (!strcmp(func, "home_spellings")) {
            out = w11_approvalho_875e9d_home_spellings(json_obj_get(in, "paths"),
                    json_get_str(in, "command", ""), json_get_str(in, "home_env", ""),
                    json_get_str(in, "expand_home", ""), json_get_str(in, "local_appdata", ""),
                    json_get_str(in, "appdata", ""), json_get_str(in, "userprofile", ""), win32);
        } else if (!strcmp(func, "fold_home_prefixes")) {
            out = w11_approvalho_875e9d_fold_home_prefixes(json_get_str(in, "command", ""),
                    json_obj_get(in, "paths"), json_get_str(in, "replacement", ""), win32);
        } else if (!strcmp(func, "rewrite_user_home_ex")) {
            out = w11_approvalho_875e9d_rewrite_user_home_ex(json_get_str(in, "command", ""),
                    json_get_str(in, "expand_home", ""), json_get_str(in, "real_home", ""),
                    json_get_str(in, "home_env", ""), win32);
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
