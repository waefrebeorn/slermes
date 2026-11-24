/* JSON-lines harness for the w29_adapter_1c52a4_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_adapter_1c52a4_p1.h"

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
        if (!strcmp(func, "allowed_host")) {
            out = json_bool(w29_adapter_1c52a4_p1_allowed_host(
                json_get_str(in, "url", NULL), json_get_bool(in, "check_port", 0)));
        } else if (!strcmp(func, "attachment_url")) {
            out = json_bool(w29_adapter_1c52a4_p1_attachment_url(json_get_str(in, "url", NULL)));
        } else if (!strcmp(func, "service_url")) {
            out = w29_adapter_1c52a4_p1_service_url(json_get_str(in, "raw", NULL));
        } else if (!strcmp(func, "conv_id_valid")) {
            out = json_bool(w29_adapter_1c52a4_p1_conv_id_valid(json_get_str(in, "chat_id", NULL)));
        } else if (!strcmp(func, "token_request")) {
            out = w29_adapter_1c52a4_p1_token_request(
                json_get_str(in, "tenant_id", NULL), json_get_str(in, "client_id", NULL),
                json_get_str(in, "client_secret", NULL));
        } else if (!strcmp(func, "require_mention")) {
            out = json_bool(w29_adapter_1c52a4_p1_require_mention(
                json_obj_get(in, "extra"), json_obj_get(in, "env")));
        } else if (!strcmp(func, "card_action_denied")) {
            out = w29_adapter_1c52a4_p1_card_action_denied(
                json_obj_get(in, "env"), json_get_str(in, "aad_object_id", NULL),
                json_get_str(in, "user_id", NULL), json_get_str(in, "deny_text", ""),
                json_get_str(in, "unauth_text", ""));
        } else if (!strcmp(func, "constants")) {
            out = w29_adapter_1c52a4_p1_constants();
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
