/* JSON-lines harness for the w22_gatewayenv_bc3450 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w22_gatewayenv_bc3450.h"
#include "plugins/platforms/whatsapp/gateway_env.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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
        if (!strcmp(func, "env_enable_credentials")) {
            out = w22_gatewayenv_bc3450_env_enable_credentials();
        } else if (!strcmp(func, "authz")) {
            out = w22_gatewayenv_bc3450_authz();
        } else if (!strcmp(func, "enabled_flag")) {
            out = w22_gatewayenv_bc3450_enabled_flag(json_get_str(in, "raw", ""),
                                                     json_get_bool(in, "has_platform", 0),
                                                     json_get_bool(in, "yaml_enabled", 0));
        } else if (!strcmp(func, "home_channel")) {
            out = w22_gatewayenv_bc3450_home_channel(json_get_str(in, "chat_id", ""),
                                                     json_get_str(in, "name", "Home"),
                                                     json_get_str(in, "thread_id", ""),
                                                     json_get_bool(in, "configured", 0));
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
