/* t_port_w11_terminalto_3baf4c.c - harness for tools/terminal_tool_sudo.py port. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_terminalto_3baf4c.h"

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
        const char *command = json_get_str(in, "command", "");
        json_t *out = NULL;
        if (!strcmp(func, "env")) {
            out = w11_terminalto_3baf4c_looks_like_env_assignment(json_get_str(in, "token", ""));
        } else if (!strcmp(func, "token")) {
            out = w11_terminalto_3baf4c_read_shell_token(command, (int)json_get_num(in, "start", 0));
        } else if (!strcmp(func, "scan")) {
            out = w11_terminalto_3baf4c_scan_shell(command,
                    json_get_bool(in, "background", false) ? 1 : 0);
        } else if (!strcmp(func, "rewrite")) {
            out = w11_terminalto_3baf4c_rewrite_sudo(command,
                    json_get_bool(in, "askpass", false) ? 1 : 0);
        } else if (!strcmp(func, "count")) {
            out = w11_terminalto_3baf4c_count_sudo(command);
        } else if (!strcmp(func, "bg")) {
            out = w11_terminalto_3baf4c_rewrite_compound_background(command);
        } else if (!strcmp(func, "wrongpw")) {
            out = w11_terminalto_3baf4c_sudo_wrong_password(json_get_str(in, "output", ""));
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
