#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_profilecha_e2c58e.h"

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
        const char *pid = json_get_str(in, "pid", "");
        json_t *out = NULL;
        if (!strcmp(func, "prefixes")) {
            out = w15_profilecha_e2c58e_platform_env_prefixes(pid);
        } else if (!strcmp(func, "env_key")) {
            out = w15_profilecha_e2c58e_env_key_of_line(json_get_str(in, "line", ""));
        } else if (!strcmp(func, "explicit_enabled")) {
            out = w15_profilecha_e2c58e_explicit_enabled(json_obj_get(in, "raw"), pid);
        } else if (!strcmp(func, "config_paths")) {
            out = w15_profilecha_e2c58e_channel_config_paths(json_obj_get(in, "raw"), json_obj_get(in, "platforms"));
        } else if (!strcmp(func, "warning")) {
            out = w15_profilecha_e2c58e_shared_credential_warning(json_get_str(in, "profile", ""),
                        json_obj_get(in, "platforms"), json_get_str(in, "source", "default"));
        } else if (!strcmp(func, "notice")) {
            out = w15_profilecha_e2c58e_format_stripped_notice(json_get_str(in, "profile", ""),
                        json_obj_get(in, "platforms"), json_get_str(in, "clone_flag", "--clone"));
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
