/* Port gate harness for gateway/run_idle_gates.py (gate w8_runidlegat_fc52a8). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_runidlegat_fc52a8.h"

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
        if (!strcmp(func, "gate")) {
            out = runidlegat_gate(json_obj_get(in, "facts"));
        } else if (!strcmp(func, "heartbeat")) {
            out = runidlegat_profile_has_active_heartbeat(json_obj_get(in, "facts"));
        } else if (!strcmp(func, "loop")) {
            out = runidlegat_profile_has_active_loop(json_obj_get(in, "facts"));
        } else if (!strcmp(func, "handoff")) {
            out = runidlegat_profile_has_pending_handoff(json_obj_get(in, "facts"));
        } else if (!strcmp(func, "off_loop")) {
            out = runidlegat_off_loop_gate(
                json_get_bool(in, "has_offload", false) ? 1 : 0,
                json_get_bool(in, "offload_result", false) ? 1 : 0);
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
