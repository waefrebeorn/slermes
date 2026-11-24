/* JSON-lines harness for the w29_adapter_0e0bda_p3 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_adapter_0e0bda_p3.h"

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
        if (!strcmp(func, "method_lookup")) {
            out = w29_adapter_0e0bda_method_lookup(json_obj_get(in, "method"));
        } else if (!strcmp(func, "find_task_error")) {
            out = w29_adapter_0e0bda_find_task_error(json_obj_get(in, "req_id"), json_obj_get(in, "params"));
        } else if (!strcmp(func, "push_op_error")) {
            out = w29_adapter_0e0bda_push_op_error(json_obj_get(in, "req_id"), json_obj_get(in, "params"));
        } else if (!strcmp(func, "push_create_error")) {
            out = w29_adapter_0e0bda_push_create_error(json_obj_get(in, "req_id"), json_obj_get(in, "params"));
        } else if (!strcmp(func, "inline_push_url")) {
            out = w29_adapter_0e0bda_inline_push_url(json_obj_get(in, "params"));
        } else if (!strcmp(func, "note_forwarded")) {
            out = w29_adapter_0e0bda_note_forwarded(json_obj_get(in, "order"), json_obj_get(in, "input_id"),
                                                    json_obj_get(in, "max"));
        } else if (!strcmp(func, "forget_forwarded")) {
            out = w29_adapter_0e0bda_forget_forwarded(json_obj_get(in, "order"), json_obj_get(in, "input_id"));
        } else if (!strcmp(func, "chat_info")) {
            out = w29_adapter_0e0bda_chat_info(json_obj_get(in, "chat_id"));
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
