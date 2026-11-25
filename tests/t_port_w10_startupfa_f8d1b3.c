#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_startupfa_f8d1b3.h"

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
        if (!strcmp(func, "is_global_fast_version_argv"))
            out = w10_startupfa_f8d1b3_is_global_fast_version_argv(json_obj_get(in, "argv"));
        else if (!strcmp(func, "is_desktop_ssh_backend_argv"))
            out = w10_startupfa_f8d1b3_is_desktop_ssh_backend_argv(json_obj_get(in, "argv"));
        else if (!strcmp(func, "active_profile_may_override_home"))
            out = w10_startupfa_f8d1b3_active_profile_may_override_home(json_get_str(in, "active_text", NULL));
        else if (!strcmp(func, "normalize_hermes_home_env"))
            out = w10_startupfa_f8d1b3_normalize_hermes_home_env(json_obj_get(in, "raw"),
                                                                 json_obj_get(in, "env"));
        else if (!strcmp(func, "read_install_method"))
            out = w10_startupfa_f8d1b3_read_install_method(json_get_str(in, "text", NULL));
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
