/*
 * t_port_w7_fallbacknot_a6d13b.c — Oracle harness for agent/fallback_notice.py.
 * Reads JSONL fixture lines, emits {"func":...,"out":...,<echoed input keys>}.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_fallbacknot_a6d13b.h"

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
        json_t *out = NULL;
        const char *func;
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "constants")) {
            out = w7_fallbacknot_a6d13b_constants();
        } else if (!strcmp(func, "reason_text")) {
            out = w7_fallbacknot_a6d13b_reason_text(json_obj_get(in, "reason"));
        } else if (!strcmp(func, "trigger_text")) {
            out = w7_fallbacknot_a6d13b_trigger_text(json_obj_get(in, "reason"), json_obj_get(in, "stashed"));
        } else if (!strcmp(func, "notice_hidden")) {
            out = w7_fallbacknot_a6d13b_notice_hidden(json_obj_get(in, "display_kind"));
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
        json_free(out);
        json_free(in);
    }
    return 0;
}
