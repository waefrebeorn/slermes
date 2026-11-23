#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_agentinit_2749c4.h"
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
        if (!strcmp(func, "writer_interface")) {
            out = w10_agentinit_2749c4_writer_interface(json_obj_get(in, "persistence"),
                                                         json_obj_get(in, "store"));
        } else if (!strcmp(func, "agent_interface")) {
            out = w10_agentinit_2749c4_agent_interface(json_obj_get(in, "agent"));
        } else if (!strcmp(func, "stand_in_interface")) {
            out = w10_agentinit_2749c4_stand_in_interface(json_obj_get(in, "session_db"),
                     json_get_bool(in, "registered", false) ? 1 : 0,
                     json_get_bool(in, "is_session_db", false) ? 1 : 0);
        } else if (!strcmp(func, "publish_session_id")) {
            const char *r = w10_agentinit_2749c4_publish_session_id(
                json_get_str(in, "session_id", ""),
                json_get_bool(in, "bridge_ok", false) ? 1 : 0,
                json_get_bool(in, "delegated_child", false) ? 1 : 0);
            out = json_string(r ? r : "");
        }
        printf("{"); emit_key("func", json_string(func)); fputs(",", stdout); emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout); emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out); json_free(in);
    }
    return 0;
}
