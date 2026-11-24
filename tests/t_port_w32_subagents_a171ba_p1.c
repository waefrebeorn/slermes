/* JSON-lines harness for the w32_subagents_a171ba_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w32_subagents_a171ba_p1.h"

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
        if (!strcmp(func, "record_shape")) {
            out = w32_subagents_a171ba_p1_record_shape(json_obj_get(in, "row"));
        } else if (!strcmp(func, "route_shape")) {
            out = w32_subagents_a171ba_p1_route_shape(in);
        } else if (!strcmp(func, "upsert")) {
            out = w32_subagents_a171ba_p1_upsert(in);
        } else if (!strcmp(func, "apply_limits")) {
            out = w32_subagents_a171ba_p1_apply_limits(in);
        } else if (!strcmp(func, "insert_completion")) {
            out = w32_subagents_a171ba_p1_insert_completion(in);
        } else if (!strcmp(func, "recover_guard")) {
            out = w32_subagents_a171ba_p1_recover_guard(in);
        } else if (!strcmp(func, "constants")) {
            out = w32_subagents_a171ba_p1_constants();
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
