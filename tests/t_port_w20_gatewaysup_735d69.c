/* JSON-lines harness for the w20_gatewaysup_735d69 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_gatewaysup_735d69.h"

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
        if (!strcmp(func, "declares_external_supervisor")) {
            out = json_bool(w20_gatewaysup_735d69_declares_external_supervisor(
                json_obj_get(in, "pid"), json_obj_get(in, "identity"),
                json_obj_get(in, "argv"), json_obj_get(in, "record")) ? 1 : 0);
        } else if (!strcmp(func, "replacement_pid")) {
            out = w20_gatewaysup_735d69_replacement_pid(
                json_obj_get(in, "old_pid"), json_obj_get(in, "observed"),
                json_obj_get(in, "timeout"), json_obj_get(in, "poll_interval"));
        } else if (!strcmp(func, "start_mode")) {
            out = w20_gatewaysup_735d69_start_mode(
                json_obj_get(in, "args"), json_get_bool(in, "force", 0),
                json_get_str(in, "home", ""), (int)json_get_num(in, "detach_code", 0.0));
        } else if (!strcmp(func, "verify_timeout")) {
            out = json_number(w20_gatewaysup_735d69_verify_timeout());
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
