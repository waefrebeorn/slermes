/* JSON-lines harness for the w21_apiservert_4998c3 gate.
 * One request object per stdin line; one response per line carrying "out" plus
 * every input key echoed back. Missing keys read as None and never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_apiservert_4998c3.h"
#include "gateway/platforms/api_server_turn_boundary.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[131072];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "same_prefix")) {
            out = w21_apiservert_4998c3_same_prefix(json_obj_get(in, "agent_messages"),
                                                   json_obj_get(in, "prefix"));
        } else if (!strcmp(func, "start_index")) {
            out = w21_apiservert_4998c3_start_index(json_obj_get(in, "conversation_history"),
                                                   json_obj_get(in, "user_message"),
                                                   json_obj_get(in, "result"),
                                                   json_obj_get(in, "live_views"));
        } else if (!strcmp(func, "reanchor")) {
            out = w21_apiservert_4998c3_reanchor(json_obj_get(in, "agent_messages"),
                                                json_obj_get(in, "user_message"),
                                                json_obj_get(in, "live_views"));
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
