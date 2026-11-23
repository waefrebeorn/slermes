/* t_port_w13_mainagent_07d4b1.c — Oracle harness for hermes_cli/main_agent_cmds.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_mainagent_07d4b1.h"
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
        if (!strcmp(func, "dict_or_empty")) {
            out = w13_mainagent_07d4b1_dict_or_empty(json_obj_get(in, "value"));
        } else if (!strcmp(func, "acp_flags")) {
            out = w13_mainagent_07d4b1_acp_flags(json_obj_get(in, "args"));
        } else if (!strcmp(func, "acp_env")) {
            out = w13_mainagent_07d4b1_acp_env(json_obj_get(in, "args"));
        } else if (!strcmp(func, "monitoring_lines")) {
            out = w13_mainagent_07d4b1_monitoring_lines(json_obj_get(in, "mon"),
                    json_get_bool(in, "otlp_available", 0));
        } else {
            out = json_string("UNKNOWN_FUNC");
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
