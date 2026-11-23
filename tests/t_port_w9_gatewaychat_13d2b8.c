#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w9_gatewaychat_13d2b8.h"

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
        json_t *args = json_obj_get(in, "args");
        json_t *env = json_obj_get(in, "env");
        json_t *out = NULL;
        if (!strcmp(func, "continue_title")) {
            out = w9_gatewaychat_13d2b8_continue_title(args);
        } else if (!strcmp(func, "wants_latest")) {
            out = w9_gatewaychat_13d2b8_wants_latest(args);
        } else if (!strcmp(func, "env_flags")) {
            out = w9_gatewaychat_13d2b8_env_launch_flags(env);
        } else if (!strcmp(func, "bypass")) {
            out = w9_gatewaychat_13d2b8_bypass_launch(args, env);
        } else if (!strcmp(func, "launch_flags")) {
            out = w9_gatewaychat_13d2b8_launch_flags(args);
        } else if (!strcmp(func, "requested_policy")) {
            out = w9_gatewaychat_13d2b8_requested_policy(args, env);
        } else if (!strcmp(func, "validate")) {
            out = w9_gatewaychat_13d2b8_validate_options(args, env);
        } else if (!strcmp(func, "resume_error")) {
            out = w9_gatewaychat_13d2b8_resume_policy_error(json_obj_get(in, "keys"));
        } else if (!strcmp(func, "client_env_policy")) {
            out = w9_gatewaychat_13d2b8_client_env_policy(json_obj_get(in, "parameters"),
                        json_obj_get(in, "client_env"));
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
