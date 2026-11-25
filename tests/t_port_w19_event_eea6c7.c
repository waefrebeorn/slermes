/* t_port_w19_event_eea6c7.c - harness for src/gateway/platforms/event.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_event_eea6c7.h"

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
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        const char *text = json_get_str(in, "text", NULL);
        int allow = json_get_bool(in, "allow_gateway_control", true);
        json_t *out = NULL;
        if (!strcmp(func, "clean_filename")) {
            out = w19_event_eea6c7_clean_attachment_filename(json_obj_get(in, "name"));
        } else if (!strcmp(func, "command_text")) {
            out = w19_event_eea6c7_command_text(text);
        } else if (!strcmp(func, "is_command")) {
            out = w19_event_eea6c7_is_command(text, allow);
        } else if (!strcmp(func, "get_command")) {
            out = w19_event_eea6c7_get_command(text, allow);
        } else if (!strcmp(func, "get_command_args")) {
            out = w19_event_eea6c7_get_command_args(text, allow);
        } else if (!strcmp(func, "sender_id")) {
            out = w19_event_eea6c7_sender_id(json_obj_get(in, "user_id"),
                                             json_obj_get(in, "source_user_id"));
        } else if (!strcmp(func, "absorb_reply_expected")) {
            out = w19_event_eea6c7_absorb_reply_expected(json_obj_get(in, "self_reply_expected"),
                                                        json_obj_get(in, "other_reply_expected"));
        } else if (!strcmp(func, "absorb_turn_input")) {
            out = w19_event_eea6c7_absorb_turn_input(
                json_obj_get(in, "self_user_id"), json_obj_get(in, "self_user_name"),
                json_obj_get(in, "self_source_user_id"), json_obj_get(in, "other_user_id"),
                json_obj_get(in, "other_user_name"), json_obj_get(in, "other_source_user_id"),
                json_obj_get(in, "self_reply_expected"), json_obj_get(in, "other_reply_expected"));
        } else if (!strcmp(func, "extend_media")) {
            out = w19_event_eea6c7_extend_media(
                json_obj_get(in, "self_urls"), json_obj_get(in, "self_types"),
                json_obj_get(in, "self_names"), json_obj_get(in, "other_urls"),
                json_obj_get(in, "other_types"), json_obj_get(in, "other_names"));
        } else if (!strcmp(func, "accept_interrupt")) {
            out = w19_event_eea6c7_accept_interrupt(json_obj_get(in, "events"));
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
        if (out) json_free(out);
        json_free(in);
    }
    return 0;
}
