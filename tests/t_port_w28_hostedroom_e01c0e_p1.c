/* JSON-lines harness for the w28_hostedroom_e01c0e_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_hostedroom_e01c0e_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); } else fputs("null", stdout);
    json_free(v);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "task_member_id")) {
            out = hrs_task_member_id(json_obj_get(in, "task"));
        } else if (!strcmp(func, "member_of")) {
            out = hrs_member_of(json_obj_get(in, "task"));
        } else if (!strcmp(func, "attempt_action")) {
            out = hrs_attempt_action(json_get_str(in, "kind", ""), json_obj_get(in, "task"));
        } else if (!strcmp(func, "approvable_attempts")) {
            out = hrs_approvable_attempts(json_obj_get(in, "tasks"));
        } else if (!strcmp(func, "attempt_actions")) {
            out = hrs_attempt_actions(json_obj_get(in, "tasks"));
        } else if (!strcmp(func, "member_is_peer")) {
            out = hrs_member_is_peer(json_obj_get(in, "room"), json_get_str(in, "member_id", ""));
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
        json_free(in);
    }
    return 0;
}
