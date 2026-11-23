/* t_port_w8_reasoningpa_062303.c — harness for agent/reasoning_params.py port. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_reasoningpa_062303.h"

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
        if (!strcmp(func, "should_sanitize")) {
            out = json_bool(w8_reasoningpa_062303_should_sanitize_tool_calls(
                json_obj_get(in, "api_mode")) ? true : false);
        } else if (!strcmp(func, "echo_opt_in")) {
            out = json_bool(w8_reasoningpa_062303_reasoning_echo_opt_in(
                json_obj_get(in, "flag")) ? true : false);
        } else if (!strcmp(func, "kimi")) {
            out = json_bool(w8_reasoningpa_062303_needs_kimi_tool_reasoning(json_obj_get(in, "args")) ? true : false);
        } else if (!strcmp(func, "deepseek")) {
            out = json_bool(w8_reasoningpa_062303_needs_deepseek_tool_reasoning(json_obj_get(in, "args")) ? true : false);
        } else if (!strcmp(func, "mimo")) {
            out = json_bool(w8_reasoningpa_062303_needs_mimo_tool_reasoning(json_obj_get(in, "args")) ? true : false);
        } else if (!strcmp(func, "sanitize")) {
            out = w8_reasoningpa_062303_sanitize_tool_calls_for_strict_api(
                json_obj_get(in, "api_msg"),
                json_get_bool(in, "keep_signature", false) ? 1 : 0);
        } else if (!strcmp(func, "supports")) {
            out = json_bool(w8_reasoningpa_062303_supports_reasoning_extra_body(json_obj_get(in, "args")) ? true : false);
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
