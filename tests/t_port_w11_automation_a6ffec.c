#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_automation_a6ffec.h"

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
        if (!strcmp(func, "descriptor")) {
            out = w11_automation_a6ffec_descriptor(json_obj_get(in, "payload"));
        } else if (!strcmp(func, "folded_payload")) {
            out = w11_automation_a6ffec_folded_payload(json_obj_get(in, "payload"));
        } else if (!strcmp(func, "goal_contract")) {
            out = w11_automation_a6ffec_goal_contract(json_obj_get(in, "data"));
        } else if (!strcmp(func, "goal_contract_empty")) {
            out = json_bool(w11_automation_a6ffec_goal_contract_empty(json_obj_get(in, "data")) ? true : false);
        } else if (!strcmp(func, "goal_gate")) {
            out = w11_automation_a6ffec_goal_gate(json_obj_get(in, "data"));
        } else if (!strcmp(func, "heartbeat_state")) {
            json_t *d = json_obj_get(in, "data");
            char *raw = d ? json_dumps(d, 0) : NULL;
            out = w11_automation_a6ffec_heartbeat_state(raw ? raw : "{}");
            free(raw);
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
