/* JSON-lines harness for the w24_provider_4aa510 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_provider_4aa510.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "headers")) {
            out = w24_provider_4aa510_headers(json_obj_get(in, "api_key"));
        } else if (!strcmp(func, "search_payload")) {
            out = w24_provider_4aa510_search_payload(json_obj_get(in, "query"),
                                                     json_obj_get(in, "limit"));
        } else if (!strcmp(func, "normalize_search")) {
            out = w24_provider_4aa510_normalize_search(json_obj_get(in, "response"));
        } else if (!strcmp(func, "normalize_documents")) {
            out = w24_provider_4aa510_normalize_documents(json_obj_get(in, "response"));
        } else if (!strcmp(func, "missing_key_error")) {
            out = w24_provider_4aa510_missing_key_error(json_obj_get(in, "action"));
        } else if (!strcmp(func, "auth")) {
            out = w24_provider_4aa510_auth(json_obj_get(in, "api_key"),
                                           json_obj_get(in, "keyless"),
                                           json_obj_get(in, "action"));
        } else if (!strcmp(func, "setup_schema")) {
            out = w24_provider_4aa510_setup_schema();
        } else if (!strcmp(func, "constants")) {
            out = w24_provider_4aa510_constants();
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
