/* t_port_w19_hostedroom_e90a2b.c - harness for src/tui_gateway/hosted_room_peer_transport.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_hostedroom_e90a2b.h"

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
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "route_identity")) {
            out = w19_hostedroom_e90a2b_route_identity(
                json_obj_get(in, "stored_rows"), json_get_str(in, "home_install_id", ""),
                json_get_str(in, "room_id", ""), json_get_str(in, "member_id", ""));
        } else if (!strcmp(func, "dispatch")) {
            out = w19_hostedroom_e90a2b_build_member_dispatch(
                json_obj_get(in, "binding"), json_obj_get(in, "route"), json_obj_get(in, "args"));
        } else if (!strcmp(func, "failover_order")) {
            out = w19_hostedroom_e90a2b_failover_order(in);
        } else if (!strcmp(func, "failover_call")) {
            out = w19_hostedroom_e90a2b_failover_call(in);
        } else if (!strcmp(func, "validate")) {
            out = w19_hostedroom_e90a2b_validate_coordinates(
                json_get_str(in, "profile", ""), json_get_str(in, "source", ""),
                json_obj_get(in, "title"), json_get_str(in, "route_profile", ""),
                json_get_str(in, "room_id", ""));
        } else if (!strcmp(func, "submit_guard")) {
            out = w19_hostedroom_e90a2b_submit_guard(
                json_get_str(in, "profile", ""), json_get_str(in, "source", ""),
                json_get_str(in, "route_profile", ""), json_get_str(in, "session_id", ""),
                json_obj_get(in, "member_id"), json_get_str(in, "route_member_id", ""),
                json_obj_get(in, "self_session_id"),
                json_get_bool(in, "attachments_present", false));
        } else if (!strcmp(func, "interrupt")) {
            out = w19_hostedroom_e90a2b_interrupt_decision(
                json_obj_get(in, "dispatch_task_id"), json_get_str(in, "expected_task_id", ""),
                json_obj_get(in, "task_id"), json_obj_get(in, "execution_generation"),
                json_get_bool(in, "has_stop_receipt", false));
        } else if (!strcmp(func, "seq")) {
            out = w19_hostedroom_e90a2b_validate_source_event_seq(json_obj_get(in, "source_event_seq"));
        } else if (!strcmp(func, "bind_args")) {
            out = w19_hostedroom_e90a2b_bind_scope_args(json_obj_get(in, "binding"),
                                                        json_obj_get(in, "route"));
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
