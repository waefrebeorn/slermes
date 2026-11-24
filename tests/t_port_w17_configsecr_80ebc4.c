#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_configsecr_80ebc4.h"

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
        json_t *a = json_obj_get(in, "in");
        json_t *out = NULL;
        if (!strcmp(func, "env_slug"))
            out = w17_configsecr_80ebc4_env_slug(json_get_str(a, "text", ""));
        else if (!strcmp(func, "model_api_key_env"))
            out = w17_configsecr_80ebc4_model_api_key_env();
        else if (!strcmp(func, "aux_api_key_env"))
            out = w17_configsecr_80ebc4_aux_api_key_env(json_get_str(a, "task", ""));
        else if (!strcmp(func, "digest"))
            out = w17_configsecr_80ebc4_digest(json_get_str(a, "text", ""));
        else if (!strcmp(func, "provider_name_key_env"))
            out = w17_configsecr_80ebc4_provider_name_key_env(json_get_str(a, "name", ""));
        else if (!strcmp(func, "is_env_reference"))
            out = w17_configsecr_80ebc4_is_env_reference(json_get_str(a, "value", ""));
        else if (!strcmp(func, "canonical_endpoint"))
            out = w17_configsecr_80ebc4_canonical_endpoint(json_get_str(a, "base_url", ""));
        else if (!strcmp(func, "endpoint_identity"))
            out = w17_configsecr_80ebc4_endpoint_identity(json_get_str(a, "base_url", ""));
        else if (!strcmp(func, "endpoint_api_key_env"))
            out = w17_configsecr_80ebc4_endpoint_api_key_env(json_get_str(a, "base_url", ""));
        else if (!strcmp(func, "dashboard_endpoint_key_env"))
            out = w17_configsecr_80ebc4_dashboard_endpoint_key_env(json_obj_get(a, "entry"),
                      json_get_str(a, "endpoint_id", ""));
        else if (!strcmp(func, "block_base_url"))
            out = w17_configsecr_80ebc4_block_base_url(json_obj_get(a, "block"));
        else out = json_string("UNKNOWN_FUNC");
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
