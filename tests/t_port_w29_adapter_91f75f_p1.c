/* JSON-lines harness for the w29_adapter_91f75f_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_adapter_91f75f_p1.h"

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
        if (!strcmp(func, "constants")) {
            out = w29_adapter_91f75f_constants();
        } else if (!strcmp(func, "csv_set")) {
            out = w29_adapter_91f75f_csv_set(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "normalize_markdown")) {
            out = w29_adapter_91f75f_normalize_markdown(json_obj_get(in, "text"));
        } else if (!strcmp(func, "require_mention")) {
            out = w29_adapter_91f75f_require_mention(json_obj_get(in, "configured"));
        } else if (!strcmp(func, "is_user_allowed")) {
            out = w29_adapter_91f75f_is_user_allowed(json_obj_get(in, "allowed_users"),
                                                     json_obj_get(in, "sender_id"),
                                                     json_obj_get(in, "sender_staff_id"));
        } else if (!strcmp(func, "should_process_message")) {
            out = w29_adapter_91f75f_should_process_message(json_obj_get(in, "message"));
        } else if (!strcmp(func, "is_sdk_incompat")) {
            out = w29_adapter_91f75f_is_sdk_incompat(json_obj_get(in, "chain"));
        } else if (!strcmp(func, "chat_info")) {
            out = w29_adapter_91f75f_chat_info(json_obj_get(in, "chat_id"));
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
