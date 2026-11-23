/*
 * t_port_w6_turnfacade_bf7ace.c — Oracle harness for agent/turn_facade.py.
 * Reads JSONL fixture lines, emits {"func":...,"out":...,<echoed input keys>}.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_turnfacade_bf7ace.h"

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
        char *s = NULL;
        if (!strcmp(func, "chat")) {
            out = w6_turnfacade_bf7ace_chat(json_obj_get(in, "result"));
        } else if (!strcmp(func, "outcome")) {
            out = json_string(w6_turnfacade_bf7ace_outcome(json_obj_get(in, "result")));
        } else if (!strcmp(func, "exception_outcome")) {
            out = json_string(w6_turnfacade_bf7ace_exception_outcome(
                json_get_str(in, "raise", NULL)));
        } else if (!strcmp(func, "task_context")) {
            out = w6_turnfacade_bf7ace_task_context(
                json_get_str(in, "session_id", NULL),
                json_get_str(in, "task_id", NULL),
                json_get_str(in, "platform", NULL));
        } else if (!strcmp(func, "relay_turn_id")) {
            s = w6_turnfacade_bf7ace_relay_turn_id(
                json_get_str(in, "session_id", NULL),
                json_get_str(in, "task_id", NULL),
                json_get_str(in, "uuid_hex", NULL));
            out = s ? json_string(s) : json_null();
            free(s);
        } else if (!strcmp(func, "relay_parent")) {
            s = w6_turnfacade_bf7ace_relay_parent(
                json_get_str(in, "parent_session_id", NULL),
                json_get_str(in, "platform", NULL));
            out = s ? json_string(s) : json_null();
            free(s);
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
