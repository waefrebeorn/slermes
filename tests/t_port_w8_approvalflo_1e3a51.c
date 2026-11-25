#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_approvalflo_1e3a51.h"

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
        if (!strcmp(func, "user_deny")) {
            out = w8_approvalflo_1e3a51_user_deny_block_result(json_get_str(in, "pattern", ""));
        } else if (!strcmp(func, "sudo")) {
            out = w8_approvalflo_1e3a51_sudo_stdin_block_result(json_get_str(in, "description", ""));
        } else if (!strcmp(func, "hardline")) {
            out = w8_approvalflo_1e3a51_hardline_block_result(json_get_str(in, "description", ""),
                                               json_get_str(in, "command", ""));
        } else if (!strcmp(func, "has_operator")) {
            out = json_bool(w8_approvalflo_1e3a51_has_allowlist_shell_operator(json_get_str(in, "command", "")) ? true : false);
        } else if (!strcmp(func, "matching") || !strcmp(func, "matches")) {
            const char *cmd = json_get_str(in, "command", "");
            if (!strcmp(func, "matching")) {
                char *p = w8_approvalflo_1e3a51_matching_allowlist_ex(cmd, json_obj_get(in, "patterns"));
                out = p ? json_string(p) : json_null(); free(p);
            } else {
                out = json_bool(w8_approvalflo_1e3a51_command_matches_allowlist_ex(cmd, json_obj_get(in, "patterns")) ? true : false);
            }
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