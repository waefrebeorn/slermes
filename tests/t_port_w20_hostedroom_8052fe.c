/* JSON-lines harness for the w20_hostedroom_8052fe gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_hostedroom_8052fe.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "member_roster")) {
            out = w20_hostedroom_8052fe_member_roster(json_obj_get(in, "room"),
                                                      json_obj_get(in, "homes"));
        } else if (!strcmp(func, "recent_messages")) {
            out = w20_hostedroom_8052fe_recent_messages(json_obj_get(in, "events"),
                                                        json_obj_get(in, "members"),
                                                        json_obj_get(in, "before_seq"));
        } else if (!strcmp(func, "start")) {
            out = w20_hostedroom_8052fe_start(json_get_str(in, "room_id", ""),
                                              json_get_str(in, "discussion_event_id", ""),
                                              json_get_bool(in, "already_inflight", false) ? 1 : 0,
                                              json_obj_get(in, "claim"),
                                              json_obj_get(in, "call_result"),
                                              json_get_bool(in, "record_raises", false) ? 1 : 0,
                                              json_get_bool(in, "release_raises", false) ? 1 : 0,
                                              json_obj_get(in, "now"));
        } else if (!strcmp(func, "handles")) {
            out = w20_hostedroom_8052fe_handles(json_obj_get(in, "members"));
        } else if (!strcmp(func, "claim_ttl")) {
            out = w20_hostedroom_8052fe_claim_ttl();
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
