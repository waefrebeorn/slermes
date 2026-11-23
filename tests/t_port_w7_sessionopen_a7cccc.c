#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_sessionopen_a7cccc.h"

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
        if (!strcmp(func, "reason")) {
            out = w7_sessionopen_a7cccc_reason(
                json_get_str(in, "session_id", ""),
                json_get_bool(in, "receipt_ok", false) ? 1 : 0,
                json_get_str(in, "receipt_target", ""),
                json_get_bool(in, "row_exists", false) ? 1 : 0,
                json_get_str(in, "message_count", "0"));
        } else if (!strcmp(func, "gate_fire")) {
            out = json_bool(w7_sessionopen_a7cccc_gate_fire_opens(
                json_get_bool(in, "policy_present", false) ? 1 : 0,
                json_get_bool(in, "ignore_user_config", false) ? 1 : 0) ? true : false);
        } else if (!strcmp(func, "settled_extra")) {
            out = json_number(w7_sessionopen_a7cccc_settled_extra_wait(
                json_get_num(in, "callback_timeout", 0)));
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
