/* JSON-lines harness for the w29_hostedroom_8927a5_p3 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_hostedroom_8927a5_p3.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "task_view")) {
            out = w29_hostedroom_8927a5_p3_task_view(json_obj_get(in, "row"),
                                                     json_get_bool(in, "idempotent", false));
        } else if (!strcmp(func, "settlement_params")) {
            out = w29_hostedroom_8927a5_p3_settlement_params(json_get_str(in, "settlement_id", ""),
                                                             json_get_str(in, "status", ""),
                                                             json_obj_get(in, "result"),
                                                             json_get_num(in, "now", 0));
        } else if (!strcmp(func, "settlement_replay")) {
            out = w29_hostedroom_8927a5_p3_settlement_replay(json_obj_get(in, "row"),
                                                             json_get_str(in, "settlement_id", ""),
                                                             json_get_str(in, "status", ""),
                                                             json_obj_get(in, "result"),
                                                             json_get_num(in, "now", 0));
        } else if (!strcmp(func, "cancel_replay")) {
            out = w29_hostedroom_8927a5_p3_cancel_replay(json_obj_get(in, "row"),
                                                         json_get_str(in, "cancel_id", ""),
                                                         json_get_str(in, "status", "cancelled"));
        } else if (!strcmp(func, "defer_result")) {
            out = w29_hostedroom_8927a5_p3_defer_result(json_get_str(in, "reason", ""));
        } else if (!strcmp(func, "admit")) {
            out = w29_hostedroom_8927a5_p3_admit(in);
        } else if (!strcmp(func, "start")) {
            out = w29_hostedroom_8927a5_p3_start(in);
        } else if (!strcmp(func, "cancel")) {
            out = w29_hostedroom_8927a5_p3_cancel(in);
        } else if (!strcmp(func, "settle")) {
            out = w29_hostedroom_8927a5_p3_settle(in);
        } else if (!strcmp(func, "generation")) {
            out = w29_hostedroom_8927a5_p3_generation(in);
        } else if (!strcmp(func, "prune")) {
            out = w29_hostedroom_8927a5_p3_prune(in);
        } else if (!strcmp(func, "settle_stopping")) {
            out = w29_hostedroom_8927a5_p3_settle_stopping(in);
        } else if (!strcmp(func, "begin_cancel")) {
            out = w29_hostedroom_8927a5_p3_begin_cancel(in);
        } else if (!strcmp(func, "complete_cancel")) {
            out = w29_hostedroom_8927a5_p3_complete_cancel(in);
        } else if (!strcmp(func, "requeue_not_admitted")) {
            out = w29_hostedroom_8927a5_p3_requeue_not_admitted(in);
        } else if (!strcmp(func, "mark_indeterminate")) {
            out = w29_hostedroom_8927a5_p3_mark_indeterminate(in);
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
