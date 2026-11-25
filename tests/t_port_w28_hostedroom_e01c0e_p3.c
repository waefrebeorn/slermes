/* JSON-lines harness for the w28_hostedroom_e01c0e_p3 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_hostedroom_e01c0e_p3.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); } else fputs("null", stdout);
    json_free(v);
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
        if (!strcmp(func, "activity_event")) {
            out = hrs_activity_event(json_obj_get(in, "room"), json_obj_get(in, "decision"));
        } else if (!strcmp(func, "continue_event")) {
            out = hrs_continue_event(json_get_str(in, "room_id", ""),
                                     json_get_str(in, "discussion_event_id", ""),
                                     json_get_str(in, "gateway_id", ""),
                                     json_get_num(in, "epoch", 0.0),
                                     json_obj_get(in, "paused"),
                                     json_get_num(in, "max_continues", 5.0),
                                     json_obj_get(in, "event_seqs"));
        } else if (!strcmp(func, "stop_room_tally")) {
            out = hrs_stop_room_tally(json_obj_get(in, "fixture"));
        } else if (!strcmp(func, "retry_room_task")) {
            out = hrs_retry_room_task(json_obj_get(in, "fixture"));
        } else if (!strcmp(func, "owed_revokes")) {
            out = hrs_owed_revokes(json_obj_get(in, "fixture"));
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
        json_free(in);
    }
    return 0;
}
