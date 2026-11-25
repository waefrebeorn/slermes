/* JSON-lines harness for the w16_termination_ef34ac gate. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_termination_ef34ac.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "exit_status")) {
            out = json_number((double)w16_termination_ef34ac_exit_status(
                (int)json_get_num(in, "signum", 0.0)));
        } else if (!strcmp(func, "signals")) {
            out = w16_termination_ef34ac_termination_signals(json_obj_get(in, "available"));
        } else if (!strcmp(func, "guard")) {
            out = w16_termination_ef34ac_guard_plan(
                json_get_bool(in, "is_main", 0) ? 1 : 0,
                json_obj_get(in, "signals"),
                json_obj_get(in, "prev_kinds"));
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
