#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_libatomic_4bd942.h"

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
        if (!strcmp(func, "manager_order")) {
            out = w5_libatomic_4bd942_manager_order(json_obj_get(in, "tokens"));
        } else if (!strcmp(func, "host_install_command")) {
            out = w5_libatomic_4bd942_host_install_command(json_obj_get(in, "tokens"),
                                                           json_obj_get(in, "available"));
        } else if (!strcmp(func, "command_plan")) {
            out = w5_libatomic_4bd942_command_plan(json_obj_get(in, "command"),
                        json_get_bool(in, "is_root", false) ? 1 : 0,
                        json_get_str(in, "sudo_path", ""));
        } else if (!strcmp(func, "shlex_join")) {
            out = w5_libatomic_4bd942_shlex_join(json_obj_get(in, "tokens"));
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
