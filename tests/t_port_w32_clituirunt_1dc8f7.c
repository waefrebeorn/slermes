/* JSON-lines harness for the w32_clituirunt_1dc8f7 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w32_clituirunt_1dc8f7.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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
        if (!strcmp(func, "closing_session")) {
            out = w32_clituirunt_1dc8f7_closing_session(in);
        } else if (!strcmp(func, "hold_step")) {
            out = w32_clituirunt_1dc8f7_hold_step(in);
        } else if (!strcmp(func, "build_application")) {
            out = w32_clituirunt_1dc8f7_build_application(in);
        } else if (!strcmp(func, "signal_plan")) {
            out = w32_clituirunt_1dc8f7_signal_plan(in);
        } else if (!strcmp(func, "slash_input")) {
            out = w32_clituirunt_1dc8f7_slash_input(in);
        } else if (!strcmp(func, "after_turn")) {
            out = w32_clituirunt_1dc8f7_after_turn(in);
        } else if (!strcmp(func, "shutdown_plan")) {
            out = w32_clituirunt_1dc8f7_shutdown_plan(in);
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
