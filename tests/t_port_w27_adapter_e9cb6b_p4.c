/* JSON-lines harness for the w27_adapter_e9cb6b_p4 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w27_adapter_e9cb6b_p4.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "is_self_sender")) {
            out = w27_adapter_is_self_sender(json_obj_get(in, "user_id"),
                                             json_obj_get(in, "sender"));
        } else if (!strcmp(func, "is_system_or_bridge_sender")) {
            out = w27_adapter_is_system_or_bridge_sender(json_obj_get(in, "sender"));
        } else if (!strcmp(func, "classify_inbound_media")) {
            out = w27_adapter_classify_inbound_media(json_obj_get(in, "msgtype"),
                                                     json_obj_get(in, "mimetype"),
                                                     json_obj_get(in, "content"));
        } else if (!strcmp(func, "voice_may_park")) {
            out = w27_adapter_voice_may_park(in);
        } else if (!strcmp(func, "content_mentions_bot")) {
            out = w27_adapter_content_mentions_bot(json_obj_get(in, "user_id"),
                                                   json_obj_get(in, "body"),
                                                   json_obj_get(in, "content"));
        } else if (!strcmp(func, "extract_invite_dm_signal")) {
            out = w27_adapter_extract_invite_dm_signal(json_obj_get(in, "user_id"),
                                                       json_obj_get(in, "invited_room"));
        } else if (!strcmp(func, "matrix_prompt_expired")) {
            out = w27_adapter_matrix_prompt_expired(json_obj_get(in, "expires_at"),
                                                    json_obj_get(in, "now"));
        } else if (!strcmp(func, "is_authorized_user")) {
            out = w27_adapter_is_authorized_user(json_obj_get(in, "user_id"),
                                                 json_obj_get(in, "allow_all"),
                                                 json_obj_get(in, "allowed_users"));
        } else if (!strcmp(func, "reactor_allowed")) {
            out = w27_adapter_reactor_allowed(json_obj_get(in, "sender"),
                                              json_obj_get(in, "requester"),
                                              json_obj_get(in, "require_sender"));
        } else if (!strcmp(func, "state_event_value")) {
            out = w27_adapter_state_event_value(json_obj_get(in, "event"),
                                                json_obj_get(in, "key"));
        } else if (!strcmp(func, "room_server_name")) {
            out = w27_adapter_room_server_name(json_obj_get(in, "room_id"));
        } else if (!strcmp(func, "room_identity")) {
            out = w27_adapter_room_identity(in);
        } else if (!strcmp(func, "presence_valid")) {
            out = w27_adapter_presence_valid(json_obj_get(in, "state"));
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
