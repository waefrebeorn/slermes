/*
 * t_port_w6_server_e95893.c — Oracle harness for acp_adapter/server.py.
 * Reads JSONL fixture lines, emits {"func":...,"out":...,<echoed input keys>}.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_server_e95893.h"

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
        if (!strcmp(func, "flatten")) {
            out = w6_server_e95893_flatten_history_text(json_obj_get(in, "value"));
        } else if (!strcmp(func, "reasoning")) {
            out = w6_server_e95893_history_reasoning_text(json_obj_get(in, "message"));
        } else if (!strcmp(func, "notice")) {
            out = w6_server_e95893_content_refused_notice(json_obj_get(in, "message"));
        } else if (!strcmp(func, "attach")) {
            out = w6_server_e95893_attach_interrupted_prompt(
                json_get_str(in, "interrupted", ""),
                json_get_str(in, "guidance", ""));
        } else if (!strcmp(func, "tool_call")) {
            out = w6_server_e95893_history_tool_call_name_args(json_obj_get(in, "tool_call"));
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
