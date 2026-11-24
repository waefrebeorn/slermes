/* JSON-lines harness for gate w27_messages_87d5f2_p1.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_messages_87d5f2_p1.h"

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
        if (!strcmp(func, "encode_display_metadata")) {
            out = msg_encode_display_metadata(json_obj_get(in, "display_metadata"));
        } else if (!strcmp(func, "decode_display_metadata")) {
            out = msg_decode_display_metadata(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "reasoning_json_text")) {
            out = msg_reasoning_json_text(json_obj_get(in, "value"));
        } else if (!strcmp(func, "decoded_repair_row")) {
            out = msg_decoded_repair_row(json_obj_get(in, "row"));
        } else if (!strcmp(func, "bump_session_counters")) {
            out = msg_bump_session_counters(json_obj_get(in, "session_id"), json_obj_get(in, "inserted"),
                                            json_obj_get(in, "tool_calls"), json_obj_get(in, "unit"));
        } else if (!strcmp(func, "reaction_list")) {
            out = msg_reaction_list(json_obj_get(in, "meta"));
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
