#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_configprov_34dd3f.h"

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
        if (!strcmp(func, "canonical_api_mode")) {
            out = w15_configprov_34dd3f_canonical_api_mode(json_get_str(in, "api_mode", ""));
        } else if (!strcmp(func, "coerce_provider_id")) {
            out = w15_configprov_34dd3f_coerce_provider_id(json_obj_get(in, "value"));
        } else if (!strcmp(func, "stringify_provider_map")) {
            out = w15_configprov_34dd3f_stringify_provider_map(json_obj_get(in, "providers"));
        } else if (!strcmp(func, "find_provider_entry")) {
            out = w15_configprov_34dd3f_find_provider_entry(json_obj_get(in, "providers"),
                                                            json_obj_get(in, "key"));
        } else if (!strcmp(func, "normalize_provider_models")) {
            out = w15_configprov_34dd3f_normalize_provider_models(json_obj_get(in, "models"));
        } else if (!strcmp(func, "coerce_ssl_verify")) {
            out = w15_configprov_34dd3f_coerce_ssl_verify(json_obj_get(in, "value"));
        } else if (!strcmp(func, "normalize_extra_headers")) {
            out = w15_configprov_34dd3f_normalize_extra_headers(json_obj_get(in, "extra_headers"));
        } else if (!strcmp(func, "is_provider_enabled")) {
            out = w15_configprov_34dd3f_is_provider_enabled(json_obj_get(in, "provider_cfg"));
        } else if (!strcmp(func, "route_model_cfg")) {
            out = w15_configprov_34dd3f_route_model_cfg(json_obj_get(in, "entry"),
                                                        json_get_str(in, "model", ""));
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
