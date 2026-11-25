#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_auxiliaryp_293888.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "configured_endpoint")) {
            const char *pbase = json_get_str(in, "profile_inference_base_url", "");
            if (json_has(in, "provider_in_registry") &&
                !json_get_bool(in, "provider_in_registry", 1)) pbase = "";
            out = w15_auxiliaryp_293888_configured_endpoint(
                json_get_str(in, "config_base_url", ""), pbase);
        } else if (!strcmp(func, "resolve")) {
            out = w15_auxiliaryp_293888_resolve(
                json_obj_get(in, "req"),
                json_get_str(in, "normalized_api_key", ""),
                json_get_bool(in, "pool_selected", 0) ? 1 : 0,
                json_get_str(in, "pool_runtime_key", ""),
                json_get_str(in, "pool_runtime_base_url", ""),
                json_get_str(in, "configured_endpoint", ""),
                json_get_bool(in, "profile_client_available", 0) ? 1 : 0,
                json_get_str(in, "model_candidate", ""),
                json_get_str(in, "normalized_model", ""));
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
