/* JSON-lines harness for the w28_hostedroom_e01c0e_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_hostedroom_e01c0e_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); } else fputs("null", stdout);
    json_free(v);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "grant_revoke_is_terminal")) {
            out = hrs_grant_revoke_is_terminal(json_obj_get(in, "status_code"),
                                               json_get_str(in, "error_code", ""));
        } else if (!strcmp(func, "route_statuses")) {
            const char *room = json_has(in, "room_id") ? json_get_str(in, "room_id", NULL) : NULL;
            out = hrs_route_statuses(json_obj_get(in, "routes"), room);
        } else if (!strcmp(func, "live_status_sig")) {
            out = hrs_live_status_sig(in);
        } else if (!strcmp(func, "turn_timeout_seconds")) {
            out = hrs_turn_timeout_seconds(json_obj_get(in, "env"));
        } else if (!strcmp(func, "constants")) {
            out = hrs_constants();
        } else if (!strcmp(func, "authority")) {
            out = hrs_authority(json_obj_get(in, "room"));
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
        json_free(in);
    }
    return 0;
}
