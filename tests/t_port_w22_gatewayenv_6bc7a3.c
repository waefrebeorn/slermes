/* JSON-lines harness for the w22_gatewayenv_6bc7a3 gate.
 * One request object per stdin line; one response per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash.
 * Links against src/plugins/platforms/matrix/gateway_env.o + libjson. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_gatewayenv_6bc7a3.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "e2ee")) {
            out = w22_gatewayenv_6bc7a3_e2ee(json_obj_get(in, "env"));
        } else if (!strcmp(func, "authz")) {
            out = w22_gatewayenv_6bc7a3_authz();
        } else if (!strcmp(func, "enable_credentials")) {
            out = w22_gatewayenv_6bc7a3_enable_credentials();
        } else if (!strcmp(func, "env_overrides")) {
            out = w22_gatewayenv_6bc7a3_env_overrides();
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
