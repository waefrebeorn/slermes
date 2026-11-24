/* JSON-lines harness for gate w27_messages_87d5f2_p3.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_messages_87d5f2_p3.h"

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
        if (!strcmp(func, "is_model_only_row")) {
            out = msg_is_model_only_row(json_obj_get(in, "row"));
        } else if (!strcmp(func, "fold_summary_carrier")) {
            out = msg_fold_summary_carrier(json_obj_get(in, "row"), json_obj_get(in, "key"),
                                           json_obj_get(in, "first_by_uid"));
        } else if (!strcmp(func, "row_to_message_dict")) {
            out = msg_row_to_message_dict(json_obj_get(in, "row"), json_obj_get(in, "summary_flag"));
        } else if (!strcmp(func, "display_rows_from_conn")) {
            out = msg_display_rows_from_conn(json_obj_get(in, "session_id"), json_obj_get(in, "limit"),
                                             json_obj_get(in, "offset"), json_obj_get(in, "latest"),
                                             json_obj_get(in, "after"));
        } else if (!strcmp(func, "active_clause")) {
            out = msg_active_clause(json_obj_get(in, "include_inactive"),
                                    json_obj_get(in, "include_compacted"));
        } else if (!strcmp(func, "fetch_conversation_rows")) {
            out = msg_fetch_conversation_rows(json_obj_get(in, "session_ids"),
                                              json_obj_get(in, "active_clause"),
                                              json_obj_get(in, "with_session_id"));
        } else if (!strcmp(func, "get_messages")) {
            out = msg_get_messages(json_obj_get(in, "session_id"), json_obj_get(in, "include_inactive"),
                                   json_obj_get(in, "include_compacted"), json_obj_get(in, "limit"),
                                   json_obj_get(in, "offset"), json_obj_get(in, "latest"),
                                   json_obj_get(in, "after_id"), json_obj_get(in, "include_ancestors"),
                                   json_obj_get(in, "rows"));
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
