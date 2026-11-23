/* t_port_w11_agentinit_f7c2eb.c - harness for agent/agent_init_fallback.py port. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_agentinit_f7c2eb.h"

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
        char *text = NULL;

        if (!strcmp(func, "entries")) {
            out = w11_agentinit_f7c2eb_fallback_entries(json_obj_get(in, "fallback_model"));
        } else if (!strcmp(func, "labels")) {
            out = w11_agentinit_f7c2eb_labels(json_obj_get(in, "chain"));
        } else if (!strcmp(func, "message")) {
            text = w11_agentinit_f7c2eb_message(json_obj_get(in, "chain"));
            out = json_string(text ? text : "");
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        free(text);

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
