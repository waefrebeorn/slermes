#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_anonauth_c583c5.h"

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
        json_t *out = NULL;
        if (!strcmp(func, "friendly_wait")) {
            out = w7_anonauth_c583c5_friendly_wait(json_get_str(in, "seconds", ""));
        } else if (!strcmp(func, "is_guest")) {
            out = json_bool(w7_anonauth_c583c5_is_guest_state(json_obj_get(in, "state")) ? true : false);
        } else if (!strcmp(func, "parse_refusal")) {
            out = w7_anonauth_c583c5_parse_welcome_refusal(json_obj_get(in, "body"));
        } else if (!strcmp(func, "failure_copy")) {
            out = w7_anonauth_c583c5_anon_failure_copy(
                json_get_str(in, "code", ""), json_get_str(in, "retry_after", ""));
        } else if (!strcmp(func, "welcome_host")) {
            out = json_bool(w7_anonauth_c583c5_route_is_welcome_host(
                json_get_str(in, "base_url", ""), json_get_str(in, "extra_hosts", "")) ? true : false);
        } else if (!strcmp(func, "pin_model")) {
            out = w7_anonauth_c583c5_pin_model_for_route(
                json_get_str(in, "provider", ""), json_get_str(in, "base_url", ""),
                json_get_str(in, "model", ""), json_get_str(in, "extra_hosts", ""));
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
