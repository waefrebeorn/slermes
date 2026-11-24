/* JSON-lines harness for the w33_configfree_3f224a gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w33_configfree_3f224a.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* A fixture flag reads as JSON true only: the live seams are Python bools, so
 * "true"/1 must not be coerced the way json_get_bool does. */
static int flag(json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_BOOL && v->bool_val) ? 1 : 0;
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
        if (!strcmp(func, "status_payload")) {
            out = w33_configfree_3f224a_status_payload(json_obj_get(in, "identity"));
        } else if (!strcmp(func, "guest_state")) {
            out = json_number(w33_configfree_3f224a_guest_state(json_obj_get(in, "state")));
        } else if (!strcmp(func, "free_inference")) {
            out = json_number(w33_configfree_3f224a_free_inference(
                json_obj_get(in, "state"), flag(in, "guest_allowed")));
        } else if (!strcmp(func, "guest_without_tier")) {
            out = json_number(w33_configfree_3f224a_guest_without_tier(
                json_obj_get(in, "state"), flag(in, "guest_allowed")));
        } else if (!strcmp(func, "notice_pending")) {
            out = json_number(w33_configfree_3f224a_notice_pending(
                json_obj_get(in, "state"), flag(in, "guest_allowed")));
        } else if (!strcmp(func, "failure_copy")) {
            out = w33_configfree_3f224a_failure_copy(json_get_str(in, "code", NULL),
                                                     json_obj_get(in, "retry_after"));
        } else if (!strcmp(func, "mint_payload")) {
            out = w33_configfree_3f224a_mint_payload(json_obj_get(in, "failure"),
                                                     json_get_num(in, "now", 0));
        } else if (!strcmp(func, "challenge_parse")) {
            out = w33_configfree_3f224a_challenge_parse(json_obj_get(in, "payload"),
                                                        json_get_str(in, "portal", ""));
        } else if (!strcmp(func, "challenge_payload")) {
            out = w33_configfree_3f224a_challenge_payload(json_obj_get(in, "rec"),
                                                          json_get_num(in, "now", 0));
        } else if (!strcmp(func, "challenge_outcome")) {
            out = json_number(w33_configfree_3f224a_challenge_outcome(
                json_obj_get(in, "rec"), json_get_str(in, "url", NULL),
                (long)json_get_num(in, "attempt", 0), json_get_num(in, "now", 0)));
        } else if (!strcmp(func, "claim_blocker")) {
            out = w33_configfree_3f224a_claim_blocker(json_obj_get(in, "ask"));
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
