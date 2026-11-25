/* JSON-lines harness for the w26_credential_0a5825 gate
 * (plugins/config_backends/remote/credentials.py).  One request object per
 * stdin line; one response line carrying the computed "out" plus every input
 * key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_credential_0a5825.h"

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
        if (!strcmp(func, "idp_env")) {
            out = w26_credential_0a5825_idp_env(json_obj_get(in, "env"));
        } else if (!strcmp(func, "credential_kind")) {
            out = w26_credential_0a5825_credential_kind(json_obj_get(in, "env"));
        } else if (!strcmp(func, "idp_form")) {
            out = w26_credential_0a5825_idp_form(json_obj_get(in, "env"));
        } else if (!strcmp(func, "cache_ttl_offset")) {
            out = w26_credential_0a5825_cache_ttl_offset(json_obj_get(in, "expires_in"));
        } else if (!strcmp(func, "constants")) {
            out = w26_credential_0a5825_constants();
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
