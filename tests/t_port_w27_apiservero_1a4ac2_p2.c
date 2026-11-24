/* JSON-lines harness for the w27_apiservero_1a4ac2_p2 gate (api_server_openai_routes
 * text shaping / transcript hygiene).  One request object per stdin line; one
 * response line carrying the computed "out" plus every input key echoed back. */
#include "hermes_json.h"
#include "noc_w27_apiservero_1a4ac2_p2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static long numarg(const json_t *in, const char *key, long dflt) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type != JSON_NUMBER) return dflt;
    return (long)v->num_val;
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
        if (!strcmp(func, "cap_text")) {
            out = w27_apiservero_1a4ac2_p2_cap_text(json_get_str(in, "text", ""),
                                                    numarg(in, "keep", 0));
        } else if (!strcmp(func, "cap_history_tool_outputs")) {
            out = w27_apiservero_1a4ac2_p2_cap_history_tool_outputs(
                json_obj_get(in, "history"), numarg(in, "max_chars", 0));
        } else if (!strcmp(func, "trim_tool_items")) {
            out = w27_apiservero_1a4ac2_p2_trim_tool_items(json_obj_get(in, "items"));
        } else if (!strcmp(func, "message_item")) {
            out = w27_apiservero_1a4ac2_p2_message_item(json_obj_get(in, "text"));
        } else if (!strcmp(func, "post_stream_transform")) {
            out = w27_apiservero_1a4ac2_p2_post_stream_transform(json_obj_get(in, "result"));
        } else if (!strcmp(func, "is_reasoning_input_item")) {
            out = w27_apiservero_1a4ac2_p2_is_reasoning_input_item(json_obj_get(in, "item"));
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
        if (out) json_free(out);
        json_free(in);
    }
    return 0;
}
