/* t_port_w7_turntoolro_99622f.c — differential harness for agent/turn_tool_round.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_turntoolro_99622f.h"

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

        if (!strcmp(func, "tool_round_facts")) {
            const json_t *names = json_obj_get(in, "tool_names");
            const char *content = json_get_str(in, "content", "");
            bool hk = false, reply = false, drop = false;
            w7_turntoolro_tool_round_facts(names, content, &hk, &reply, &drop);
            out = json_object();
            json_set(out, "all_housekeeping", json_bool(hk));
            json_set(out, "line_is_reply", json_bool(reply));
            json_set(out, "drop_marker", json_bool(drop));
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
