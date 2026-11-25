#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_delegateto_5b49f0.h"

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
        if (!strcmp(func, "parse_timeout")) {
            out = w14_delegateto_5b49f0_parse_timeout(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "normalized_url")) {
            out = w14_delegateto_5b49f0_normalized_runtime_url(json_obj_get(in, "value"));
        } else if (!strcmp(func, "merge")) {
            out = w14_delegateto_5b49f0_merge_request_overrides(json_obj_get(in, "runtime"), json_obj_get(in, "explicit"));
        } else if (!strcmp(func, "credential_bundle")) {
            out = w14_delegateto_5b49f0_credential_bundle(json_obj_get(in, "model"), json_obj_get(in, "provider"),
                        json_obj_get(in, "base_url"), json_obj_get(in, "api_key"),
                        json_obj_get(in, "api_mode"), json_obj_get(in, "request_overrides"),
                        json_obj_get(in, "extra"));
        } else if (!strcmp(func, "inherit_caps")) {
            out = w14_delegateto_5b49f0_inherit_parent_capabilities(json_obj_get(in, "parent_agent"),
                        json_obj_get(in, "override_provider"), json_obj_get(in, "override_base_url"));
        } else if (!strcmp(func, "gateway_inline")) {
            out = json_bool(w14_delegateto_5b49f0_gateway_delegation_inline(json_obj_get(in, "user_config"),
                        json_get_str(in, "platform_key", ""),
                        json_get_bool(in, "interim_enabled", false) ? 1 : 0) ? true : false);
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
