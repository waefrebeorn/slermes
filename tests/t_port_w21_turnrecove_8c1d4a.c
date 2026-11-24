/* JSON-lines harness for the w21_turnrecove_8c1d4a gate.
 * One request object per stdin line; one response per line carrying "out" plus
 * every input key echoed back. Missing keys read as None and never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_turnrecove_8c1d4a.h"
#include "agent/turn_recovery_autorecover.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[131072];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "cycles")) {
            out = w21_turnrecove_8c1d4a_cycles(json_obj_get(in, "agent"));
        } else if (!strcmp(func, "wait")) {
            out = w21_turnrecove_8c1d4a_wait_seconds((int)json_get_num(in, "cycle", 0),
                                                    json_obj_get(in, "api_error"));
        } else if (!strcmp(func, "eligible")) {
            out = w21_turnrecove_8c1d4a_eligible(json_obj_get(in, "agent"),
                                                json_get_str(in, "reason", ""),
                                                json_get_str(in, "streamed", ""),
                                                json_get_bool(in, "has_content", 0) ? 1 : 0);
        } else if (!strcmp(func, "notice")) {
            out = w21_turnrecove_8c1d4a_notice(json_get_str(in, "platform", ""),
                                              json_get_num(in, "wait_s", 0.0),
                                              (int)json_get_num(in, "cycle", 0),
                                              (int)json_get_num(in, "total", 0));
        } else if (!strcmp(func, "recover")) {
            out = w21_turnrecove_8c1d4a_recover(json_obj_get(in, "case_data"));
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
