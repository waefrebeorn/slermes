/* JSON-lines harness for the w27_apiservero_1a4ac2_p3 gate (api_server_openai_routes
 * wire parsing + Responses envelope).  One request object per stdin line; one
 * response line carrying the computed "out" plus every input key echoed back. */
#include "hermes_json.h"
#include "noc_w27_apiservero_1a4ac2_p3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static double numarg(const json_t *in, const char *key, double dflt) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type != JSON_NUMBER) return dflt;
    return v->num_val;
}

static int flagarg(const json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    return v && v->type == JSON_BOOL && v->bool_val;
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
        if (!strcmp(func, "parse_chat_messages")) {
            out = w27_apiservero_1a4ac2_p3_parse_chat_messages(json_obj_get(in, "messages"));
        } else if (!strcmp(func, "parse_responses_input")) {
            out = w27_apiservero_1a4ac2_p3_parse_responses_input(json_obj_get(in, "input"));
        } else if (!strcmp(func, "parse_conversation_history")) {
            out = w27_apiservero_1a4ac2_p3_parse_conversation_history(
                json_obj_get(in, "conversation_history"));
        } else if (!strcmp(func, "stream_envelope")) {
            out = w27_apiservero_1a4ac2_p3_stream_envelope(
                json_get_str(in, "response_id", ""), numarg(in, "created_at", 0),
                json_get_str(in, "model", ""), json_get_str(in, "status", ""));
        } else if (!strcmp(func, "terminal_envelope")) {
            out = w27_apiservero_1a4ac2_p3_terminal_envelope(
                json_get_str(in, "response_id", ""), numarg(in, "created_at", 0),
                json_get_str(in, "model", ""), json_get_str(in, "status", ""),
                json_obj_get(in, "output"), json_obj_get(in, "error"),
                json_obj_get(in, "usage"), flagarg(in, "durable_key"));
        } else if (!strcmp(func, "history_with_user")) {
            out = w27_apiservero_1a4ac2_p3_history_with_user(
                json_obj_get(in, "history"), json_obj_get(in, "user_message"));
        } else if (!strcmp(func, "final_items")) {
            out = w27_apiservero_1a4ac2_p3_final_items(
                json_obj_get(in, "emitted_items"), json_obj_get(in, "final_response_text"),
                json_obj_get(in, "agent_error"));
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
