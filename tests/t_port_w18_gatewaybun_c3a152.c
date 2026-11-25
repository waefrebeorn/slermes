#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w18_gatewaybun_c3a152.h"

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
        if (!strcmp(func, "env_pairs")) {
            out = w18_gatewaybun_c3a152_env_pairs(json_obj_get(in, "prior"),
                                                   json_obj_get(in, "env"));
        } else if (!strcmp(func, "systemd_env_line")) {
            out = w18_gatewaybun_c3a152_systemd_env_line(json_get_str(in, "name", ""),
                                                          json_get_str(in, "value", ""));
        } else if (!strcmp(func, "systemd_bundled_env_block")) {
            out = w18_gatewaybun_c3a152_systemd_bundled_env_block(
                json_obj_get(in, "prior"), json_obj_get(in, "env"));
        } else if (!strcmp(func, "launchd_bundled_env_xml")) {
            out = w18_gatewaybun_c3a152_launchd_bundled_env_xml(
                json_obj_get(in, "prior"), json_obj_get(in, "env"));
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
