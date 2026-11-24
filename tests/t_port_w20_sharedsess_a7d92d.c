/* JSON-lines harness for the w20_sharedsess_a7d92d gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash.
 *
 * The port source is included by bare name (the repo's harness convention) so
 * the gate links against libjson alone. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_sharedsess_a7d92d.h"
#include "hermes_cli/shared_session_attach.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *str_or(json_t *in, const char *key, const char *def) {
    return json_get_str(in, key, def);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "local_origin")) {
            out = w20_sharedsess_a7d92d_local_origin(str_or(in, "url", ""),
                                                     str_or(in, "scheme", "http"));
        } else if (!strcmp(func, "request")) {
            out = w20_sharedsess_a7d92d_request(str_or(in, "session_id", ""),
                                                str_or(in, "nonce", ""),
                                                str_or(in, "profile_home", ""),
                                                str_or(in, "lease_id", ""));
        } else if (!strcmp(func, "verify")) {
            out = w20_sharedsess_a7d92d_verify(str_or(in, "session_id", ""),
                                               str_or(in, "nonce", ""),
                                               str_or(in, "profile_home", ""),
                                               str_or(in, "lease_id", ""),
                                               str_or(in, "endpoint", ""),
                                               json_obj_get(in, "reply"));
        } else if (!strcmp(func, "discover")) {
            out = w20_sharedsess_a7d92d_discover(str_or(in, "session_id", ""),
                                                 json_obj_get(in, "owners"),
                                                 str_or(in, "nonce", ""),
                                                 str_or(in, "profile_home", ""),
                                                 str_or(in, "owner_details", ""),
                                                 json_obj_get(in, "handshake"));
        } else if (!strcmp(func, "configure")) {
            out = w20_sharedsess_a7d92d_configure(json_obj_get(in, "env"),
                                                  str_or(in, "session_id", ""),
                                                  json_obj_get(in, "discovered_url"));
        } else if (!strcmp(func, "attach")) {
            out = w20_sharedsess_a7d92d_attach(json_obj_get(in, "env"),
                                               str_or(in, "session_id", ""),
                                               json_obj_get(in, "owners"),
                                               str_or(in, "nonce", ""),
                                               str_or(in, "profile_home", ""),
                                               str_or(in, "owner_details", ""),
                                               json_obj_get(in, "handshake"));
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
