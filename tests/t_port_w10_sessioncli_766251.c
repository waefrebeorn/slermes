/* t_port_w10_sessioncli_766251.c — fixture-driven differential harness.
 * Reads one JSON case per stdin line, dispatches by "func", emits
 * {"func":..., "out":..., ...all input keys echoed...}. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_sessioncli_766251.h"

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
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "configured_names"))
            out = w10_sessioncli_766251_configured_names(in);
        else if (!strcmp(func, "client_env_rules"))
            out = w10_sessioncli_766251_client_env_rules(in);
        else if (!strcmp(func, "travels"))
            out = w10_sessioncli_766251_travels(in);
        else if (!strcmp(func, "normalise"))
            out = w10_sessioncli_766251_normalise(in);
        else if (!strcmp(func, "accept"))
            out = w10_sessioncli_766251_accept(in);
        else if (!strcmp(func, "retry_matches"))
            out = w10_sessioncli_766251_retry_matches(in);
        if (!out) out = json_null();
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func") || !strcmp(k, "out")) continue;
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
