/* t_port_w7_commands_7ff5d7.c -- JSON-lines harness for src/acp_adapter/commands.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_commands_7ff5d7.h"

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
        char *tmp = NULL;
        if (!strcmp(func, "commands_table")) {
            out = w7_commands_7ff5d7_commands_table();
        } else if (!strcmp(func, "mid_turn_blocked")) {
            out = w7_commands_7ff5d7_mid_turn_blocked();
        } else if (!strcmp(func, "parse_slash")) {
            out = w7_commands_7ff5d7_parse_slash(json_get_str(in, "text", ""));
        } else if (!strcmp(func, "help_text")) {
            tmp = w7_commands_7ff5d7_help_text();
            out = json_string(tmp ? tmp : "");
        } else if (!strcmp(func, "model_status")) {
            tmp = w7_commands_7ff5d7_model_status(
                json_get_str(in, "model", ""), json_get_str(in, "fallback", ""),
                json_get_str(in, "provider", ""));
            out = json_string(tmp ? tmp : "");
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
        free(tmp);
        json_free(out);
        json_free(in);
    }
    return 0;
}
