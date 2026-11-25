#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_mcpoauthp_5e630c.h"

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
        if (!strcmp(func, "metadata_issuer")) {
            out = w11_mcpoauthp_5e630c_metadata_issuer(json_obj_get(in, "context"));
        } else if (!strcmp(func, "asm_discovery_failure")) {
            out = w11_mcpoauthp_5e630c_asm_discovery_failure(json_obj_get(in, "response"));
        } else if (!strcmp(func, "with_discovery_context")) {
            out = w11_mcpoauthp_5e630c_with_discovery_context(json_get_str(in, "exc", ""),
                                                              json_obj_get(in, "failures"));
        } else if (!strcmp(func, "metadata_issued_by_origin")) {
            out = w11_mcpoauthp_5e630c_metadata_issued_by_origin(json_obj_get(in, "metadata"),
                    json_get_str(in, "auth_server_url", ""), json_obj_get(in, "response"));
        } else if (!strcmp(func, "google_offline_access_params")) {
            out = w11_mcpoauthp_5e630c_google_offline_access_params(json_obj_get(in, "context"));
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
