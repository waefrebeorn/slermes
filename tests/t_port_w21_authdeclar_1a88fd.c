/* JSON-lines harness for the w21_authdeclar_1a88fd gate.
 * One request object per stdin line; one response per line carrying "out" plus
 * every input key echoed back. Missing keys read as None and never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_authdeclar_1a88fd.h"
#include "hermes_cli/auth_declared_providers.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[131072];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "default_url")) {
            out = w21_authdeclar_1a88fd_default_api_key_base_url(
                json_get_str(in, "api_key", ""),
                json_get_str(in, "default_url", NULL),
                json_get_str(in, "env_url", ""));
        } else if (!strcmp(func, "origin")) {
            out = w21_authdeclar_1a88fd_origin(json_get_str(in, "base_url", ""));
        } else if (!strcmp(func, "relay")) {
            out = w21_authdeclar_1a88fd_relay_probe_api_key(
                json_get_str(in, "provider_id", ""),
                json_get_str(in, "relay", ""),
                json_get_str(in, "api_key", ""),
                json_get_str(in, "cred_base_url", ""),
                json_get_bool(in, "known_provider", 0) ? 1 : 0);
        } else if (!strcmp(func, "resolve")) {
            out = w21_authdeclar_1a88fd_resolve_declared(
                json_get_str(in, "provider_id", ""),
                json_obj_get(in, "config"),
                json_obj_get(in, "env"),
                json_obj_get(in, "key_cmd_values"));
        } else if (!strcmp(func, "custom")) {
            out = w21_authdeclar_1a88fd_custom_catalog(json_obj_get(in, "bundle"));
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
