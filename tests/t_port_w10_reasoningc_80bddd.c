/* t_port_w10_reasoningc_80bddd.c — fixture-driven differential harness.
 * Reads one JSON case per stdin line, dispatches by "func", emits
 * {"func":..., "out":..., ...all input keys echoed...}. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_reasoningc_80bddd.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "thought_signature"))
            out = w10_reasoningc_80bddd_thought_signature(in);
        else if (!strcmp(func, "carrier_record"))
            out = w10_reasoningc_80bddd_carrier_record(in);
        else if (!strcmp(func, "strip_function_signature"))
            out = w10_reasoningc_80bddd_strip_function_signature(in);
        else if (!strcmp(func, "replay_plan"))
            out = w10_reasoningc_80bddd_replay_plan(in);
        else if (!strcmp(func, "shape"))
            out = w10_reasoningc_80bddd_shape(in);
        if (!out) out = json_null();
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func") || !strcmp(k, "out")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
