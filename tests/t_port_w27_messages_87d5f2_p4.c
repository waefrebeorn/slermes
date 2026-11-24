/* JSON-lines harness for gate w27_messages_87d5f2_p4.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_messages_87d5f2_p4.h"

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
        if (!strcmp(func, "canonical_replayed_user_content")) {
            out = msg_canonical_replayed_user_content(json_obj_get(in, "msg"));
        } else if (!strcmp(func, "exact_replayed_user_clone_key")) {
            out = msg_exact_replayed_user_clone_key(json_obj_get(in, "timestamp"),
                                                    json_obj_get(in, "content"));
        } else if (!strcmp(func, "find_duplicate_replayed_user_message")) {
            out = msg_find_duplicate_replayed_user_message(json_obj_get(in, "messages"),
                                                           json_obj_get(in, "msg"));
        } else if (!strcmp(func, "is_explicit_fork_child_row")) {
            out = msg_is_explicit_fork_child_row(json_obj_get(in, "session"),
                                                 json_obj_get(in, "include_reset"));
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
