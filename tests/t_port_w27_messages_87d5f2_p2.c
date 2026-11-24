/* JSON-lines harness for gate w27_messages_87d5f2_p2.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_messages_87d5f2_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    static char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "row_identity")) {
            out = msg_row_identity(json_obj_get(in, "role"), json_obj_get(in, "content"),
                                   json_obj_get(in, "tool_call_id"), json_obj_get(in, "tool_calls"));
        } else if (!strcmp(func, "loaded_view_content")) {
            out = msg_loaded_view_content(json_obj_get(in, "role"), json_obj_get(in, "content"));
        } else if (!strcmp(func, "stamp_kept_live_prefix")) {
            out = msg_stamp_kept_live_prefix(json_obj_get(in, "live"), json_obj_get(in, "messages"));
        } else if (!strcmp(func, "clone_message_rows")) {
            out = msg_clone_message_rows(json_obj_get(in, "columns"), json_obj_get(in, "tail_ids"),
                                         json_obj_get(in, "session_id"));
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
