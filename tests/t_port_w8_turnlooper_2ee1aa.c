#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_turnlooper_2ee1aa.h"

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
        if (!strcmp(func, "handle")) {
            out = turnlooper_handle_outer_loop_error(
                (int)json_get_num(in, "outer_error_count", 0),
                (int)json_get_num(in, "api_call_count", 0),
                (int)json_get_num(in, "max_iterations", 0),
                (int)json_get_num(in, "max_outer_errors", 5),
                json_get_bool(in, "is_finalizing", false),
                json_get_bool(in, "shutdown_error", false),
                json_get_str(in, "e_text", ""),
                json_obj_get(in, "tb_files"),
                json_get_str(in, "session_id", NULL),
                json_get_bool(in, "suppress_status_output", false),
                json_obj_get(in, "messages"),
                json_get_str(in, "turn_exit_reason", NULL),
                json_get_bool(in, "failed", false),
                json_obj_get(in, "final_response"));
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
