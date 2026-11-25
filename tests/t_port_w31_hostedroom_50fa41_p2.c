/* JSON-lines harness for the w31_hostedroom_50fa41_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w31_hostedroom_50fa41_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *dispatch(const char *func, const json_t *in) {
    if (!strcmp(func, "api_checks")) return w31_hostedroom_50fa41_p2_api_checks(in);
    if (!strcmp(func, "prepare")) return w31_hostedroom_50fa41_p2_prepare(in);
    if (!strcmp(func, "room_scope")) return w31_hostedroom_50fa41_p2_room_scope(in);
    if (!strcmp(func, "accepted")) return w31_hostedroom_50fa41_p2_accepted(in);
    if (!strcmp(func, "receipt_record")) return w31_hostedroom_50fa41_p2_receipt_record(in);
    if (!strcmp(func, "session_id_choice")) return w31_hostedroom_50fa41_p2_session_id_choice(in);
    if (!strcmp(func, "close_result")) return w31_hostedroom_50fa41_p2_close_result(in);
    if (!strcmp(func, "recover_decision")) return w31_hostedroom_50fa41_p2_recover_decision(in);
    if (!strcmp(func, "poll_cache")) return w31_hostedroom_50fa41_p2_poll_cache(in);
    if (!strcmp(func, "observation_receipt")) return w31_hostedroom_50fa41_p2_observation_receipt(in);
    if (!strcmp(func, "history_view")) return w31_hostedroom_50fa41_p2_history_view(in);
    if (!strcmp(func, "status_view")) return w31_hostedroom_50fa41_p2_status_view(in);
    if (!strcmp(func, "approve_decision")) return w31_hostedroom_50fa41_p2_approve_decision(in);
    if (!strcmp(func, "forget_decision")) return w31_hostedroom_50fa41_p2_forget_decision(in);
    if (!strcmp(func, "stop_decision")) return w31_hostedroom_50fa41_p2_stop_decision(in);
    if (!strcmp(func, "refresh_decision")) return w31_hostedroom_50fa41_p2_refresh_decision(in);
    if (!strcmp(func, "refreshed_grant")) return w31_hostedroom_50fa41_p2_refreshed_grant(in);
    return json_string("UNKNOWN_FUNC");
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
        out = dispatch(func, in);
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
