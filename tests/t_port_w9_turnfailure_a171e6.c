#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_turnfailure_a171e6.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *args = json_obj_get(in, "in");
        json_t *out = NULL;
        if (!strcmp(func, "stamp")) {
            out = w9_turnfailure_a171e6_stamp_failure(json_obj_get(args, "result"),
                json_get_str(args, "reason", ""), json_get_bool(args, "retryable", false) ? 1 : 0);
        } else if (!strcmp(func, "display_kind")) {
            out = w9_turnfailure_a171e6_untyped_failed_turn_display_kind(
                json_get_str(args, "role", ""), json_obj_get(args, "content"));
        } else if (!strcmp(func, "notice")) {
            out = w9_turnfailure_a171e6_failed_turn_notice(json_obj_get(args, "turn_messages"));
        } else if (!strcmp(func, "exit_failure")) {
            out = w9_turnfailure_a171e6_exit_reason_failure(json_get_str(args, "reason", ""));
        } else if (!strcmp(func, "handoff")) {
            out = json_bool(w9_turnfailure_a171e6_is_max_iteration_handoff(
                json_obj_get(args, "result")) ? true : false);
        } else if (!strcmp(func, "gloss")) {
            out = w9_turnfailure_a171e6_failure_cause_gloss(
                json_get_str(args, "reason", ""),
                json_get_str(args, "subject", "it"),
                json_get_str(args, "possessive", "its"));
        } else if (!strcmp(func, "short_detail")) {
            out = w9_turnfailure_a171e6_short_detail(
                json_get_str(args, "text", ""), (int)json_get_num(args, "limit", 200));
        } else if (!strcmp(func, "invalid_reason")) {
            out = w9_turnfailure_a171e6_invalid_response_failure_reason(
                json_obj_get(args, "error"));
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
