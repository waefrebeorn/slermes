/* JSON-lines harness for the w31_hostedroom_50fa41_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w31_hostedroom_50fa41_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *dispatch(const char *func, const json_t *in) {
    if (!strcmp(func, "valid_code")) return w31_hostedroom_50fa41_p1_valid_code(in);
    if (!strcmp(func, "response_error_code")) return w31_hostedroom_50fa41_p1_response_error_code(in);
    if (!strcmp(func, "run_path")) return w31_hostedroom_50fa41_p1_run_path(in);
    if (!strcmp(func, "run_status")) return w31_hostedroom_50fa41_p1_run_status(in);
    if (!strcmp(func, "next_poll_delay")) return w31_hostedroom_50fa41_p1_next_poll_delay(in);
    if (!strcmp(func, "require_room_grant")) return w31_hostedroom_50fa41_p1_require_room_grant(in);
    if (!strcmp(func, "request")) return w31_hostedroom_50fa41_p1_request(in);
    if (!strcmp(func, "admission_body")) return w31_hostedroom_50fa41_p1_admission_body(in);
    if (!strcmp(func, "invitation_body")) return w31_hostedroom_50fa41_p1_invitation_body(in);
    if (!strcmp(func, "digest_check")) return w31_hostedroom_50fa41_p1_digest_check(in);
    if (!strcmp(func, "pre_admission")) return w31_hostedroom_50fa41_p1_pre_admission(in);
    if (!strcmp(func, "http_error")) return w31_hostedroom_50fa41_p1_http_error(in);
    if (!strcmp(func, "budget_error")) return w31_hostedroom_50fa41_p1_budget_error(in);
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
