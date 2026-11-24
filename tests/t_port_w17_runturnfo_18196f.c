/* t_port_w17_runturnfo_18196f.c — JSON-lines harness for the faithful C11 port
 * of gateway/run_turn_followup_ack.py (gate w17_runturnfo_18196f).
 *
 * Self-contained: #includes the port .c (bare name, resolved via -I src).
 */
#include "hermes_json.h"
#include "gateway/run_turn_followup_ack.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        json_t *io = json_obj_get(in, "io");

        if (!strcmp(func, "apply")) {
            out = json_bool(w17_runturnfo_18196f_followup_processing_hooks_apply(
                    json_obj_get(in, "adapter"), json_obj_get(in, "event")));
        } else if (!strcmp(func, "cancel")) {
            const char *r = w17_runturnfo_18196f_followup_cancel_outcome(
                    json_obj_get(in, "adapter"), io);
            out = json_string(r ? r : "FAILURE");
        } else if (!strcmp(func, "run_hook")) {
            out = w17_runturnfo_18196f_run_followup_processing_hook(
                    json_obj_get(in, "adapter"), json_obj_get(in, "event"),
                    json_get_str(in, "hook_name", ""), json_obj_get(in, "args"), io);
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
