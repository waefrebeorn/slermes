/* JSON-lines harness for the w26_processreg_5c33c6 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_processreg_5c33c6.h"

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
        if (!strcmp(func, "output_dir")) {
            out = w26_processreg_5c33c6_output_dir(json_obj_get(in, "hermes_home"));
        } else if (!strcmp(func, "adoptable_log")) {
            out = w26_processreg_5c33c6_adoptable_log(json_obj_get(in, "path"),
                                                      json_obj_get(in, "output_dir"));
        } else if (!strcmp(func, "exit_path")) {
            out = w26_processreg_5c33c6_exit_path(json_obj_get(in, "log"));
        } else if (!strcmp(func, "record_exit_command")) {
            out = w26_processreg_5c33c6_record_exit_command(json_obj_get(in, "log"),
                                                            json_obj_get(in, "command"));
        } else if (!strcmp(func, "constants")) {
            out = w26_processreg_5c33c6_constants();
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
