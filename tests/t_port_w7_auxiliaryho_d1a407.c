/*
 * t_port_w7_auxiliaryho_d1a407.c — Oracle harness for agent/auxiliary_hooks.py.
 * Reads JSONL fixture lines, emits {"func":...,"out":...,<echoed input keys>}.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_auxiliaryho_d1a407.h"

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
        json_t *out = NULL;
        const char *func;
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "constants")) {
            out = w7_auxiliaryho_d1a407_constants();
        } else if (!strcmp(func, "identity")) {
            out = w7_auxiliaryho_d1a407_parent_turn_identity(json_obj_get(in, "turn"));
        } else if (!strcmp(func, "system_prompt")) {
            out = w7_auxiliaryho_d1a407_system_prompt(json_obj_get(in, "messages"), json_obj_get(in, "kwargs"));
        } else if (!strcmp(func, "first_choice")) {
            out = w7_auxiliaryho_d1a407_first_choice_message(json_obj_get(in, "response"));
        } else if (!strcmp(func, "field")) {
            out = w7_auxiliaryho_d1a407_field(json_obj_get(in, "obj"), json_get_str(in, "name", ""));
        } else if (!strcmp(func, "message_count")) {
            out = w7_auxiliaryho_d1a407_message_count(json_obj_get(in, "messages"), json_obj_get(in, "input"));
        } else if (!strcmp(func, "retry_counts")) {
            out = w7_auxiliaryho_d1a407_retry_counts(json_obj_get(in, "raw"));
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        {
            size_t i;
            for (i = 0; i < json_object_size(in); i++) {
                const char *k = json_object_get_key_at(in, i);
                if (!k || !strcmp(k, "func")) continue;
                fputs(",", stdout);
                emit_key(k, json_copy(json_object_get_at(in, i)));
            }
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
