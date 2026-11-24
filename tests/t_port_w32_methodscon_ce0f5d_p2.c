/* JSON-lines harness for the w32_methodscon_ce0f5d_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w32_methodscon_ce0f5d_p2.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "err")) {
            out = w32_methodscon_ce0f5d_p2_err(json_obj_get(in, "rid"), json_obj_get(in, "code"),
                                               json_obj_get(in, "message"));
        } else if (!strcmp(func, "ok")) {
            out = w32_methodscon_ce0f5d_p2_ok(json_obj_get(in, "rid"), json_obj_get(in, "result"));
        } else if (!strcmp(func, "kv")) {
            out = w32_methodscon_ce0f5d_p2_kv(json_obj_get(in, "rid"), json_obj_get(in, "key"),
                                              json_obj_get(in, "value"), json_obj_get(in, "extra"));
        } else if (!strcmp(func, "model_ok")) {
            out = w32_methodscon_ce0f5d_p2_model_ok(json_obj_get(in, "rid"), json_obj_get(in, "key"),
                                                    json_obj_get(in, "value"), json_obj_get(in, "warning"),
                                                    json_obj_get(in, "confirm_message"),
                                                    json_obj_get(in, "scope"), json_obj_get(in, "extra"));
        } else if (!strcmp(func, "confirm_required")) {
            out = w32_methodscon_ce0f5d_p2_confirm_required(json_obj_get(in, "confirm_message"));
        } else if (!strcmp(func, "reasoning")) {
            out = w32_methodscon_ce0f5d_p2_reasoning(json_obj_get(in, "rid"), json_obj_get(in, "value"),
                                                     json_obj_get(in, "scope"));
        } else if (!strcmp(func, "toggle_set")) {
            out = w32_methodscon_ce0f5d_p2_toggle_set(json_obj_get(in, "rid"), json_obj_get(in, "key"),
                                                      json_obj_get(in, "value"), json_obj_get(in, "display"));
        } else if (!strcmp(func, "bool_set")) {
            out = w32_methodscon_ce0f5d_p2_bool_set(json_obj_get(in, "rid"), json_obj_get(in, "key"),
                                                    json_obj_get(in, "value"), json_obj_get(in, "cfg_key"));
        } else if (!strcmp(func, "prompt_set")) {
            out = w32_methodscon_ce0f5d_p2_prompt_set(json_obj_get(in, "rid"), json_obj_get(in, "value"));
        } else if (!strcmp(func, "is_stale_session")) {
            out = w32_methodscon_ce0f5d_p2_is_stale_session(json_obj_get(in, "key"),
                                                            json_obj_get(in, "session_id_present"),
                                                            json_obj_get(in, "session_exists"),
                                                            json_obj_get(in, "scope"));
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
