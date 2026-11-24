/* JSON-lines harness for the w31_localenvpo_3ff534 gate.
 *
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back.  The process environment is never
 * read: every name the policy would look up in os.environ arrives in the
 * request (`env`, `name`, `key`), so the answer is a pure function of the line.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w31_localenvpo_3ff534.h"

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
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "snake_field")) {
            out = w31_localenvpo_3ff534_snake_field(json_obj_get(in, "key"));
        } else if (!strcmp(func, "matches_terminal_first_party_prefix")) {
            out = w31_localenvpo_3ff534_matches_terminal_first_party_prefix(json_obj_get(in, "name"));
        } else if (!strcmp(func, "is_hermes_internal_secret")) {
            out = w31_localenvpo_3ff534_is_hermes_internal_secret(json_obj_get(in, "key"));
        } else if (!strcmp(func, "is_provider_env_blocklisted")) {
            out = w31_localenvpo_3ff534_is_provider_env_blocklisted(json_obj_get(in, "name"),
                                                                    json_obj_get(in, "extra"),
                                                                    json_obj_get(in, "registered"));
        } else if (!strcmp(func, "is_profile_gate_env")) {
            out = w31_localenvpo_3ff534_is_profile_gate_env(json_obj_get(in, "name"),
                                                            json_obj_get(in, "prefixes"));
        } else if (!strcmp(func, "carry_gateway_profile_pin")) {
            out = w31_localenvpo_3ff534_carry_gateway_profile_pin(json_obj_get(in, "env"));
        } else if (!strcmp(func, "strip_supervised_process_identity")) {
            out = w31_localenvpo_3ff534_strip_supervised_process_identity(json_obj_get(in, "env"));
        } else if (!strcmp(func, "strip_profile_gate_env")) {
            out = w31_localenvpo_3ff534_strip_profile_gate_env(json_obj_get(in, "env"),
                                                               json_obj_get(in, "prefixes"));
        } else if (!strcmp(func, "config_declared_key_envs")) {
            out = w31_localenvpo_3ff534_config_declared_key_envs(json_obj_get(in, "node"));
        } else if (!strcmp(func, "config_credential_env_refs")) {
            out = w31_localenvpo_3ff534_config_credential_env_refs(json_obj_get(in, "node"));
        } else if (!strcmp(func, "constants")) {
            out = w31_localenvpo_3ff534_constants();
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
