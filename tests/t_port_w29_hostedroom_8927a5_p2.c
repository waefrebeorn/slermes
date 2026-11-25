/* JSON-lines harness for the w29_hostedroom_8927a5_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_hostedroom_8927a5_p2.h"

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
        if (!strcmp(func, "run_fence")) {
            out = w29_hostedroom_8927a5_p2_run_fence(json_obj_get(in, "lease"));
        } else if (!strcmp(func, "lease_from_row")) {
            out = w29_hostedroom_8927a5_p2_lease_from_row(json_obj_get(in, "row"),
                                                          json_get_bool(in, "reclaimed", false));
        } else if (!strcmp(func, "lease_row_matches")) {
            out = json_bool(w29_hostedroom_8927a5_p2_lease_row_matches(json_obj_get(in, "row"),
                                                                       json_obj_get(in, "lease")));
        } else if (!strcmp(func, "task_identity")) {
            out = w29_hostedroom_8927a5_p2_task_identity(json_obj_get(in, "row"));
        } else if (!strcmp(func, "load_task")) {
            out = w29_hostedroom_8927a5_p2_load_task(json_obj_get(in, "row"), json_obj_get(in, "identity"),
                                                     json_get_bool(in, "required", true));
        } else if (!strcmp(func, "active_room")) {
            out = w29_hostedroom_8927a5_p2_active_room(json_obj_get(in, "room"),
                                                       json_get_bool(in, "no_table", false));
        } else if (!strcmp(func, "room_authority")) {
            out = w29_hostedroom_8927a5_p2_room_authority(json_obj_get(in, "room"),
                                                          json_get_str(in, "gateway_id", ""),
                                                          (long long)json_get_num(in, "epoch", 0));
        } else if (!strcmp(func, "lease_gate")) {
            out = w29_hostedroom_8927a5_p2_lease_gate(json_obj_get(in, "room"), json_obj_get(in, "lease_row"),
                                                      json_obj_get(in, "lease"), json_get_num(in, "now", 0),
                                                      json_get_bool(in, "allow_expired", false));
        } else if (!strcmp(func, "generations_match")) {
            out = json_bool(w29_hostedroom_8927a5_p2_generations_match(
                json_obj_get(in, "row"), json_get_str(in, "status", ""),
                (long long)json_get_num(in, "execution_generation", 0),
                (long long)json_get_num(in, "cancel_generation", 0)));
        } else if (!strcmp(func, "cancel_generation_guard")) {
            out = w29_hostedroom_8927a5_p2_cancel_generation_guard(
                json_obj_get(in, "row"), (long long)json_get_num(in, "expected", 0));
        } else if (!strcmp(func, "acquire")) {
            out = w29_hostedroom_8927a5_p2_acquire(in);
        } else if (!strcmp(func, "renew")) {
            out = w29_hostedroom_8927a5_p2_renew(in);
        } else if (!strcmp(func, "release")) {
            out = w29_hostedroom_8927a5_p2_release(in);
        } else if (!strcmp(func, "recover")) {
            out = w29_hostedroom_8927a5_p2_recover(in);
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
