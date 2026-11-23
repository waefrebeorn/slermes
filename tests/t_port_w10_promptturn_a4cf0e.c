#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_promptturn_a4cf0e.h"
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
        if (!strcmp(func, "result_status")) {
            out = json_string(w10_promptturn_a4cf0e_result_status(json_obj_get(in, "result")));
        } else if (!strcmp(func, "is_successful_goal_turn")) {
            int v = w10_promptturn_a4cf0e_is_successful_goal_turn(json_obj_get(in, "result"),
                      json_get_str(in, "status", ""), json_obj_get(in, "raw"),
                      json_get_bool(in, "handoff", false) ? 1 : 0);
            out = json_bool(v ? true : false);
        } else if (!strcmp(func, "turn_outcome")) {
            out = w10_promptturn_a4cf0e_turn_outcome(json_obj_get(in, "result"),
                    json_obj_get(in, "error_surface"), json_get_str(in, "error_text", ""),
                    json_get_str(in, "interrupt_prefix", ""));
        }
        printf("{"); emit_key("func", json_string(func)); fputs(",", stdout); emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout); emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out); json_free(in);
    }
    return 0;
}
