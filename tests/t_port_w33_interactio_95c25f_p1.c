/* JSON-lines harness for the w33_interactio_95c25f_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w33_interactio_95c25f_p1.h"

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
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "input_steps")) {
            out = w33_interactio_95c25f_p1_input_steps(json_obj_get(in, "messages"));
        } else if (!strcmp(func, "request")) {
            out = w33_interactio_95c25f_p1_request(in);
        } else if (!strcmp(func, "continuation")) {
            out = w33_interactio_95c25f_p1_continuation(in);
        } else if (!strcmp(func, "tools")) {
            out = w33_interactio_95c25f_p1_tools(json_obj_get(in, "tools"));
        } else if (!strcmp(func, "tool_choice")) {
            out = w33_interactio_95c25f_p1_tool_choice(json_obj_get(in, "tool_choice"));
        } else if (!strcmp(func, "response_format")) {
            out = w33_interactio_95c25f_p1_response_format(json_obj_get(in, "response_format"));
        } else if (!strcmp(func, "image")) {
            out = w33_interactio_95c25f_p1_image(json_obj_get(in, "item"));
        } else if (!strcmp(func, "content_blocks")) {
            out = w33_interactio_95c25f_p1_content_blocks(json_obj_get(in, "content"));
        } else if (!strcmp(func, "function_result")) {
            out = w33_interactio_95c25f_p1_function_result(in);
        } else if (!strcmp(func, "call_arguments")) {
            out = w33_interactio_95c25f_p1_call_arguments(json_obj_get(in, "tool_call"));
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
