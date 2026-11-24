/* JSON-lines harness for the w17_terminalap_428119 gate. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_terminalap_428119.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "runs")) {
            out = w17_terminalap_428119_terminal_approval_runs(json_obj_get(in, "calls"));
        } else if (!strcmp(func, "guard")) {
            out = w17_terminalap_428119_consume_prepared_guard(
                json_get_bool(in, "slot_exists", 0) ? 1 : 0,
                json_get_bool(in, "preparing", 0) ? 1 : 0,
                json_get_bool(in, "failure_seen", 0) ? 1 : 0,
                json_get_str(in, "ctx_call_id", ""),
                json_get_str(in, "expected_call_id", ""),
                json_get_str(in, "command", ""),
                json_get_str(in, "env_type", ""),
                json_get_bool(in, "has_host_access", 0) ? 1 : 0,
                json_get_str(in, "guard_command", ""),
                json_get_str(in, "guard_env_type", ""),
                json_get_bool(in, "guard_host_access", 0) ? 1 : 0,
                json_obj_get(in, "decision"),
                json_get_str(in, "approval_mode", ""));
        } else if (!strcmp(func, "preparing")) {
            int v = w17_terminalap_428119_preparing_terminal_approval(
                json_get_bool(in, "slot_exists", 0) ? 1 : 0,
                json_get_bool(in, "preparing", 0) ? 1 : 0);
            out = json_bool(v ? 1 : 0);
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
