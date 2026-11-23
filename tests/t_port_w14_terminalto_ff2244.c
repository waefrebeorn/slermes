#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_terminalto_ff2244.h"

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
        if (!strcmp(func, "signal_exit")) {
            out = w14_terminalto_ff2244_interpret_signal_exit(json_obj_get(in, "exit_code"));
        } else if (!strcmp(func, "exit_code")) {
            out = w14_terminalto_ff2244_interpret_exit_code(json_obj_get(in, "command"),
                        json_obj_get(in, "exit_code"));
        } else if (!strcmp(func, "cwd_fallback")) {
            out = w14_terminalto_ff2244_cwd_fallback_note(json_obj_get(in, "fallback"));
        } else if (!strcmp(func, "env_recreated")) {
            out = w14_terminalto_ff2244_env_recreated_note();
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
