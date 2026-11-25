/*
 * t_port_w6_sleepclock_919d0b.c — Oracle harness for agent/sleep_clock.py.
 * Reads JSONL fixture lines, emits {"func":...,"out":...,<echoed input keys>}.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_sleepclock_919d0b.h"

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
        if (!strcmp(func, "threshold")) {
            out = json_number(w6_sleepclock_919d0b_sleep_threshold());
        } else if (!strcmp(func, "slept")) {
            out = json_number(w6_sleepclock_919d0b_slept(
                json_get_num(in, "prev_wall", 0.0),
                json_get_num(in, "wall", 0.0),
                json_get_num(in, "interval", 0.0)));
        } else if (!strcmp(func, "sequence")) {
            out = w6_sleepclock_919d0b_slept_sequence(
                json_get_num(in, "interval", 0.0),
                json_get_num(in, "initial_wall", 0.0),
                json_obj_get(in, "walls"));
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
