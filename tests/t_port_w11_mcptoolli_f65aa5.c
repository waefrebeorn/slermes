#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_mcptoolli_f65aa5.h"

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
        if (!strcmp(func, "filter_children")) {
            out = w11_mcptoolli_f65aa5_filter_children(json_obj_get(in, "pids"),
                    json_obj_get(in, "cmdlines"),
                    json_get_bool(in, "psutil_available", true) ? 1 : 0,
                    json_get_str(in, "supervisor_script", ""));
        } else if (!strcmp(func, "take_reapable")) {
            out = w11_mcptoolli_f65aa5_take_reapable(
                    json_get_bool(in, "include_active", false) ? 1 : 0,
                    json_obj_get(in, "server_name"),
                    json_obj_get(in, "orphan_pids"), json_obj_get(in, "orphan_servers"),
                    json_obj_get(in, "stdio_pids"), json_obj_get(in, "stdio_pgids"),
                    json_obj_get(in, "stdio_starttimes"));
        } else if (!strcmp(func, "clear_cooldowns")) {
            out = w11_mcptoolli_f65aa5_clear_cooldowns(json_obj_get(in, "keys"),
                    json_obj_get(in, "retry_after"), json_obj_get(in, "failures"));
        } else if (!strcmp(func, "stop_loop_if_idle")) {
            out = json_bool(w11_mcptoolli_f65aa5_stop_loop_if_idle(
                    json_get_bool(in, "loop_result", false) ? 1 : 0) ? true : false);
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
