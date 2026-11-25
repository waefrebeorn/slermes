/* JSON-lines harness for the w29_oauth_dc3549_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_oauth_dc3549_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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
        if (!strcmp(func, "truncate_token")) {
            out = w29_oauth_dc3549_p1_truncate_token(json_obj_get(in, "value"),
                                                     json_get_num(in, "visible", 6));
        } else if (!strcmp(func, "epoch_ms_to_iso")) {
            out = w29_oauth_dc3549_p1_epoch_ms_to_iso(json_obj_get(in, "value"));
        } else if (!strcmp(func, "status_card")) {
            out = w29_oauth_dc3549_p1_status_card(in);
        } else if (!strcmp(func, "resolve_provider_status")) {
            out = w29_oauth_dc3549_p1_resolve_provider_status(in);
        } else if (!strcmp(func, "claude_disconnect")) {
            out = w29_oauth_dc3549_p1_claude_disconnect_command(json_get_str(in, "platform", "linux"));
        } else if (!strcmp(func, "disconnect_command")) {
            out = w29_oauth_dc3549_p1_provider_disconnect_command(json_obj_get(in, "provider"),
                                                                  json_get_str(in, "platform", "linux"));
        } else if (!strcmp(func, "disconnect_hint")) {
            out = w29_oauth_dc3549_p1_provider_disconnect_hint(json_obj_get(in, "provider"),
                                                               json_obj_get(in, "status"));
        } else if (!strcmp(func, "disconnect_http_error")) {
            out = w29_oauth_dc3549_p1_disconnect_http_error(json_obj_get(in, "status_code"),
                                                            json_get_str(in, "provider_name", ""));
        } else if (!strcmp(func, "reject_detail")) {
            out = w29_oauth_dc3549_p1_reject_detail(json_obj_get(in, "provider"),
                                                    json_obj_get(in, "status"));
        } else if (!strcmp(func, "profile_name")) {
            out = w29_oauth_dc3549_p1_profile_name(json_obj_get(in, "profile"));
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
